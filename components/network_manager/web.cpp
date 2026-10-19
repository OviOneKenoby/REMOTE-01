#include "network_manager/api.hpp"
#include "internal.hpp"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
namespace network_manager {
namespace {
httpd_handle_t server = nullptr;
std::atomic<unsigned> stack_min{0};
std::atomic<bool> running{false};
std::atomic<unsigned> accepted{0}, mapped{0}, rejected{0}, requests{0}, redirects{0}, errors{0}, last_reject{0};
std::atomic<unsigned> android{0}, apple{0}, windows{0};
void margin() {
  auto n=uxTaskGetStackHighWaterMark(nullptr), previous=stack_min.load();
  if(!previous || n<previous) stack_min.store(n);
}
void reject(unsigned reason) {
  last_reject.store(reason); ++errors;
  if (++rejected<=4) ESP_LOGW("NETWORK_HTTP", "rejected=1 reason=%u no_secret_logs=1",reason);
}
bool decode_address(const sockaddr_storage &s, socklen_t n, portal::IPv4 &out, bool &v6) {
  v6=s.ss_family==AF_INET6;
  if (s.ss_family==AF_INET && n>=sizeof(sockaddr_in)) {
    auto &a=reinterpret_cast<const sockaddr_in &>(s);
    return portal::normalize(false,{reinterpret_cast<const unsigned char *>(&a.sin_addr.s_addr),4},out);
  }
  if (v6 && n>=sizeof(sockaddr_in6)) {
    auto &a=reinterpret_cast<const sockaddr_in6 &>(s);
    return portal::normalize(true,{a.sin6_addr.s6_addr,16},out);
  }
  return false;
}
unsigned portal_socket(int fd, bool &was_mapped) {
  // HTTPD is dual-stack in this exact SDK configuration. Full buffers and
  // normalization preserve IPv4-mapped IPv6, while real IPv6 stays rejected.
  sockaddr_storage local{}, peer{}; socklen_t ln=sizeof local, pn=sizeof peer;
  if(getsockname(fd,reinterpret_cast<sockaddr *>(&local),&ln) ||
     getpeername(fd,reinterpret_cast<sockaddr *>(&peer),&pn)) return 1;
  portal::IPv4 lip{}, pip{}, sip{}; bool l6=false,p6=false;
  if(!decode_address(local,ln,lip,l6) || !decode_address(peer,pn,pip,p6)) return 2;
  was_mapped=l6||p6;
  auto *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if(!ap || !esp_netif_is_netif_up(ap)) return 3;
  auto *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!sta) return 3;
  bool up=esp_netif_is_netif_up(sta);
  if (up) {
    esp_netif_ip_info_t ip{};
    if(esp_netif_get_ip_info(sta,&ip)!=ESP_OK || !portal::normalize(false,{reinterpret_cast<const unsigned char *>(&ip.ip.addr),4},sip)) return 3;
  }
  return portal::socket_allowed(lip,pip,up,sip)?0:4;
}
esp_err_t open_socket(httpd_handle_t, int fd) {
  margin(); bool v6=false; auto reason=portal_socket(fd,v6);
  if(reason) { reject(reason); return ESP_FAIL; }
  if(v6) ++mapped;
  if(++accepted<=3) ESP_LOGI("NETWORK_HTTP", "accepted=1 mapped_v4=%u",v6);
  return ESP_OK;
}
portal::Action allowed(httpd_req_t *r, WebView &v, bool mutation) {
  margin(); ++requests;
  char host[260]{}; bool v6=false;
  if (!web_view(v) || !v.status.portal ||
      portal_socket(httpd_req_to_sockfd(r),v6)) { reject(5); return portal::Action::DENY; }
  if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof host)!=ESP_OK || !portal::host_valid(host)) {
    reject(6); return portal::Action::DENY;
  }
  bool token_ok=false, origin_ok=false;
  if (mutation) {
    char token[40]{}, origin[40]{};
    token_ok=httpd_req_get_hdr_value_str(r,"X-Setup-Token",token,sizeof token)==ESP_OK && !std::strcmp(token,v.token);
    origin_ok=!httpd_req_get_hdr_value_len(r,"Origin") ||
      (httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof origin)==ESP_OK && !std::strcmp(origin,"http://192.168.4.1"));
  }
  std::string_view path=r->uri; auto query=path.find('?');
  if(mutation && query!=path.npos) { reject(7); return portal::Action::DENY; }
  path=path.substr(0,query);
  auto action=portal::request(true,true,host,r->method==HTTP_GET,path,token_ok,origin_ok);
  if(action==portal::Action::DENY) { reject(7); return action; }
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
  httpd_resp_set_hdr(r, "Referrer-Policy", "no-referrer");
  return action;
}
esp_err_t refuse(httpd_req_t *r, const char *status, const char *body) {
  httpd_resp_set_status(r, status);
  if (status[0] != '2') {
    if(status[0]=='4') { last_reject.store(8); ++errors; ++rejected; }
    // Do not let the SDK purge an unbounded rejected request body.
    httpd_resp_set_hdr(r, "Connection", "close"); httpd_resp_sendstr(r, body); return ESP_FAIL;
  }
  return httpd_resp_sendstr(r, body);
}
esp_err_t redirect(httpd_req_t *r) {
  if(r->content_len) { reject(8); return refuse(r,"400 Bad Request","Invalid input"); }
  ++redirects; httpd_resp_set_status(r,"302 Found");
  std::string_view path=r->uri; path=path.substr(0,path.find('?'));
  switch(portal::probe(path)) {
    case portal::Probe::ANDROID: ++android; break;
    case portal::Probe::APPLE: ++apple; break;
    case portal::Probe::WINDOWS: ++windows; break;
    default: break;
  }
  httpd_resp_set_hdr(r,"Location","http://192.168.4.1/");
  httpd_resp_set_type(r,"text/html; charset=utf-8");
  // A nonempty body also supports Apple's captive network assistant.
  auto result=httpd_resp_sendstr(r,"<!doctype html><a href=\"http://192.168.4.1/\">REMOTE Wi-Fi setup</a>");
  if(result!=ESP_OK) ++errors;
  return result;
}
esp_err_t captive(httpd_req_t *r, httpd_err_code_t) {
  WebView v{};
  if(allowed(r,v,false)!=portal::Action::REDIRECT) return refuse(r,"403 Forbidden","Local setup only");
  return redirect(r);
}
esp_err_t captive_get(httpd_req_t *r) { return captive(r,HTTPD_404_NOT_FOUND); }
esp_err_t protocol_error(httpd_req_t *r, httpd_err_code_t code) {
  reject(100+unsigned(code));
  return refuse(r,"400 Bad Request","Invalid HTTP request");
}
constexpr char page[] = R"HTML(<!doctype html><html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>REMOTE Wi-Fi setup</title>
<h1>REMOTE//01 Wi-Fi setup</h1><p>Local setup only. Keep this connection until success. Stored passwords are never displayed.</p>
<button id="scan">Scan nearby networks</button><select id="net"><option value="">Select or enter manually</option></select>
<form id="form"><p><label>SSID (also hidden networks) <input id="ssid" maxlength="32" required autocomplete="off"></label></p>
<p><label>Security <select id="security"><option value="2">WPA2 Personal / WPA2-WPA3 mixed</option><option value="3">WPA3 Personal</option></select></label></p>
<p><label>Password <input id="password" type="password" maxlength="64" required autocomplete="off"></label></p>
<button>Connect and save</button></form><p id="status" role="status">Ready</p>
<script nonce="TOKEN">const token='TOKEN';const $=id=>document.getElementById(id);let sent=false,last='',failures=0;
async function post(path,body){return fetch(path,{method:'POST',headers:{'X-Setup-Token':token,'Content-Type':'application/x-www-form-urlencoded'},body});}
$('scan').onclick=async()=>{try{let r=await post('/scan','');$('status').textContent=r.ok?'Scanning…':'Scan busy; retry shortly';}catch(e){$('status').textContent='Setup connection unavailable';}};
$('net').onchange=()=>{const o=$('net').selectedOptions[0];if(o.value){$('ssid').value=o.value;$('security').value=o.dataset.security;}};
$('form').onsubmit=async e=>{e.preventDefault();const body=new URLSearchParams({ssid:$('ssid').value,password:$('password').value,security:$('security').value});try{const r=await post('/credentials',body);$('password').value='';sent=r.ok;$('status').textContent=r.ok?'Connecting; please wait…':'Rejected or busy; check input and retry';}catch(e){$('password').value='';$('status').textContent='Setup connection unavailable';}};
async function poll(){try{let r=await fetch('/networks',{cache:'no-store'});if(!r.ok)return;let v=await r.json();failures=0;if(v.state==='CONNECTED'){$('status').textContent='Success: saved. Setup AP closes shortly. Rejoin your home network.';return;}if(sent&&v.state==='PROVISIONING'){$('status').textContent='Connection failed; credentials were not saved. Retry.';sent=false;}const select=$('net');const signature=JSON.stringify(v.networks);if(!v.scanning&&signature!==last){last=signature;select.replaceChildren(new Option('Select or enter manually',''));for(const n of v.networks){let o=new Option((n.ssid||'[hidden]')+' ('+n.rssi+' dBm)'+(n.security?'':' [unsupported]'),n.ssid);o.disabled=!n.security||!n.ssid;o.dataset.security=n.security;select.add(o);}}}catch(e){if(++failures>=5){$('status').textContent='Setup connection closed. Check REMOTE status and rejoin the home network if connected.';return;}}setTimeout(poll,1000);}poll();</script></html>)HTML";
esp_err_t home(httpd_req_t *r) {
  if (r->content_len) return refuse(r, "400 Bad Request", "Invalid input");
  WebView v{}; auto action=allowed(r,v,false);
  if(action==portal::Action::REDIRECT) return redirect(r);
  if(action!=portal::Action::PAGE) return refuse(r,"403 Forbidden","Local setup only");
  char csp[256]{};
  std::snprintf(csp, sizeof csp, "default-src 'none'; script-src 'nonce-%s'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'", v.token);
  httpd_resp_set_hdr(r, "Content-Security-Policy", csp); httpd_resp_set_type(r, "text/html; charset=utf-8");
  const char *p = page;
  while (auto *match = std::strstr(p, "TOKEN")) {
    if (httpd_resp_send_chunk(r, p, match-p) != ESP_OK || httpd_resp_send_chunk(r, v.token, 32) != ESP_OK) return ESP_FAIL;
    p = match + 5;
  }
  if (httpd_resp_sendstr_chunk(r, p) != ESP_OK) return ESP_FAIL;
  return httpd_resp_send_chunk(r, nullptr, 0);
}
esp_err_t credentials(httpd_req_t *r) {
  WebView v{}; if (allowed(r,v,true)!=portal::Action::API) return refuse(r, "403 Forbidden", "Local setup only");
  char type[64]{};
  if (r->content_len < 1 || r->content_len > 512 ||
      httpd_req_get_hdr_value_str(r, "Content-Type", type, sizeof type) != ESP_OK ||
      std::strcmp(type, "application/x-www-form-urlencoded")) return refuse(r, "400 Bad Request", "Invalid input");
  char body[513]{}; std::size_t used = 0; bool ok = true;
  const auto deadline = esp_timer_get_time() + 5000000;
  while (used < r->content_len && esp_timer_get_time() < deadline) {
    int got = httpd_req_recv(r, body + used, r->content_len - used);
    if (got <= 0) { ok = false; break; } used += got;
  }
  Credentials c{}; ok = ok && used == r->content_len && parse(std::string_view(body, used), c);
  wipe(body, sizeof body);
  bool queued = ok && submit(c, v.status.generation); wipe(&c, sizeof c);
  if (!queued) return refuse(r, ok ? "409 Conflict" : "400 Bad Request", ok ? "Busy; retry" : "Invalid input");
  return refuse(r, "202 Accepted", "Connecting; check setup status");
}
esp_err_t scan_request(httpd_req_t *r) {
  WebView v{}; if (allowed(r,v,true)!=portal::Action::API) return refuse(r, "403 Forbidden", "Local setup only");
  if (r->content_len || !scan(v.status.generation)) return refuse(r, "409 Conflict", "Busy; retry");
  return refuse(r, "202 Accepted", "Scanning");
}
esp_err_t networks(httpd_req_t *r) {
  if (r->content_len) return refuse(r, "400 Bad Request", "Invalid input");
  WebView v{}; if (allowed(r,v,false)!=portal::Action::API) return refuse(r, "403 Forbidden", "Local setup only");
  cJSON *root = cJSON_CreateObject(), *list = cJSON_CreateArray();
  if (!root || !list) { fault(true); cJSON_Delete(root); cJSON_Delete(list); return refuse(r, "503 Service Unavailable", "Retry"); }
  bool ok = cJSON_AddStringToObject(root, "state", name(v.status.state)) &&
    cJSON_AddBoolToObject(root, "scanning", v.status.scanning) && cJSON_AddItemToObject(root, "networks", list);
  // Bound all allocations by the 16-entry scan snapshot. Never serialize Credentials/LocalSetup.
  for (unsigned i = 0; ok && i < v.status.scan_count && i < 16; ++i) {
    auto *o = cJSON_CreateObject(); if (!o) { ok = false; break; }
    ok = cJSON_AddStringToObject(o, "ssid", v.networks[i].ssid) &&
      cJSON_AddNumberToObject(o, "rssi", v.networks[i].rssi) && cJSON_AddNumberToObject(o, "security", v.networks[i].security);
    if (!ok || !cJSON_AddItemToArray(list, o)) { cJSON_Delete(o); ok = false; }
  }
  char *out = ok ? cJSON_PrintUnformatted(root) : nullptr;
  // list is owned by root after the successful insertion.
  if (!cJSON_GetObjectItem(root, "networks")) cJSON_Delete(list);
  cJSON_Delete(root);
  if (!out) { fault(true); return refuse(r, "503 Service Unavailable", "Retry"); }
  httpd_resp_set_type(r, "application/json"); auto result = httpd_resp_sendstr(r, out); cJSON_free(out); return result;
}
}
bool web_start() {
  if (server) return true;
  httpd_config_t c = HTTPD_DEFAULT_CONFIG(); c.stack_size = 6144; c.max_open_sockets = 2;
  c.task_priority = 1; c.open_fn = open_socket; c.max_req_hdr_len = 1024; c.max_uri_len = 64;
  c.max_uri_handlers = 5; c.uri_match_fn=httpd_uri_match_wildcard;
  c.recv_wait_timeout = 3; c.send_wait_timeout = 3; c.lru_purge_enable = true;
  auto started=httpd_start(&server,&c);
  if(started!=ESP_OK) { ++errors; server=nullptr; ESP_LOGE("NETWORK_HTTP","start=FAILED code=%d",started); return false; }
  running.store(true);
  auto route = [](const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *)) {
    httpd_uri_t r{}; r.uri = uri; r.method = method; r.handler = handler;
    return httpd_register_uri_handler(server, &r) == ESP_OK;
  };
  if (!route("/", HTTP_GET, home) || !route("/credentials", HTTP_POST, credentials) ||
      !route("/scan", HTTP_POST, scan_request) || !route("/networks", HTTP_GET, networks) ||
      !route("/*",HTTP_GET,captive_get) ||
      httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,captive)!=ESP_OK ||
      httpd_register_err_handler(server,HTTPD_405_METHOD_NOT_ALLOWED,captive)!=ESP_OK) {
    ++errors;
    web_stop(); return false;
  }
  for(auto code:{HTTPD_400_BAD_REQUEST,HTTPD_408_REQ_TIMEOUT,HTTPD_413_CONTENT_TOO_LARGE,HTTPD_414_URI_TOO_LONG,HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,HTTPD_500_INTERNAL_SERVER_ERROR}) {
    if(httpd_register_err_handler(server,code,protocol_error)!=ESP_OK) { ++errors; web_stop(); return false; }
  }
  ESP_LOGI("NETWORK_HTTP","running=1 port=80 dual_stack=1 captive_redirects=1"); return true;
}
void web_stop() { if (server) { if(httpd_stop(server)!=ESP_OK) { ++errors; return; } server=nullptr; running.store(false); } }
unsigned web_stack_min() { return stack_min.load(); }
portal::Stats web_stats() {
  portal::Stats s{}; s.http=running.load(); s.accepted=accepted.load(); s.mapped=mapped.load(); s.rejected=rejected.load();
  s.requests=requests.load(); s.redirects=redirects.load(); s.http_errors=errors.load(); s.last_reject=last_reject.load();
  s.android=android.load(); s.apple=apple.load(); s.windows=windows.load(); return s;
}
}

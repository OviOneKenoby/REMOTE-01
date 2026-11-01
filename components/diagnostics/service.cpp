#include "diagnostics/api.hpp"
#include "ota_service/api.hpp"
#include "ota_service/server.hpp"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <atomic>
#include <cstring>
#include <new>
namespace diagnostics {
namespace {
struct Storage { Snapshots copies; Ring ring; RtcpCloseHistory close_history; char response[response_bytes]{}; };
static_assert(sizeof(Storage)<=8192);
struct Control {
  std::atomic<Storage*> storage{nullptr}; std::atomic_flag lock=ATOMIC_FLAG_INIT;
  std::atomic<bool> enabled{false}; std::atomic<unsigned> failures{0}, drops{0}, margin{0};
  httpd_handle_t server=nullptr; Address bound{}; U64 retry=0;
} ctl;
static_assert(sizeof(Control)<=512);
U64 time_now() { return esp_timer_get_time()/1000; }
bool take() { return !ctl.lock.test_and_set(std::memory_order_acquire); }
void give() { ctl.lock.clear(std::memory_order_release); }
void margin() { auto n=uxTaskGetStackHighWaterMark(nullptr);auto old=ctl.margin.load();if(!old||n<old)ctl.margin=n; }
Address address() { Address a{};if(take()) {a=ctl.bound;give();}return a; }
bool decode(const sockaddr_storage &a,socklen_t n,std::uint32_t &out) {
  if(a.ss_family==AF_INET && n>=sizeof(sockaddr_in))return normalize(false,reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in&>(a).sin_addr.s_addr),4,out);
  if(a.ss_family==AF_INET6 && n>=sizeof(sockaddr_in6))return normalize(true,reinterpret_cast<const sockaddr_in6&>(a).sin6_addr.s6_addr,16,out);
  return false;
}
bool allowed_socket(int fd) {
  if(!ctl.enabled.load())return false;
  auto a=address();auto *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t info{};wifi_mode_t mode{};wifi_ap_record_t ap{};
  if(!sta||!esp_netif_is_netif_up(sta)||esp_netif_get_ip_info(sta,&info)!=ESP_OK||
     esp_wifi_get_mode(&mode)!=ESP_OK||mode!=WIFI_MODE_STA||esp_wifi_sta_get_ap_info(&ap)!=ESP_OK||
     ntohl(info.ip.addr)!=a.ip||ntohl(info.netmask.addr)!=a.mask)return false;
  sockaddr_storage local{},peer{};socklen_t ln=sizeof local,pn=sizeof peer;std::uint32_t l=0,p=0;
  return !getsockname(fd,reinterpret_cast<sockaddr*>(&local),&ln)&&!getpeername(fd,reinterpret_cast<sockaddr*>(&peer),&pn)&&decode(local,ln,l)&&decode(peer,pn,p)&&socket_allowed(a,l,p)&&ctl.enabled.load();
}
esp_err_t open_socket(httpd_handle_t,int fd) {margin();if(allowed_socket(fd))return ESP_OK;++ctl.failures;return ESP_FAIL;}
esp_err_t refuse(httpd_req_t *r,const char *status,const char *message="Diagnostics unavailable") {++ctl.failures;httpd_resp_set_status(r,status);httpd_resp_set_hdr(r,"Connection","close");httpd_resp_sendstr(r,message);return ESP_FAIL;}
bool request_allowed(httpd_req_t*r) {
  margin();char host[32]{};
  return route(r->uri,r->method==HTTP_GET,r->content_len)!=Route::DENY&&allowed_socket(httpd_req_to_sockfd(r))&&
    httpd_req_get_hdr_value_str(r,"Host",host,sizeof host)==ESP_OK&&host_allowed(host,address().ip);
}
void headers(httpd_req_t*r,const char *type) {
  httpd_resp_set_type(r,type);httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");httpd_resp_set_hdr(r,"X-Frame-Options","DENY");httpd_resp_set_hdr(r,"Referrer-Policy","no-referrer");
  httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
}
constexpr char page[]=R"HTML(<!doctype html><meta name="viewport" content="width=device-width"><title>REMOTE diagnostics</title><style>body{background:#11191d;color:#f3f5f2;font:14px sans-serif;margin:16px}pre{white-space:pre-wrap;overflow-wrap:anywhere}h2{color:#c2ff00}</style><h2>REMOTE diagnostics</h2><p>Read-only LAN access. Refreshes every 5 seconds. No commands.</p><p>State: network 0 DISABLED / 1 PROVISIONING / 2 CONNECTING / 3 CONNECTED / 4 RETRY_WAIT. Power 0 ACTIVE / 1 DISPLAY_IDLE / 2 SYSTEM_SLEEP / 3 SHUTDOWN. Battery 0 UNKNOWN / 1 NORMAL / 2 LOW / 3 CRITICAL.</p><pre id="status">Waiting</pre><h2>Selected events</h2><p>0 boot; 1 network; 2 network error; 3 power; 4 power error; 5 Core; 6 refused; 7 reboot request; 8 reboot refused; 9 reboot execution. Rows: sequence, uptime_ms, event, detail.</p><pre id="events"></pre><p>Reset rows: available, boot, reason, wake, runtime, state, condition, valid, mV, filtered mV, hold, event, detail, uptime_ms, sleep requests, sleep rounds. Data older than 3 seconds may be stale.</p><script>let busy=false,after=0;async function poll(){if(busy||document.hidden)return;busy=true;try{const s=await fetch('/api/status',{cache:'no-store'});if(!s.ok)throw Error('status '+s.status);document.getElementById('status').textContent=JSON.stringify(await s.json(),null,2);const e=await fetch('/api/events?after='+after,{cache:'no-store'});if(!e.ok){if(e.status===400)after=0;throw Error('events '+e.status)}const d=await e.json();document.getElementById('events').textContent=JSON.stringify(d,null,2);after=d.after;}catch(e){document.getElementById('status').textContent='Unavailable: '+e.message}finally{busy=false}}poll();setInterval(poll,5000);document.addEventListener('visibilitychange',poll);</script>)HTML";
esp_err_t home(httpd_req_t*r) {if(!request_allowed(r)||std::strcmp(r->uri,"/"))return refuse(r,"403 Forbidden");headers(r,"text/html");return httpd_resp_send(r,page,sizeof page-1);}
esp_err_t status(httpd_req_t*r) {
  if(!request_allowed(r)||std::strcmp(r->uri,"/api/status"))return refuse(r,"403 Forbidden");
  auto*s=ctl.storage.load();Snapshots copy{};if(!s)return refuse(r,"503 Service Unavailable","Diagnostics storage unavailable");if(!take())return refuse(r,"503 Service Unavailable","Diagnostics busy");copy=s->copies;give();
  const auto*app=esp_app_get_description();char sha[65]{};constexpr char hex[]="0123456789abcdef";
  for(unsigned i=0;i<32;++i) {sha[2*i]=hex[app->app_elf_sha256[i]>>4];sha[2*i+1]=hex[app->app_elf_sha256[i]&15];}
  if(!status_json(copy,time_now(),app->project_name,app->version,sha,esp_reset_reason(),s->response,sizeof s->response))return refuse(r,"503 Service Unavailable","Diagnostics serialization overflow");
  headers(r,"application/json");return httpd_resp_sendstr(r,s->response);
}
esp_err_t events(httpd_req_t*r) {
  if(!request_allowed(r))return refuse(r,"403 Forbidden");
  std::string_view uri=r->uri;constexpr std::string_view prefix="/api/events?after=";U64 after=0;
  if(!uri.starts_with(prefix)||!cursor(uri.substr(prefix.size()),after))return refuse(r,"400 Bad Request");
  auto*s=ctl.storage.load();if(!s)return refuse(r,"503 Service Unavailable");
  // One copied entry at a time keeps HTTP stack bounded; no formatting under lock.
  U64 oldest=0,next=0,overflow=0;if(!take())return refuse(r,"503 Service Unavailable");oldest=s->ring.oldest();next=s->ring.next;overflow=s->ring.overflow;give();
  if(after>=next && after) return refuse(r,"400 Bad Request");
  Json j{s->response,sizeof s->response};j.text("{\"oldest\":");j.number(oldest);j.text(",\"next\":");j.number(next);j.text(",\"overflow\":");j.number(overflow);j.text(",\"drops\":");j.number(ctl.drops.load());j.text(",\"gap\":");j.number(after<oldest-1);j.text(",\"events\":[");
  U64 begin=after+1;if(begin<oldest)begin=oldest;unsigned count=0;U64 last=after;
  for(U64 seq=begin;seq<next&&count<16;++seq) {
    Entry e{};if(!take())return refuse(r,"503 Service Unavailable");e=s->ring.slots[(seq-1)%32];give();
    if(e.sequence!=seq)continue;
    if(count++) { j.text(","); }
    j.text("[");j.number(e.sequence);j.text(",");j.number(e.time);j.text(",");j.number(unsigned(e.event));j.text(",");j.number(e.detail);j.text("]");last=e.sequence;
  }
  j.text("],\"after\":");j.number(last);j.text("}");if(!j.ok)return refuse(r,"503 Service Unavailable");headers(r,"application/json");return httpd_resp_sendstr(r,s->response);
}
esp_err_t rtcp_history(httpd_req_t*r) {
  if(!request_allowed(r)||std::strcmp(r->uri,"/api/rtcp-history"))return refuse(r,"403 Forbidden");
  auto*s=ctl.storage.load();RtcpCloseHistory copy{};if(!s)return refuse(r,"503 Service Unavailable");
  if(!take())return refuse(r,"503 Service Unavailable","Diagnostics busy");
  copy=s->close_history;give();
  Json j{s->response,sizeof s->response};j.text("{\"count\":");j.number(copy.count);j.text(",\"events\":[");
  for(unsigned age=0;age<copy.count&&age<copy.events.size();++age){
    const auto index=(copy.next+copy.events.size()-1-age)%copy.events.size();const auto&e=copy.events[index];
    if(age)j.text(",");
    j.text("{\"uptime_ms\":");j.number(e.uptime_ms);j.text(",\"session\":");j.number(e.session);
    j.text(",\"network_generation\":");j.number(e.network_generation);j.text(",\"state\":");j.number(e.state);
    j.text(",\"reason\":");j.number(e.reason);j.text(",\"socket_error\":");j.signed_number(e.socket_error);
    j.text(",\"request\":");j.number(e.request);j.text(",\"awaited\":");j.number(e.awaited);
    j.text(",\"response_pending\":");j.number(e.response_pending);j.text("}");
  }
  j.text("]}");if(!j.ok)return refuse(r,"503 Service Unavailable");headers(r,"application/json");return httpd_resp_sendstr(r,s->response);
}
esp_err_t reject_handler(httpd_req_t*r,httpd_err_code_t) {return refuse(r,"403 Forbidden");}
void stop() {ctl.enabled=false;ota_service::revoke();if(ctl.server) {if(httpd_stop(ctl.server)==ESP_OK)ctl.server=nullptr;else ++ctl.failures;}}
}
bool initialize() {
  if(ctl.storage.load())return true;
  auto*p=heap_caps_malloc(sizeof(Storage),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!p) {++ctl.failures;return false;}auto*s=new(p)Storage{};ctl.storage.store(s);
  event(time_now(),Event::BOOT,unsigned(esp_reset_reason()));return true;
}
template<class T> void publish_copy(const T&v,T Snapshots::*member) {
  auto*s=ctl.storage.load();if(!s||!take()) {++ctl.drops;return;}s->copies.*member=v;give();
}
void publish(const Core&v){publish_copy(v,&Snapshots::core);}
void publish(const Power&v){publish_copy(v,&Snapshots::power);}
void publish(const Network&v){publish_copy(v,&Snapshots::network);}
void publish(const RtcpCloseHistory&v){auto*s=ctl.storage.load();if(!s||!take()){++ctl.drops;return;}s->close_history=v;give();}
void event(U64 time,Event e,unsigned detail){auto*s=ctl.storage.load();if(!s||!take()){++ctl.drops;return;}s->ring.push(time,e,detail);give();}
void revoke(){ctl.enabled=false;}
bool quiesce(){stop();return ctl.server==nullptr;}
void service(Address desired) {
  if(!desired.ready) {stop();return;}
  if(!(address()==desired)) {stop();if(ctl.server)return;if(!take())return;ctl.bound=desired;give();}
  if(ctl.server&&ctl.enabled.load())return;
  if(ctl.server) {stop();if(ctl.server)return;}
  if(time_now()<ctl.retry)return;
  ctl.retry=time_now()+5000;
  if(!initialize())return;
  httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.stack_size=8192;c.task_priority=1;c.max_open_sockets=2;c.max_uri_handlers=7;c.max_req_hdr_len=1024;c.max_uri_len=64;c.recv_wait_timeout=3;c.send_wait_timeout=3;c.lru_purge_enable=true;c.open_fn=open_socket;
  if(httpd_start(&ctl.server,&c)!=ESP_OK){ctl.server=nullptr;++ctl.failures;return;}
  ota_service::configure({desired.ip,desired.mask,desired.generation,desired.ready});
  auto route=[](const char*uri,esp_err_t(*fn)(httpd_req_t*)){httpd_uri_t u{};u.uri=uri;u.method=HTTP_GET;u.handler=fn;return httpd_register_uri_handler(ctl.server,&u)==ESP_OK;};
  bool ok=route("/",home)&&route("/api/status",status)&&route("/api/events",events)&&route("/api/rtcp-history",rtcp_history)&&ota_service::register_handlers(ctl.server);
  for(auto code:{HTTPD_400_BAD_REQUEST,HTTPD_404_NOT_FOUND,HTTPD_405_METHOD_NOT_ALLOWED,HTTPD_408_REQ_TIMEOUT,HTTPD_413_CONTENT_TOO_LARGE,HTTPD_414_URI_TOO_LONG,HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,HTTPD_500_INTERNAL_SERVER_ERROR})ok=ok&&httpd_register_err_handler(ctl.server,code,reject_handler)==ESP_OK;
  if(!ok){++ctl.failures;stop();return;}
  ctl.enabled=true;ESP_LOGI("DIAGNOSTICS","running=1 STA_ONLY port=80 diagnostics_read_only=1 ota_authenticated=1 clients=2 psram_bytes=%u",unsigned(sizeof(Storage)));
}
unsigned errors(){return ctl.failures.load();}
unsigned stack_min(){return ctl.margin.load();}
} // namespace diagnostics

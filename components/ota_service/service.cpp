#include "ota_service/api.hpp"
#include "ota_service/server.hpp"

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string_view>

namespace ota_service {
namespace {

constexpr char tag[] = "LOCAL_OTA";
constexpr std::size_t transfer_buffer_bytes = 4096;
constexpr U64 receive_idle_timeout_ms = 30000;
constexpr U64 reboot_delay_ms = 2000;

struct Control {
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  Policy policy{};
  Address address{};
  char pin[7]{};
  char nonce[33]{};
  char session[33]{};
  std::atomic<U64> reboot_at{0};
  std::atomic<unsigned> failures{0};
  std::atomic<unsigned> margin{0};
  std::atomic<bool> writer{false};
  std::atomic<void (*)()> activity{nullptr};
} control;
static_assert(sizeof(Control) <= 256);

U64 now() { return static_cast<U64>(esp_timer_get_time() / 1000); }
bool take() { return !control.lock.test_and_set(std::memory_order_acquire); }
void give() { control.lock.clear(std::memory_order_release); }
void wipe(void *p, std::size_t n) {
  volatile unsigned char *v = static_cast<volatile unsigned char *>(p);
  while (n--) *v++ = 0;
}
void margin() {
  const auto value = uxTaskGetStackHighWaterMark(nullptr);
  auto old = control.margin.load();
  while ((!old || value < old) &&
         !control.margin.compare_exchange_weak(old, value)) {}
}
void random_hex(char *out, unsigned bytes) {
  unsigned char raw[16]{};
  esp_fill_random(raw, bytes);
  constexpr char digits[] = "0123456789abcdef";
  for (unsigned i = 0; i < bytes; ++i) {
    out[2 * i] = digits[raw[i] >> 4];
    out[2 * i + 1] = digits[raw[i] & 15];
  }
  out[2 * bytes] = 0;
  wipe(raw, sizeof raw);
}
unsigned random_pin() {
  constexpr std::uint32_t limit = UINT32_MAX - (UINT32_MAX % 1000000U);
  std::uint32_t value = 0;
  do value = esp_random(); while (value >= limit);
  return value % 1000000U;
}
void clear_secrets() {
  wipe(control.pin, sizeof control.pin);
  wipe(control.nonce, sizeof control.nonce);
  wipe(control.session, sizeof control.session);
}

bool host_allowed(httpd_req_t *request) {
  char host[32]{};
  Address address{};
  if (!take()) return false;
  address = control.address;
  give();
  if (!address.ready ||
      httpd_req_get_hdr_value_str(request, "Host", host, sizeof host) != ESP_OK)
    return false;
  char expected[20]{};
  std::snprintf(expected, sizeof expected, "%u.%u.%u.%u",
                unsigned(address.ip >> 24), unsigned((address.ip >> 16) & 255),
                unsigned((address.ip >> 8) & 255), unsigned(address.ip & 255));
  const std::string_view value(host);
  const std::string_view ip(expected);
  return value == ip ||
         (value.size() == ip.size() + 3 && value.substr(0, ip.size()) == ip &&
          value.substr(ip.size()) == ":80");
}
void security_headers(httpd_req_t *request, const char *type) {
  httpd_resp_set_type(request, type);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
  httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
  httpd_resp_set_hdr(request, "Content-Security-Policy",
    "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
    "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
}
esp_err_t reply(httpd_req_t *request, const char *status, const char *message) {
  httpd_resp_set_status(request, status);
  httpd_resp_set_hdr(request, "Connection", "close");
  security_headers(request, "text/plain; charset=utf-8");
  return httpd_resp_sendstr(request, message);
}

constexpr char page[] = R"HTML(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>REMOTE//01 firmware update</title><style>body{max-width:36rem;margin:2rem auto;padding:0 1rem;background:#11191d;color:#f3f5f2;font:16px system-ui,sans-serif}h1{color:#c2ff00;font-size:1.5rem}section{background:#1c292e;padding:1rem;border-radius:.6rem;margin:1rem 0}input,button{font:inherit;margin:.5rem 0;padding:.7rem;width:100%;box-sizing:border-box}button{background:#c2ff00;color:#11191d;border:0;border-radius:.4rem;font-weight:700}button:disabled{opacity:.5}progress{width:100%}.warn{color:#ffd166}#install{display:none}code{overflow-wrap:anywhere}</style></head><body><h1>REMOTE//01 local update</h1><section><b>Device:</b> REMOTE//01<br><b>Current build:</b> BUILD_VALUE<br><b>Target:</b> ESP32-S3 / project REMOTE01</section><section id="login"><p>Enter the six-digit PIN shown on REMOTE. It expires after two minutes.</p><input id="pin" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="one-time-code" placeholder="OTA PIN"><button id="auth">Authorize this browser</button></section><section id="install"><input id="file" type="file" accept=".bin,application/octet-stream"><label><input id="confirm" type="checkbox" style="width:auto"> I confirm this image is intended for REMOTE//01.</label><button id="upload">Verify and install</button><progress id="progress" max="100" value="0"></progress></section><section><b>Status:</b> <span id="status">Waiting for authorization.</span><p class="warn">Updates are local and integrity-checked. SHA-256 is not publisher authentication. Automatic boot rollback is not enabled; USB recovery remains available.</p></section><script>
const nonce='NONCE_VALUE',status=document.getElementById('status'),enc=new TextEncoder();
// Self-contained SHA-256 keeps the page usable on local HTTP origins where
// browsers intentionally withhold WebCrypto's secure-context-only API.
function hexDigest(input){const b=input instanceof Uint8Array?input:new Uint8Array(input),K=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2],h=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19],n=((b.length+9+63)>>6)<<6,p=new Uint8Array(n),v=new DataView(p.buffer),w=new Int32Array(64),r=(x,s)=>(x>>>s)|(x<<(32-s));p.set(b);p[b.length]=128;v.setUint32(n-8,Math.floor(b.length/0x20000000),false);v.setUint32(n-4,b.length<<3,false);for(let o=0;o<n;o+=64){for(let i=0;i<16;i++)w[i]=v.getInt32(o+4*i,false);for(let i=16;i<64;i++){const x=w[i-15],y=w[i-2];w[i]=((r(x,7)^r(x,18)^(x>>>3))+w[i-16]+(r(y,17)^r(y,19)^(y>>>10))+w[i-7])|0}let[a,c,d,e,f,g,j,k]=h;for(let i=0;i<64;i++){const t1=(k+(r(f,6)^r(f,11)^r(f,25))+((f&g)^(~f&j))+K[i]+w[i])|0,t2=((r(a,2)^r(a,13)^r(a,22))+((a&c)^(a&d)^(c&d)))|0;k=j;j=g;g=f;f=(e+t1)|0;e=d;d=c;c=a;a=(t1+t2)|0}h[0]=(h[0]+a)|0;h[1]=(h[1]+c)|0;h[2]=(h[2]+d)|0;h[3]=(h[3]+e)|0;h[4]=(h[4]+f)|0;h[5]=(h[5]+g)|0;h[6]=(h[6]+j)|0;h[7]=(h[7]+k)|0}return h.map(x=>(x>>>0).toString(16).padStart(8,'0')).join('')}
document.getElementById('auth').onclick=async()=>{const pin=document.getElementById('pin').value;if(!/^\d{6}$/.test(pin)){status.textContent='Enter the six-digit PIN.';return}const proof=hexDigest(enc.encode(nonce+':'+pin));const r=await fetch('/ota/auth',{method:'POST',headers:{'Content-Type':'text/plain'},body:proof});status.textContent=await r.text();if(r.ok){document.getElementById('login').style.display='none';document.getElementById('install').style.display='block'}};
document.getElementById('upload').onclick=async()=>{const file=document.getElementById('file').files[0];if(!file||!document.getElementById('confirm').checked){status.textContent='Select a BIN and confirm the target.';return}const digest=hexDigest(await file.arrayBuffer());const xhr=new XMLHttpRequest();xhr.open('POST','/ota/upload');xhr.setRequestHeader('Content-Type','application/octet-stream');xhr.setRequestHeader('X-REMOTE-SHA256',digest);xhr.setRequestHeader('X-REMOTE-Confirm','REMOTE01');xhr.upload.onprogress=e=>{if(e.lengthComputable)document.getElementById('progress').value=100*e.loaded/e.total};xhr.onload=()=>{status.textContent=xhr.responseText;if(xhr.status===200){document.getElementById('upload').disabled=true;status.textContent+=' Rebooting; reconnect after about 10 seconds.'}};xhr.onerror=()=>status.textContent='Upload connection failed. The running firmware was preserved unless installation success was already reported.';status.textContent='Uploading and verifying...';xhr.send(file)};
</script></body></html>)HTML";

esp_err_t page_handler(httpd_req_t *request) {
  margin();
  if (!host_allowed(request) || request->content_len) return reply(request, "403 Forbidden", "Local access denied");
  if (!take()) return reply(request, "503 Service Unavailable", "Updater busy");
  clear_secrets();
  const unsigned pin = random_pin();
  std::snprintf(control.pin, sizeof control.pin, "%06u", pin);
  random_hex(control.nonce, 16);
  const bool started = control.policy.challenge(now());
  char nonce[33]{};
  std::snprintf(nonce, sizeof nonce, "%s", control.nonce);
  give();
  if (!started) return reply(request, "409 Conflict", "Update already active");
  if (auto callback = control.activity.load()) callback();
  const auto *description = esp_app_get_description();
  security_headers(request, "text/html; charset=utf-8");
  const char *cursor = page;
  struct Replacement { const char *token; const char *value; } replacements[] = {
    {"BUILD_VALUE", description ? description->version : "unknown"},
    {"NONCE_VALUE", nonce},
  };
  for (const auto &replacement : replacements) {
    const char *match = std::strstr(cursor, replacement.token);
    if (!match || httpd_resp_send_chunk(request, cursor, match - cursor) != ESP_OK ||
        httpd_resp_sendstr_chunk(request, replacement.value) != ESP_OK)
      return ESP_FAIL;
    cursor = match + std::strlen(replacement.token);
  }
  if (httpd_resp_sendstr_chunk(request, cursor) != ESP_OK) return ESP_FAIL;
  return httpd_resp_send_chunk(request, nullptr, 0);
}

bool read_exact_body(httpd_req_t *request, char *out, std::size_t count) {
  if (request->content_len != count) return false;
  std::size_t used = 0;
  const U64 deadline = now() + 5000;
  while (used < count && now() <= deadline) {
    const int received = httpd_req_recv(request, out + used, count - used);
    if (received > 0) used += static_cast<std::size_t>(received);
    else if (received != HTTPD_SOCK_ERR_TIMEOUT) return false;
  }
  return used == count;
}

esp_err_t auth_handler(httpd_req_t *request) {
  margin();
  if (!host_allowed(request)) return reply(request, "403 Forbidden", "Local access denied");
  char proof_hex[digest_hex_bytes + 1]{};
  if (!read_exact_body(request, proof_hex, digest_hex_bytes))
    return reply(request, "400 Bad Request", "Invalid authorization proof");
  unsigned char supplied[digest_bytes]{}, expected[digest_bytes]{};
  bool decoded = decode_hex({proof_hex, digest_hex_bytes}, supplied, sizeof supplied);
  if (!take()) return reply(request, "503 Service Unavailable", "Updater busy");
  char material[41]{};
  std::snprintf(material, sizeof material, "%s:%s", control.nonce, control.pin);
  mbedtls_sha256(reinterpret_cast<const unsigned char *>(material), std::strlen(material), expected, 0);
  const bool matches = decoded && constant_time_equal(supplied, expected, sizeof supplied);
  const auto result = control.policy.authenticate(now(), matches);
  wipe(material, sizeof material);
  wipe(supplied, sizeof supplied);
  wipe(expected, sizeof expected);
  if (result == AuthResult::ACCEPTED) {
    wipe(control.pin, sizeof control.pin);
    wipe(control.nonce, sizeof control.nonce);
    random_hex(control.session, session_bytes);
  }
  char session[33]{};
  if (result == AuthResult::ACCEPTED) std::snprintf(session, sizeof session, "%s", control.session);
  give();
  if (result != AuthResult::ACCEPTED) {
    ++control.failures;
    return reply(request, result == AuthResult::LOCKED ? "429 Too Many Requests" :
                          result == AuthResult::EXPIRED ? "408 Request Timeout" :
                          result == AuthResult::BUSY ? "409 Conflict" : "401 Unauthorized",
                 result == AuthResult::LOCKED ? "Authorization locked; reopen the update page." :
                 result == AuthResult::EXPIRED ? "PIN expired; reopen the update page." :
                 result == AuthResult::BUSY ? "Update already active." : "Authorization failed.");
  }
  char cookie[96]{};
  std::snprintf(cookie, sizeof cookie,
                "ota_session=%s; Max-Age=300; Path=/ota; HttpOnly; SameSite=Strict",
                session);
  httpd_resp_set_hdr(request, "Set-Cookie", cookie);
  return reply(request, "200 OK", "Authorized. Select the REMOTE//01 BIN.");
}

bool cookie_matches(httpd_req_t *request) {
  char cookie[128]{};
  if (httpd_req_get_hdr_value_str(request, "Cookie", cookie, sizeof cookie) != ESP_OK)
    return false;
  constexpr std::string_view prefix = "ota_session=";
  std::string_view text(cookie);
  const auto position = text.find(prefix);
  if (position == std::string_view::npos) return false;
  text.remove_prefix(position + prefix.size());
  const auto end = text.find(';');
  const auto token = text.substr(0, end);
  unsigned char supplied[session_bytes]{}, expected[session_bytes]{};
  bool ok = decode_hex(token, supplied, sizeof supplied);
  if (!take()) return false;
  ok = ok && decode_hex(control.session, expected, sizeof expected) &&
       constant_time_equal(supplied, expected, sizeof supplied);
  give();
  wipe(supplied, sizeof supplied);
  wipe(expected, sizeof expected);
  return ok;
}

bool header_equals(httpd_req_t *request, const char *name, const char *wanted) {
  char value[80]{};
  return httpd_req_get_hdr_value_str(request, name, value, sizeof value) == ESP_OK &&
         std::strcmp(value, wanted) == 0;
}

esp_err_t upload_failure(httpd_req_t *request, const char *status,
                         const char *message, esp_ota_handle_t handle,
                         bool began) {
  if (began) esp_ota_abort(handle);
  if (take()) {
    control.policy.finish(false);
    clear_secrets();
    give();
  }
  control.writer = false;
  ++control.failures;
  return reply(request, status, message);
}

esp_err_t upload_handler(httpd_req_t *request) {
  margin();
  if (!host_allowed(request) ||
      !header_equals(request, "Content-Type", "application/octet-stream") ||
      !header_equals(request, "X-REMOTE-Confirm", "REMOTE01"))
    return reply(request, "403 Forbidden", "Target confirmation or local access denied");
  char digest_text[digest_hex_bytes + 1]{};
  if (httpd_req_get_hdr_value_str(request, "X-REMOTE-SHA256", digest_text,
                                  sizeof digest_text) != ESP_OK)
    return reply(request, "400 Bad Request", "Missing SHA-256");
  unsigned char wanted_digest[digest_bytes]{};
  if (!decode_hex(digest_text, wanted_digest, sizeof wanted_digest))
    return reply(request, "400 Bad Request", "Invalid SHA-256");
  const bool session_ok = cookie_matches(request);
  if (!take()) return reply(request, "503 Service Unavailable", "Updater busy");
  const bool admitted = control.policy.begin_upload(now(), session_ok);
  if (admitted) {
    wipe(control.session, sizeof control.session);
    control.writer = true;
  }
  give();
  if (!admitted) return reply(request, "401 Unauthorized", "Authorization expired or already used");

  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  if (!target || target->type != ESP_PARTITION_TYPE_APP ||
      target->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN ||
      !upload_size_allowed(request->content_len, target->size))
    return upload_failure(request, "413 Content Too Large",
                          "Image does not fit an inactive OTA slot", 0, false);

  auto *buffer = static_cast<unsigned char *>(
      heap_caps_malloc(transfer_buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer)
    return upload_failure(request, "503 Service Unavailable",
                          "Transfer buffer unavailable", 0, false);

  std::size_t received_total = 0;
  U64 idle_deadline = now() + receive_idle_timeout_ms;
  auto receive = [&](std::size_t wanted) -> int {
    while (now() <= idle_deadline) {
      const int count = httpd_req_recv(request, reinterpret_cast<char *>(buffer) + received_total,
                                       wanted - received_total);
      if (count > 0) {
        received_total += static_cast<std::size_t>(count);
        idle_deadline = now() + receive_idle_timeout_ms;
        return count;
      }
      if (count != HTTPD_SOCK_ERR_TIMEOUT) return count;
    }
    return HTTPD_SOCK_ERR_TIMEOUT;
  };

  while (received_total < image_prefix_bytes) {
    if (receive(image_prefix_bytes) <= 0) {
      heap_caps_free(buffer);
      return upload_failure(request, "400 Bad Request", "Upload interrupted", 0, false);
    }
  }
  const auto identity = inspect_image_prefix(buffer, received_total);
  if (!identity.compatible()) {
    heap_caps_free(buffer);
    return upload_failure(request, "422 Unprocessable Content",
                          !identity.header ? "Invalid ESP image header" :
                          !identity.chip ? "Image is not for ESP32-S3" :
                          !identity.descriptor ? "Invalid application descriptor" :
                          "Image is not for REMOTE//01", 0, false);
  }

  esp_ota_handle_t handle = 0;
  if (esp_ota_begin(target, request->content_len, &handle) != ESP_OK) {
    heap_caps_free(buffer);
    return upload_failure(request, "500 Internal Server Error",
                          "Could not prepare inactive OTA slot", handle, false);
  }
  bool began = true;
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  bool hash_ok = mbedtls_sha256_starts(&hash, 0) == 0;
  if (hash_ok) hash_ok = mbedtls_sha256_update(&hash, buffer, received_total) == 0;
  if (hash_ok && esp_ota_write(handle, buffer, received_total) != ESP_OK) hash_ok = false;

  while (hash_ok && received_total < request->content_len) {
    const std::size_t chunk = std::min(transfer_buffer_bytes,
                                      request->content_len - received_total);
    std::size_t chunk_used = 0;
    while (chunk_used < chunk && now() <= idle_deadline) {
      const int count = httpd_req_recv(request, reinterpret_cast<char *>(buffer) + chunk_used,
                                       chunk - chunk_used);
      if (count > 0) {
        chunk_used += static_cast<std::size_t>(count);
        idle_deadline = now() + receive_idle_timeout_ms;
      } else if (count != HTTPD_SOCK_ERR_TIMEOUT) {
        hash_ok = false;
        break;
      }
    }
    if (chunk_used != chunk) { hash_ok = false; break; }
    hash_ok = mbedtls_sha256_update(&hash, buffer, chunk_used) == 0 &&
              esp_ota_write(handle, buffer, chunk_used) == ESP_OK;
    received_total += chunk_used;
    if (auto callback = control.activity.load()) callback();
  }
  unsigned char actual_digest[digest_bytes]{};
  if (hash_ok) hash_ok = mbedtls_sha256_finish(&hash, actual_digest) == 0;
  mbedtls_sha256_free(&hash);
  wipe(buffer, transfer_buffer_bytes);
  heap_caps_free(buffer);

  if (!hash_ok || received_total != request->content_len)
    return upload_failure(request, "400 Bad Request", "Upload interrupted", handle, began);
  if (!constant_time_equal(actual_digest, wanted_digest, sizeof actual_digest)) {
    wipe(actual_digest, sizeof actual_digest);
    wipe(wanted_digest, sizeof wanted_digest);
    return upload_failure(request, "422 Unprocessable Content", "SHA-256 mismatch", handle, began);
  }
  wipe(actual_digest, sizeof actual_digest);
  wipe(wanted_digest, sizeof wanted_digest);
  if (esp_ota_end(handle) != ESP_OK)
    return upload_failure(request, "422 Unprocessable Content",
                          "ESP image validation failed", handle, false);
  began = false;
  if (esp_ota_set_boot_partition(target) != ESP_OK)
    return upload_failure(request, "500 Internal Server Error",
                          "Could not select verified OTA slot", handle, began);

  if (take()) {
    control.policy.finish(true);
    clear_secrets();
    give();
  }
  control.writer = false;
  control.reboot_at = now() + reboot_delay_ms;
  ESP_LOGI(tag, "verified=1 bytes=%u target=%s reboot_ms=%llu",
           static_cast<unsigned>(request->content_len), target->label,
           static_cast<unsigned long long>(reboot_delay_ms));
  return reply(request, "200 OK", "Verified REMOTE//01 image installed successfully.");
}

}  // namespace

void configure(Address address) {
  if (!take()) return;
  control.address = address;
  give();
}

void set_activity_callback(void (*callback)()) { control.activity = callback; }

bool register_handlers(httpd_handle_t server) {
  if (!server) return false;
  auto route = [server](const char *uri, httpd_method_t method,
                        esp_err_t (*handler)(httpd_req_t *)) {
    httpd_uri_t item{};
    item.uri = uri;
    item.method = method;
    item.handler = handler;
    return httpd_register_uri_handler(server, &item) == ESP_OK;
  };
  return route("/ota", HTTP_GET, page_handler) &&
         route("/ota/auth", HTTP_POST, auth_handler) &&
         route("/ota/upload", HTTP_POST, upload_handler);
}

void revoke() {
  control.reboot_at = 0;
  control.writer = false;
  if (!take()) return;
  control.policy.revoke();
  control.address = {};
  clear_secrets();
  give();
}

bool ui_view(UiView &view) {
  if (!take()) return false;
  view.state = control.policy.state;
  view.expires_ms = control.policy.expires;
  if (view.state == State::CHALLENGE && now() <= view.expires_ms)
    std::snprintf(view.pin, sizeof view.pin, "%s", control.pin);
  else
    view.pin[0] = 0;
  give();
  return true;
}

bool busy() {
  if (control.writer.load() || control.reboot_at.load()) return true;
  if (!take()) return true;
  const bool value = control.policy.busy(now());
  give();
  return value;
}

bool reboot_due(U64 time) {
  auto due = control.reboot_at.load();
  return due && time >= due && control.reboot_at.compare_exchange_strong(due, 0);
}
unsigned errors() { return control.failures.load(); }
unsigned stack_min() { return control.margin.load(); }

}  // namespace ota_service

#include "diagnostics/model.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
namespace diagnostics {
void Ring::push(U64 time,Event e,unsigned detail) {
  if(next==std::numeric_limits<U64>::max()) { ++overflow; return; }
  auto &s=slots[(next-1)%32]; s={}; s.sequence=next++; s.time=time; s.event=e; s.detail=detail;
  if(s.sequence>32) ++overflow;
}
bool normalize(bool v6,const unsigned char *a,std::size_t n,std::uint32_t &out) {
  out=0; if(!a) return false;
  if(v6) { if(n!=16) return false; for(unsigned i=0;i<10;++i) if(a[i]) return false; if(a[10]!=255||a[11]!=255) return false; a+=12; }
  else if(n!=4) return false;
  out=(std::uint32_t(a[0])<<24)|(std::uint32_t(a[1])<<16)|(std::uint32_t(a[2])<<8)|a[3]; return true;
}
bool socket_allowed(Address a,std::uint32_t local,std::uint32_t peer) {
  // Contiguous netmask, excludes unspecified/multicast and network/broadcast peers.
  const auto inv=~a.mask;
  return a.ready && a.ip && a.mask && !(inv&(inv+1)) && local==a.ip && peer &&
    (peer>>24)<224 && peer!=a.ip && (peer&a.mask)==(a.ip&a.mask) &&
    (peer&inv)!=0 && (peer&inv)!=inv;
}
bool host_allowed(std::string_view host,std::uint32_t ip) {
  char expected[20]; std::snprintf(expected,sizeof expected,"%u.%u.%u.%u",unsigned(ip>>24),unsigned((ip>>16)&255),unsigned((ip>>8)&255),unsigned(ip&255));
  if(host==expected) return true;
  const auto n=std::strlen(expected); return host.size()==n+3 && host.substr(0,n)==expected && host.substr(n)==":80";
}
bool cursor(std::string_view s,U64 &v) {
  v=0; if(s.empty()||s.size()>20) return false;
  for(char c:s) { if(c<'0'||c>'9'||v>(std::numeric_limits<U64>::max()-unsigned(c-'0'))/10) return false; v=v*10+unsigned(c-'0'); } return true;
}
Route route(std::string_view uri,bool get,std::size_t body) {
  if(!get || body) return Route::DENY;
  if(uri=="/")return Route::HOME;
  if(uri=="/api/status")return Route::STATUS;
  constexpr std::string_view prefix="/api/events?after=";U64 value=0;
  if(uri.starts_with(prefix)&&cursor(uri.substr(prefix.size()),value))return Route::EVENTS;
  return Route::DENY;
}
Reboot::Result Reboot::request(U64 now,bool power,bool writer) {
  if(power||writer) return Result::REFUSE;
  if(pending) return Result::COALESCE;
  due=now+500; pending=true; return Result::ACCEPT;
}
bool Reboot::execute(U64 now,bool power,bool writer) {
  if(!pending) return false;
  if(power||writer) { pending=false; return false; }
  if(now<due) return false;
  pending=false; return true;
}
void Json::text(std::string_view s) {
  if(!ok) return;
  if(!capacity||s.size()>=capacity-used) { ok=false; return; }
  std::memcpy(out+used,s.data(),s.size()); used+=s.size(); out[used]=0;
}
void Json::number(U64 v) { char b[24]; std::snprintf(b,sizeof b,"%llu",static_cast<unsigned long long>(v)); text(b); }
void Json::signed_number(int v) { char b[24]; std::snprintf(b,sizeof b,"%d",v); text(b); }
void Json::string(std::string_view s) {
  text("\""); for(unsigned char c:s) { if(c=='"'||c=='\\') { char b[]{'\\',char(c)}; text({b,2}); }
    else if(c<32 || c>=127 || c=='<' || c=='>' || c=='&') { char b[7]; std::snprintf(b,sizeof b,"\\u%04x",unsigned(c)); text(b); }
    else { char b=char(c); text({&b,1}); } } text("\"");
}
namespace {
constexpr const char *core_names[]={"session","capability_revision","dispatches","payload","allocations","frees","failed_allocations","accounting_corrupt","owner_ok","queue_rejected","stack_min_bytes","INT_free","INT_min","INT_largest","PSRAM_free","PSRAM_min","PSRAM_largest","posted","rejected","overflow","busy","applied","stale","invalid","refused","lifecycle","discarded","encoder_transitions","encoder_detents","encoder_last_delta","volume_intents","volume_intents_accepted","volume_admission","volume_connected","volume_known","volume_set","volume_value","volume_at_min","volume_at_max","volume_in_flight"};
constexpr const char *rtcp_counter_names[]={"rtcp_attempts","rtcp_tcp_connected","rtcp_client_hello_sent","rtcp_server_hello_received","rtcp_server_frame_valid","rtcp_server_proof_valid","rtcp_client_finish_sent","rtcp_identity_request_sent","rtcp_identity_response_received","rtcp_identity_accepted"};
constexpr const char *power_names[]={"state","hold","display","wake","wakes","sleep_requests","rounds","errors","idle_ms","IMU","motions","bus_errors","stack_min_bytes","raw","mv","filtered_mv","valid","condition","simulated","read_errors","wake_to_ready_ms","last_sleep_cause","calibrated"};
constexpr const char *network_names[]={"state","connected","stored","storage_ready","provisioning","generation","reconnects","reason","scan_count","errors","allocation_errors","rejected","stack_min_bytes","portal_stack_min_bytes","diagnostic_errors","diagnostic_stack_min_bytes"};
template<std::size_t N> void fields(Json &j,const std::array<U64,N>&v,const char *const (&names)[N]) {
  for(std::size_t i=0;i<N;++i) { j.text(",");j.string(names[i]);j.text(":");j.number(v[i]); }
}
void age(Json &j,U64 time,U64 now) { j.text("{\"available\":");j.number(time!=0);j.text(",\"age_ms\":");j.number(now>=time?now-time:0);j.text(",\"time_ms\":");j.number(time); }
}
bool status_json(const Snapshots&s,U64 now,std::string_view project,std::string_view version,std::string_view sha,unsigned reset,char*out,std::size_t n) {
  Json j{out,n};j.text("{\"system\":{\"uptime_ms\":");j.number(now);j.text(",\"reset_reason\":");j.number(reset);j.text(",\"project\":");j.string(project);j.text(",\"version\":");j.string(version);j.text(",\"elf_sha256\":");j.string(sha);j.text("},\"core\":");age(j,s.core.time,now);fields(j,s.core.values,core_names);
  j.text(",\"rtcp_stage\":");j.number(s.core.rtcp.stage());j.text(",\"rtcp_failure\":");j.number(s.core.rtcp.failure());j.text(",\"rtcp_error\":");j.signed_number(s.core.rtcp.error);j.text(",\"rtcp_server_hello_bytes\":");j.number(s.core.rtcp.server_hello_bytes);j.text(",\"rtcp_close_reason\":");j.number(s.core.rtcp.close_reason);j.text(",\"rtcp_close_count\":");j.number(s.core.rtcp.close_count);j.text(",\"rtcp_poll_last_ms\":");j.number(unsigned(s.core.rtcp.last_poll_32ms)*32U);j.text(",\"rtcp_poll_max_ms\":");j.number(unsigned(s.core.rtcp.max_poll_32ms)*32U);j.text(",\"rtcp_timeout_state\":");j.number(s.core.rtcp.timeout_state);j.text(",\"rtcp_timeout_elapsed_ms\":");j.number(unsigned(s.core.rtcp.timeout_elapsed_32ms)*32U);j.text(",\"rtcp_timeout_queued_bytes\":");j.signed_number(s.core.rtcp.timeout_queued_bytes==UINT16_MAX?-1:s.core.rtcp.timeout_queued_bytes);j.text(",\"rtcp_timeouts_with_unread\":");j.number(s.core.rtcp.timeout_unread_count);
  for(std::size_t i=0;i<s.core.rtcp.counters.size();++i){j.text(",");j.string(rtcp_counter_names[i]);j.text(":");j.number(s.core.rtcp.counters[i]);}
  j.text("},\"network\":");age(j,s.network.time,now);fields(j,s.network.values,network_names);j.text(",\"http_dns_off\":");j.number(s.network.http_dns_off);j.text(",\"ip\":");j.string(s.network.ip);j.text(",\"ssid\":");j.string(s.network.ssid);j.text(",\"rssi\":");j.signed_number(s.network.rssi);
  j.text("},\"power\":");age(j,s.power.time,now);fields(j,s.power.values,power_names);j.text(",\"source\":");j.string(!s.power.time?"UNAVAILABLE":s.power.values[18]?"SIMULATED":"REAL");j.text(",\"external_raw\":");j.number(s.power.external.raw);
  j.text(",\"external_mv\":");j.number(s.power.external.mv);
  j.text(",\"external_valid\":");j.number(s.power.external.valid);
  j.text(",\"external_calibrated\":");j.number(s.power.external.calibrated);
  j.text(",\"external_error\":");j.signed_number(s.power.external.error);
  j.text(",\"external_read_errors\":");j.number(s.power.external.read_errors);
  j.text(",\"external\":");
  j.string(s.power.external.valid && s.power.external.calibrated && s.power.external.error==0 && s.power.external.state==1?"PRESENT":s.power.external.valid && s.power.external.calibrated && s.power.external.error==0 && s.power.external.state==2?"ABSENT":"UNKNOWN");
  j.text(",\"charging\":\"UNKNOWN\",\"SOC\":\"UNAVAILABLE\",\"automatic_system_sleep\":\"ENABLED\",\"reset_history\":[");
  for(unsigned a=0;a<4;++a) { if(a)j.text(",");j.text("[");for(unsigned i=0;i<16;++i) { if(i)j.text(",");j.number(s.power.resets[a][i]); } j.text("]"); }
  j.text("]}}");return j.ok;
}
} // namespace diagnostics

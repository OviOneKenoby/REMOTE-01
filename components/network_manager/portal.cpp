#include "network_manager/portal.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
namespace network_manager::portal {
bool normalize(bool v6, std::span<const unsigned char> bytes, IPv4 &out) {
  out = {};
  if (!v6) { if (bytes.size() != 4) return false; std::copy(bytes.begin(),bytes.end(),out.begin()); return true; }
  if (bytes.size() != 16 || bytes[10] != 255 || bytes[11] != 255) return false;
  for (unsigned i = 0; i < 10; ++i) if (bytes[i]) return false;
  std::copy(bytes.begin()+12,bytes.end(),out.begin()); return true;
}
bool socket_allowed(const IPv4 &local, const IPv4 &peer, bool sta_up, const IPv4 &sta) {
  return local == address && peer[0]==192 && peer[1]==168 && peer[2]==4 &&
    peer[3]>1 && peer[3]<255 && (!sta_up || sta != address);
}
bool canonical(std::string_view host) { return host=="192.168.4.1" || host=="192.168.4.1:80"; }
bool host_valid(std::string_view host) {
  if (host.ends_with(":80")) host.remove_suffix(3);
  if (host.empty() || host.size()>253) return false;
  unsigned label=0; char previous=0;
  for (char c:host) {
    if (c=='.') { if (!label || previous=='-') return false; label=0; }
    else { if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-') ||
      (!label&&c=='-') || ++label>63) return false; }
    previous=c;
  }
  return label && previous!='-';
}
Action request(bool active, bool socket_ok, std::string_view host, bool get,
               std::string_view path, bool token_ok, bool origin_ok) {
  if (!active || !socket_ok || !host_valid(host)) return Action::DENY;
  if (get) {
    if (path=="/") return canonical(host)?Action::PAGE:Action::REDIRECT;
    if (path=="/networks") return canonical(host)?Action::API:Action::DENY;
    if (path=="/credentials" || path=="/scan") return Action::DENY;
    return Action::REDIRECT;
  }
  return canonical(host) && token_ok && origin_ok && (path=="/credentials" || path=="/scan") ? Action::API:Action::DENY;
}
Probe probe(std::string_view path) {
  if (path=="/generate_204" || path=="/gen_204") return Probe::ANDROID;
  if (path=="/hotspot-detect.html" || path=="/library/test/success.html") return Probe::APPLE;
  if (path=="/connecttest.txt" || path=="/ncsi.txt" || path=="/redirect") return Probe::WINDOWS;
  return Probe::OTHER;
}
namespace {
unsigned word(std::span<const unsigned char> p, unsigned i) { return (unsigned(p[i])<<8)|p[i+1]; }
}
std::size_t dns_reply(bool active, std::span<const unsigned char> q, std::span<unsigned char> out) {
  if (!active || q.size()<17 || q.size()>512 || (word(q,2)&0xFECF) ||
      word(q,4)!=1 || word(q,6) || word(q,8) || word(q,10)>1) return 0;
  std::size_t pos=12, name_size=0;
  while (pos<q.size()) {
    unsigned len=q[pos++]; if (!len) break;
    if (len>63 || pos+len>=q.size() || (name_size+=len+1)>254) return 0;
    pos+=len;
  }
  if (pos+4>q.size() || q[pos-1]!=0 || word(q,pos+2)!=1) return 0;
  unsigned type=word(q,pos); pos+=4; const auto question_end=pos;
  if (word(q,10)) {
    if (pos+11>q.size() || q[pos] || word(q,pos+1)!=41 || q[pos+5] || q[pos+6]) return 0;
    auto data_len=word(q,pos+9); pos+=11;
    if (pos+data_len!=q.size()) return 0;
    // Validate bounded EDNS option framing; unsupported options are not reflected.
    const auto end=pos+data_len;
    while(pos<end) { if(pos+4>end)return 0; auto n=word(q,pos+2); pos+=4; if(pos+n>end)return 0; pos+=n; }
  } else if(pos!=q.size()) return 0;
  const bool a=type==1; const auto size=question_end+(a?16:0);
  if (size>out.size()) return 0;
  std::copy(q.begin(),q.begin()+question_end,out.begin());
  out[2]=0x84|(q[2]&1); out[3]=0; // QR+AA, preserve RD, no recursion.
  out[6]=0; out[7]=a?1:0; out[8]=out[9]=out[10]=out[11]=0;
  if(a) {
    constexpr unsigned char answer[]={0xC0,0x0C,0,1,0,1,0,0,0,0,0,4,192,168,4,1}; // TTL0: session-local, no stale cache.
    std::copy(std::begin(answer),std::end(answer),out.begin()+question_end);
  }
  return size;
}
void diagnostic(const Stats &s, char *out, std::size_t n) {
  std::snprintf(out,n,"http_running=%u accepted=%u mapped_v4=%u http_rejected=%u requests=%u redirects=%u http_errors=%u last_reject=%u android=%u apple=%u windows=%u dns_running=%u dns_queries=%u dns_answers=%u dns_rejected=%u dns_errors=%u",
    s.http,s.accepted,s.mapped,s.rejected,s.requests,s.redirects,s.http_errors,s.last_reject,s.android,s.apple,s.windows,s.dns,s.dns_queries,s.dns_answers,s.dns_rejected,s.dns_errors);
}
}

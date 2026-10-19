#include "network_manager/portal.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace network_manager::portal;
static unsigned assertions=0, cycles=1000;
#define CHECK(x) do { ++assertions; if(!(x)){std::printf("FAIL portal line=%d %s\n",__LINE__,#x);return false;} }while(0)
struct Query {
  std::array<unsigned char,513> bytes{}; std::size_t size=12;
  Query(std::string_view name="example.invalid", unsigned type=1) {
    bytes[0]=0x12; bytes[1]=0x34; bytes[2]=1; bytes[5]=1;
    while(!name.empty()){auto i=name.find('.'); auto s=name.substr(0,i); bytes[size++]=s.size();
      for(char c:s)bytes[size++]=c;
      if(i==name.npos)break;
      name.remove_prefix(i+1);
    }
    bytes[size++]=0; bytes[size++]=type>>8; bytes[size++]=type; bytes[size++]=0; bytes[size++]=1;
  }
  std::span<const unsigned char> span()const{return {bytes.data(),size};}
};
static bool mapped_regression() { IPv4 out{}; std::array<unsigned char,16> a{0,0,0,0,0,0,0,0,0,0,255,255,192,168,4,1};
  CHECK(normalize(true,a,out) && out==address); IPv4 peer{192,168,4,2}, zero{};
  CHECK(socket_allowed(out,peer,false,zero)); CHECK(request(true,true,"192.168.4.1",true,"/")==Action::PAGE);
  CHECK(!normalize(true,std::span(a).first(8),out)); CHECK(out==zero); return true; }
static bool native_and_ipv6() { IPv4 out; CHECK(normalize(false,address,out) && out==address); std::array<unsigned char,16> v6{};
  v6[0]=0x20; v6[1]=1; CHECK(!normalize(true,v6,out)); v6={}; v6[15]=1; CHECK(!normalize(true,v6,out)); CHECK(!normalize(false,v6,out)); return true; }
static bool lan_rejection() { IPv4 peer{192,168,4,2}, lan{192,168,1,20};
  CHECK(!socket_allowed(lan,peer,true,lan)); CHECK(!socket_allowed(address,lan,true,lan)); CHECK(!socket_allowed(address,peer,true,address));
  CHECK(request(true,false,"192.168.4.1",true,"/")==Action::DENY);
  CHECK(request(true,false,"192.168.4.1",false,"/credentials",true,true)==Action::DENY); return true; }
static bool host_checks() {
  for(auto h:{"","bad host","x\r\nX:y","a..b","-x.test","x-.test","x:81","x/y","user@host","[::1]"})CHECK(!host_valid(h));
  CHECK(host_valid("connectivitycheck.gstatic.com")); CHECK(host_valid("192.168.4.1:80"));
  CHECK(request(true,true,"bad host",true,"/")==Action::DENY); CHECK(request(true,true,"other.invalid",true,"/networks")==Action::DENY); return true; }
static bool posts() { CHECK(request(true,true,"192.168.4.1",false,"/credentials",true,true)==Action::API);
  CHECK(request(true,true,"192.168.4.1:80",false,"/scan",true,true)==Action::API);
  CHECK(request(true,true,"192.168.4.1",false,"/credentials",false,true)==Action::DENY);
  CHECK(request(true,true,"192.168.4.1",false,"/credentials",true,false)==Action::DENY);
  CHECK(request(true,true,"captive.apple.com",false,"/credentials",true,true)==Action::DENY); return true; }
static bool android() { for(auto p:{"/generate_204","/gen_204"}){CHECK(probe(p)==Probe::ANDROID);CHECK(request(true,true,"connectivitycheck.gstatic.com",true,p)==Action::REDIRECT);} return true; }
static bool apple() { for(auto p:{"/hotspot-detect.html","/library/test/success.html"}){CHECK(probe(p)==Probe::APPLE);CHECK(request(true,true,"captive.apple.com",true,p)==Action::REDIRECT);}
  CHECK(request(true,true,"captive.apple.com",true,"/")==Action::REDIRECT); return true; }
static bool windows() { for(auto p:{"/connecttest.txt","/ncsi.txt","/redirect"}){CHECK(probe(p)==Probe::WINDOWS);CHECK(request(true,true,"www.msftconnecttest.com",true,p)==Action::REDIRECT);}return true; }
static bool direct_fallback() { CHECK(request(true,true,"192.168.4.1",true,"/")==Action::PAGE); CHECK(request(true,true,"192.168.4.1:80",true,"/networks")==Action::API);
  CHECK(request(true,true,"other.invalid",true,"/future-probe")==Action::REDIRECT); CHECK(request(true,true,"other.invalid",false,"/future-probe")==Action::DENY);return true; }
static bool dns_a() { std::array<unsigned char,512> out{};
  for(auto name:{"anything.invalid","connectivitycheck.gstatic.com","captive.apple.com","www.msftconnecttest.com"}){Query q(name);auto n=dns_reply(true,q.span(),out);
    CHECK(n==q.size+16 && out[0]==0x12 && out[1]==0x34 && (out[2]&0x80) && out[7]==1);CHECK(std::equal(address.begin(),address.end(),out.begin()+n-4)); CHECK(out[n-7]==0 && out[n-10]==0);}
  std::array<char,253> name{}; name.fill('a'); name[63]=name[127]=name[191]='.';
  Query largest(std::string_view(name.data(),name.size())); CHECK(dns_reply(true,largest.span(),out)==largest.size+16);
  return true; }
static bool dns_other_edns() { std::array<unsigned char,512> out{}; Query v6("example.invalid",28); CHECK(dns_reply(true,v6.span(),out)==v6.size && out[7]==0);
  Query q; q.bytes[11]=1; unsigned char opt[]={0,0,41,2,0,0,0,0,0,0,0}; for(auto c:opt)q.bytes[q.size++]=c;
  CHECK(dns_reply(true,q.span(),out)>0 && out[11]==0); q.bytes[q.size-1]=3; CHECK(!dns_reply(true,q.span(),out)); return true; }
static bool dns_malformed_bounds() { Query q; std::array<unsigned char,514> out{}; out.fill(0xA5);
  for(unsigned i=0;i<q.size;++i)CHECK(!dns_reply(true,{q.bytes.data(),i},std::span(out).subspan(1,512)));
  auto bad=q; bad.bytes[12]=0xC0; CHECK(!dns_reply(true,bad.span(),out)); bad=q; bad.bytes[5]=2; CHECK(!dns_reply(true,bad.span(),out));
  bad=q; bad.bytes[2]=0x81; CHECK(!dns_reply(true,bad.span(),out)); bad=q; ++bad.size;CHECK(!dns_reply(true,bad.span(),out));
  CHECK(!dns_reply(true,q.span(),std::span(out).first(12))); CHECK(!dns_reply(true,{q.bytes.data(),513},out)); CHECK(out[0]==0xA5 && out[513]==0xA5);return true; }
static bool stopped_and_sessions() { Query q; std::array<unsigned char,512> out{};
  for(unsigned i=0;i<cycles;++i){CHECK(dns_reply(true,q.span(),out)>0);CHECK(!dns_reply(false,q.span(),out));
    CHECK(request(false,true,"192.168.4.1",true,"/")==Action::DENY);CHECK(request(true,true,"192.168.4.1",true,"/")==Action::PAGE);}
  return true; }
static bool stats_redaction() { Stats s; s.http=s.dns=true; s.rejected=2;s.dns_errors=1;char out[512];diagnostic(s,out,sizeof out);
  CHECK(std::strstr(out,"http_rejected=2") && std::strstr(out,"dns_errors=1"));CHECK(!std::strstr(out,"password") && !std::strstr(out,"token") && !std::strstr(out,"ssid"));return true; }
static bool fuzz() { std::array<unsigned char,513> q{};std::array<unsigned char,514> out{};unsigned random=0x10203040;
  for(unsigned i=0;i<cycles;++i){for(auto &c:q){random^=random<<13;random^=random>>17;random^=random<<5;c=random;}
    out.fill(0xA5); auto n=dns_reply(true,{q.data(),i%514},std::span(out).subspan(1,512)); CHECK(n<=512 && out.front()==0xA5 && out.back()==0xA5);}
  return true; }
int main(int argc,char **argv){if(argc>1&&!std::strcmp(argv[1],"--soak"))cycles=10000;
  bool(*tests[])()={mapped_regression,native_and_ipv6,lan_rejection,host_checks,posts,android,apple,windows,direct_fallback,dns_a,dns_other_edns,dns_malformed_bounds,stopped_and_sessions,stats_redaction,fuzz};unsigned pass=0,fail=0;
  for(auto test:tests){if(test())++pass;else ++fail;}std::printf("PORTAL: PASS=%u FAIL=%u assertions=%u lifecycle_cycles=%u fuzz_cycles=%u\n",pass,fail,assertions,cycles,cycles);return fail?1:0;}

#include "network_manager/api.hpp"
#include "network_manager/presentation.hpp"
#include <cstdio>
using namespace network_manager;
static unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
static Credentials sample() { Credentials c{}; std::strcpy(c.ssid, "synthetic-test"); std::strcpy(c.password, "test-only-secret"); return c; }
struct Store {
  Blob blob{}; bool present = false, fail = false, unrelated = true; unsigned writes = 0, erases = 0;
  bool write(const char *space, const char *key, const void *p, std::size_t n) {
    if (std::strcmp(space,"remote_wifi") || std::strcmp(key,"credentials") || n != blob.size() || fail) return false;
    std::memcpy(blob.data(), p, n); present = true; ++writes; return true;
  }
  bool erase(const char *space, const char *key) {
    if (std::strcmp(space,"remote_wifi") || std::strcmp(key,"credentials") || fail) return false;
    blob = {}; present = false; ++erases; return true;
  }
};
static bool first_boot() { Policy p; p.boot(false); CHECK(p.state == State::PROVISIONING && p.portal && !p.stored); CHECK(!p.due(100000)); return true; }
static bool saved_boot() { Policy p; p.boot(true); CHECK(p.state == State::CONNECTING && !p.portal && p.stored); return true; }
static bool failures_preserve() { Store s; CHECK(replace(s,sample())); auto before=s.blob; Policy p; p.boot(true); p.failed(10); CHECK(p.stored && s.blob==before && s.erases==0); CHECK(p.deadline==5010); return true; }
static bool backoff() { Policy p; p.boot(true); unsigned expected[]={5000,10000,20000,40000,60000,60000}; std::uint64_t t=0;
  for (auto d:expected) { p.failed(t); CHECK(p.state==State::RETRY_WAIT && p.deadline==t+d); CHECK(!p.due(t+d-1)); CHECK(p.due(t+d)); t+=d; }
  CHECK(p.reconnects==6 && p.retry==4); p.connected(); CHECK(p.state==State::CONNECTED && p.retry==0); return true; }
static bool corrupt() { auto b=encode(sample()); Credentials c; CHECK(decode(b,c)); CHECK(!std::strcmp(c.password,sample().password));
  for(unsigned i=0;i<b.size();++i){auto bad=b; bad[i]^=1; CHECK(!decode(bad,c)); CHECK(c.password[0]==0 && c.ssid[0]==0);}
  b={}; CHECK(!decode(b,c)); return true; }
static bool limits() { auto c=sample(); std::memset(c.ssid,'s',32); c.ssid[32]=0; std::memset(c.password,'a',64); c.password[64]=0;
  CHECK(valid(c)); c.security=3; CHECK(!valid(c)); c.password[63]=0; CHECK(valid(c)); c.ssid[0]=0; CHECK(!valid(c)); c=sample(); c.password[3]='\n'; CHECK(!valid(c)); std::memset(c.password,'a',65); CHECK(!valid(c)); return true; }
static bool replacement() { Store s; auto old=sample(); CHECK(replace(s,old)); auto b=s.blob; Policy p; p.boot(false); CHECK(p.submit(0)); CHECK(!p.submit(1));
  p.failed(30000); CHECK(p.state==State::PROVISIONING && !p.candidate && s.blob==b); auto next=old; std::strcpy(next.password,"different-test-secret");
  CHECK(p.submit(31000)); s.fail=true; CHECK(!replace(s,next) && s.blob==b); s.fail=false; CHECK(replace(s,next)); p.connected(); CHECK(p.stored && p.state==State::CONNECTED && s.writes==2); return true; }
static bool deletion() { Store s; CHECK(replace(s,sample())); CHECK(erase(s)); CHECK(s.unrelated && !s.present && s.erases==1); Policy p; p.boot(true); auto gen=p.generation; p.forget(); CHECK(p.generation!=gen && p.portal && !p.stored); return true; }
static bool forms() { Credentials c; CHECK(parse("ssid=hidden+test&password=test%26only%2Bsecret&security=2",c)); CHECK(!std::strcmp(c.ssid,"hidden test")); CHECK(!std::strcmp(c.password,"test&only+secret"));
  const char *bad[]={"", "ssid=x&password=short&security=2", "ssid=x&password=testsecret&security=0", "ssid=x&password=testsecret&security=2&", "ssid=x&password=testsecret&security=2&ssid=y", "ssid=x&password=testsecret&security=2&extra=y", "ssid=x%00&password=testsecret&security=2", "ssid=x%0A&password=testsecret&security=2", "ssid=x%XX&password=testsecret&security=2", "ssid=x&password=testsecret", "ssid=x&password=%&security=2"};
  for(auto text:bad){CHECK(!parse(text,c)); CHECK(c.password[0]==0 && c.ssid[0]==0);} char huge[514]; std::memset(huge,'x',sizeof huge); CHECK(!parse(std::string_view(huge,sizeof huge),c)); return true; }
static bool redaction() { Status s; s.state=State::CONNECTED; s.connected=s.stored=true; std::strcpy(s.ip,"192.0.2.10"); char out[384]; diagnostic(s,out,sizeof out);
  CHECK(std::strstr(out,"CONNECTED") && std::strstr(out,"stored=1")); CHECK(!std::strstr(out,sample().password) && !std::strstr(out,"password") && !std::strstr(out,"key=")); char tiny[8]; diagnostic(s,tiny,sizeof tiny); CHECK(tiny[7]==0); return true; }
static bool queue() { Inbox q; Message m; m.kind=Kind::SUBMIT; m.credentials=sample(); m.generation=42;
  for(unsigned i=0;i<8;++i){m.detail=i; CHECK(q.post(m));} CHECK(!q.post(m));
  for(unsigned i=0;i<8;++i){Message out; CHECK(q.pop(out)); CHECK(out.detail==i && out.generation==42 && valid(out.credentials)); CHECK(q.slots[i].credentials.password[0]==0);}
  CHECK(!q.pop(m)); q.lock.test_and_set(); CHECK(!q.post(m) && !q.pop(m)); q.lock.clear(); CHECK(q.post(m)); return true; }
static bool lifecycle() { Policy p; p.boot(false); CHECK(p.submit(0)); unsigned old=p.generation; p.stop(); CHECK(p.generation!=old && !p.portal && p.state==State::DISABLED); p.connected(); p.failed(100); CHECK(p.state==State::DISABLED && !p.due(100000)); p.boot(true); CHECK(p.state==State::CONNECTING && p.stored && !p.candidate); p.connected(); CHECK(p.state==State::CONNECTED && !p.portal); return true; }
static bool ip_presentation() {
  Status s; CHECK(!std::strcmp(ip_text(s).text,"--"));
  s.state=State::PROVISIONING; CHECK(!std::strcmp(ip_text(s).text,"SETUP"));
  s.state=State::CONNECTING; CHECK(!std::strcmp(ip_text(s).text,"CONNECTING"));
  s.state=State::RETRY_WAIT; CHECK(!std::strcmp(ip_text(s).text,"OFFLINE"));
  s.state=State::CONNECTED; s.connected=true;
  const char *good[]={"192.168.1.83","223.255.255.255","169.254.1.2","10.0.0.1"};
  for(auto ip:good){std::strcpy(s.ip,ip); auto text=ip_text(s); CHECK(text.assigned && !std::strcmp(text.text,ip));}
  const char *bad[]={"","0.0.0.0","127.0.0.1","224.1.2.3","255.255.255.255","192.168.1","192.168.1.256","192.168.01.2","1.2.3.4x","1..3.4"};
  for(auto ip:bad){std::strcpy(s.ip,ip);CHECK(!ip_text(s).assigned && !std::strcmp(ip_text(s).text,"NO IP"));}
  std::memset(s.ip,'1',sizeof s.ip); CHECK(!ip_text(s).assigned);
  std::strcpy(s.ip,"192.168.1.83"); s.connected=false; CHECK(!ip_text(s).assigned);
  s.state=State::RETRY_WAIT; CHECK(!ip_text(s).assigned); return true;
}
static bool recovery_confirmation() {
  Recovery r; CHECK(!r.authorize(SettingsAction::FACTORY_RESET,1,1,0,1));
  CHECK(!r.authorize(static_cast<SettingsAction>(99),1,1,0,1));
  CHECK(!r.authorize(SettingsAction::OPEN_SETUP,1,2,0,1));
  CHECK(r.authorize(SettingsAction::OPEN_SETUP,2,2,0,1));
  CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,2,2,0,10)); auto ticket=r.ticket;
  CHECK(ticket && r.result==SettingsResult::ARMED && r.deadline==10010);
  CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,2,2,ticket+1,11) && !r.ticket);
  CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,2,2,ticket,12));
  CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,2,2,0,20)); ticket=r.ticket;
  CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,2,2,ticket,10021));
  CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,2,2,0,30)); ticket=r.ticket;
  CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,2,3,ticket,31));
  CHECK(r.authorize(SettingsAction::CANCEL_SETUP,3,3,0,32) && !r.ticket);
  CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,3,3,0,40)); ticket=r.ticket;
  CHECK(r.authorize(SettingsAction::CONFIRM_WIFI_RESET,3,3,ticket,10040));
  CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,3,3,ticket,10040));
  r.serial=UINT64_MAX; CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,3,3,0,50));
  r.serial=0; CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,3,3,0,UINT64_MAX)); return true;
}
static bool recovery_preserves_credentials() {
  Store s; CHECK(replace(s,sample())); const auto saved=s.blob;
  Policy p; p.boot(true); p.failed(0); auto epoch=p.generation; p.open_setup();
  CHECK(p.stored && p.portal && p.state==State::PROVISIONING && p.generation!=epoch);
  CHECK(s.blob==saved && s.erases==0); CHECK(p.submit(1)); p.failed(30001);
  CHECK(p.portal && p.stored && p.state==State::PROVISIONING && s.blob==saved);
  Credentials c; CHECK(decode(s.blob,c)); p.boot(true); CHECK(!p.portal && p.stored && p.state==State::CONNECTING);
  Recovery r; CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,p.generation,p.generation,0,0));
  CHECK(s.erases==0 && s.unrelated); CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,p.generation,p.generation,0,1));
  CHECK(r.authorize(SettingsAction::CONFIRM_WIFI_RESET,p.generation,p.generation,r.ticket,2));
  s.fail=true; CHECK(!erase(s) && s.blob==saved); s.fail=false; CHECK(erase(s)); p.forget();
  CHECK(!p.stored && p.portal && s.unrelated); return true;
}
static unsigned cycles=1000;
static bool recovery_stress() {
  for(unsigned i=0;i<cycles;++i) {
    Recovery r; Policy p; Store s; CHECK(replace(s,sample())); p.boot(true);
    const auto before=s.blob; auto epoch=p.generation;
    CHECK(!r.authorize(SettingsAction::ARM_WIFI_RESET,epoch,epoch,0,i));
    CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,epoch,epoch,r.ticket+1,i));
    CHECK(s.blob==before && s.erases==0);
    CHECK(r.authorize(SettingsAction::OPEN_SETUP,epoch,epoch,0,i)); p.open_setup();
    CHECK(p.stored && p.portal && p.generation!=epoch);
    CHECK(!r.authorize(SettingsAction::CONFIRM_WIFI_RESET,epoch,p.generation,1,i));
    CHECK(p.submit(i)); p.failed(i+30000); CHECK(s.blob==before && p.stored);
    CHECK(r.authorize(SettingsAction::CANCEL_SETUP,p.generation,p.generation,0,i+30001));
    p.boot(true); CHECK(p.stored && !p.portal);
    CHECK(!r.authorize(SettingsAction::FACTORY_RESET,p.generation,p.generation,0,i+30002));
    CHECK(s.blob==before && s.unrelated);
  } return true;
}
static bool soak() { for(unsigned i=0;i<cycles;++i){Policy p; Store s; auto c=sample(); p.boot(false); CHECK(p.submit(i)); CHECK(replace(s,c)); p.connected(); p.portal=false;
  for(unsigned j=0;j<8;++j){p.failed(i); CHECK(p.stored && s.present && s.erases==0); CHECK(p.due(p.deadline)); p.connected();}
  p.stop(); Credentials restored; CHECK(decode(s.blob,restored)); p.boot(true); CHECK(p.state==State::CONNECTING); CHECK(erase(s)); p.forget(); CHECK(p.state==State::PROVISIONING); } return true; }
int main(int argc,char **argv) { if(argc>1 && !std::strcmp(argv[1],"--soak"))cycles=10000;
  bool (*tests[])()={first_boot,saved_boot,failures_preserve,backoff,corrupt,limits,replacement,deletion,forms,redaction,queue,lifecycle,ip_presentation,recovery_confirmation,recovery_preserves_credentials,recovery_stress,soak}; unsigned pass=0,fail=0;
  for(auto test:tests){if(test())++pass;else ++fail;} std::printf("NETWORK: PASS=%u FAIL=%u assertions=%u cycles=%u\n",pass,fail,assertions,cycles); return fail?1:0; }

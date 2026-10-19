#include "network_manager/sleep.hpp"
#include "network_manager/model.hpp"
#include "power_management/sleep_timer.hpp"
#include <cstdio>
#include <cstring>
static unsigned assertions=0;
#define CHECK(x) do{++assertions;if(!(x)){std::printf("FAIL line=%u\n",__LINE__);return 1;}}while(0)
struct Port {bool stop_ok=true,resume_ok=true;unsigned stops=0,resumes=0;bool stop(){++stops;return stop_ok;}bool resume(){++resumes;return resume_ok;}};
int main(int argc,char**) {
  using namespace network_manager;
  using namespace network_manager::sleep;
  unsigned cases=0;
  const unsigned cycles=argc>1?10000:1000;
  Channel c;Owner o;Port p;
  for(unsigned i=0;i<cycles;++i) {
    auto e=c.begin();CHECK(e && c.busy());o.tick(c,p,true,i*10);
    CHECK(c.ready(e) && o.paused());CHECK(!c.begin());
    auto old=c.desired.load();c.resume(e);c.respond(old,Reply::READY);CHECK(!c.ready(e));
    o.tick(c,p,true,i*10+1);CHECK(!c.busy() && !o.paused());
  }
  CHECK(p.stops==cycles && p.resumes==cycles);++cases;
  {Channel x;Owner owner;Port port;auto e=x.begin();owner.tick(x,port,false,0);
   CHECK(x.reply(e)==Reply::REFUSED && !port.stops);x.resume(e);owner.tick(x,port,true,1);CHECK(!x.busy());}++cases;
  {Channel x;Owner owner;Port port;port.stop_ok=false;port.resume_ok=false;
   auto e=x.begin();owner.tick(x,port,true,0);CHECK(!x.ready(e) && owner.paused());
   CHECK(x.reply(e)==Reply::FAULT);x.resume(e);owner.tick(x,port,true,999);CHECK(port.resumes==1);
   port.resume_ok=true;owner.tick(x,port,true,1000);CHECK(!owner.paused() && !x.busy());}++cases;
  {Channel x;Owner owner;Port port;auto e=x.begin();auto late=x.desired.load();
   x.resume(e);x.respond(late,Reply::READY);CHECK(!x.ready(e));owner.tick(x,port,true,10000);CHECK(!port.stops && !x.busy());
   auto next=x.begin();CHECK(next!=e);x.respond(late,Reply::READY);CHECK(!x.ready(next));}++cases;
  {Channel x;x.epoch=Channel::max_epoch;CHECK(!x.begin() && !x.busy());}++cases;
  {Inbox inbox;Message m{};for(unsigned i=0;i<8;++i)CHECK(inbox.post(m));
   CHECK(!inbox.post(m) && !inbox.seal());for(unsigned i=0;i<8;++i)CHECK(inbox.pop(m));
   CHECK(inbox.seal() && !inbox.post(m));CHECK(inbox.unseal() && inbox.post(m));}++cases;
  for(unsigned fail=0;fail<7;++fail) {
    unsigned order=0;auto step=[&](unsigned expected){++order;return order==expected && expected!=fail;};
    bool ok=quiesce([&]{++order;},[&]{return step(2);},[&]{return step(3);},[&]{return step(4);},[&]{return step(5);},[&]{return step(6);});
    CHECK(order==6);CHECK(ok==(fail==0 || fail==1));
  }++cases;
  CHECK(eligible(true,true,false,false,false,false,false,false,0));
  CHECK(!eligible(false,true,false,false,false,false,false,false,0));
  CHECK(!eligible(true,false,false,false,false,false,false,false,0));
  CHECK(!eligible(true,true,true,false,false,false,false,false,0));
  CHECK(!eligible(true,true,false,true,false,false,false,false,0));
  CHECK(!eligible(true,true,false,false,true,false,false,false,0));
  CHECK(!eligible(true,true,false,false,false,true,false,false,0));
  CHECK(!eligible(true,true,false,false,false,false,true,false,0));
  CHECK(!eligible(true,true,false,false,false,false,false,true,0));
  CHECK(!eligible(true,true,false,false,false,false,false,false,0xffffffffu-1));
  CHECK(!eligible(true,true,false,false,false,false,false,false,0,0xffffffffu-1));++cases;
  {power_management::SleepTimer timer;unsigned enabled=0,disabled=0;
   auto enable=[&]{++enabled;return true;};auto disable=[&]{++disabled;return true;};
   CHECK(timer.configure(false,enable,disable) && !enabled && !disabled);
   CHECK(timer.configure(true,enable,disable) && timer.armed && enabled==1);
   CHECK(!timer.configure(false,enable,[]{return false;}) && timer.armed);
   CHECK(timer.configure(false,enable,disable) && !timer.armed && disabled==1);
   CHECK(timer.configure(false,enable,disable) && disabled==1);
   CHECK(!timer.configure(true,[]{return false;},disable) && !timer.armed);}++cases;
  std::printf("NETWORK_SLEEP: PASS=%u FAIL=0 assertions=%u cycles=%u\n",cases,assertions,cycles);
}


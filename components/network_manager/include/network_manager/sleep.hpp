#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
namespace network_manager::sleep {
enum class Reply : unsigned { RUN=0, PREPARE=1, READY=2, REFUSED=3, RECOVERING=4, FAULT=5 };
// One POWER writer, one NETWORK reader/writer. Packed epoch+phase is atomic on S3.
struct Channel {
  static constexpr unsigned max_epoch=0x1fffffffu;
  std::atomic<unsigned> desired{0}, answer{0};
  unsigned epoch=0; // POWER only
  unsigned begin() {
    if(busy() || epoch==max_epoch) return 0;
    const auto e=++epoch; desired.store((e<<3)|unsigned(Reply::PREPARE)); return e;
  }
  bool busy() const {return desired.load()!=answer.load();}
  bool ready(unsigned e) const {
    return e && desired.load()==((e<<3)|unsigned(Reply::PREPARE)) &&
      answer.load()==((e<<3)|unsigned(Reply::READY));
  }
  Reply reply(unsigned e) const {
    auto a=answer.load();return (a>>3)==e?Reply(a&7):Reply::PREPARE;
  }
  void resume(unsigned e) {if(e && (desired.load()>>3)==e) desired.store(e<<3);}
  void respond(unsigned word,Reply r) {if(desired.load()==word) answer.store((word&~7u)|unsigned(r));}
};
// Synchronous SDK calls belong to NETWORK's Port; cancellation never grants ready.
struct Owner {
  enum class Phase { RUN, PARKED, RECOVERY };
  Phase phase=Phase::RUN; unsigned seen=0; std::uint64_t retry=0;
  template<class Port> void tick(Channel &c,Port &p,bool eligible,std::uint64_t now) {
    auto word=c.desired.load();bool prepare=(word&7)==unsigned(Reply::PREPARE);
    if(phase==Phase::RUN && prepare && word!=seen) {
      seen=word;
      if(!eligible) {c.respond(word,Reply::REFUSED);return;}
      if(!p.stop()) {phase=Phase::RECOVERY;c.respond(word,Reply::FAULT);retry=now;}
      else {phase=Phase::PARKED;c.respond(word,Reply::READY);}
    }
    word=c.desired.load();prepare=(word&7)==unsigned(Reply::PREPARE);
    if(phase==Phase::PARKED && !prepare) {phase=Phase::RECOVERY;retry=now;}
    if(phase==Phase::RECOVERY && now>=retry) {
      if(p.resume()) {phase=Phase::RUN;c.respond(word,prepare?Reply::FAULT:Reply::RUN);}
      else {retry=now+1000;c.respond(word,Reply::FAULT);}
    }
    if(phase==Phase::RUN && !prepare)c.respond(word,Reply::RUN);
  }
  bool paused()const{return phase!=Phase::RUN;}
};
// Production and host tests execute the same service-stop ordering.
template<class Revoke,class Diagnostics,class Portal,class Dns,class Wifi,class Fence>
bool quiesce(Revoke revoke,Diagnostics diagnostics,Portal portal,Dns dns,Wifi wifi,Fence fence) {
  revoke();bool ok=diagnostics();ok=portal()&&ok;ok=dns()&&ok;
  ok=wifi()&&ok;return fence()&&ok;
}
inline bool eligible(bool initialized,bool stored,bool portal,bool candidate,bool scan,
                     bool submitting,bool ticket,bool reboot,unsigned generation,unsigned barrier=0) {
  return initialized && stored && !portal && !candidate && !scan && !submitting &&
    !ticket && !reboot && generation<std::numeric_limits<unsigned>::max()-1 &&
    barrier<std::numeric_limits<unsigned>::max()-1;
}
} // namespace network_manager::sleep

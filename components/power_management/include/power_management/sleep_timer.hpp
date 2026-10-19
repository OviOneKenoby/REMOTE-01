#pragma once
namespace power_management {
// POWER owns this flag and the only timer-wakeup configuration calls.
struct SleepTimer {
  bool armed=false;
  template<class Enable,class Disable> bool configure(bool forced,Enable enable,Disable disable) {
    if(forced) {if(!enable())return false;armed=true;return true;}
    if(armed) {if(!disable())return false;armed=false;}
    return true;
  }
};
}

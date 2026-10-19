#pragma once
#include "REG/QMI8658Constants.h"
#include <cstdint>
// Temporary DEVELOPMENT_ONLY boot diagnostics; =0 removes record/hooks/summary.
#ifndef POWER_WOM_DIAGNOSTIC
#define POWER_WOM_DIAGNOSTIC 1
#endif
namespace power_management {
#if POWER_WOM_DIAGNOSTIC
struct WomDiagnostic {
  std::uint8_t stage=0, failed=0, op=0, reg=0, expected=0, mask=0, actual=0, actual_valid=0;
  std::int32_t error=0;
  std::uint32_t polls=0, stage_polls=0, elapsed_ms=0, total_ms=0, started=0, stage_started=0;
  std::uint32_t result=0, cause=0;
};
static_assert(sizeof(WomDiagnostic)<=64);
#endif
// V2 QMI8658 + vendor datasheet sections 5.9/9 and vendored SensorQMI8658.
// Minimal bus seam: read/write byte, now milliseconds, delay milliseconds.
struct Wom : QMI8658Constants {
  enum class Stage : unsigned { IDENTITY=1, RESET, RESET_ACK, CTRL1, DISABLE,
    ODR, THRESHOLD, INT_CONFIG, APPLY, APPLY_ACK, ACK, ACK_CLEAR, ENABLE,
    VALIDATE_CTRL7, VALIDATE_CTRL1, CLEAR_STARTUP };
  static constexpr unsigned threshold_mg = 200, blanking_samples = 4;
#if POWER_WOM_DIAGNOSTIC
  template<class Bus> static WomDiagnostic *trace(Bus &b) {
    if constexpr(requires { b.wom_diagnostic(); }) {
      auto &d=b.wom_diagnostic(); return d.result==1?&d:nullptr;
    } else return nullptr;
  }
#endif
  template<class Bus> static void step(Bus &b, Stage s, unsigned op,
      unsigned reg, unsigned expected, unsigned mask) {
#if POWER_WOM_DIAGNOSTIC
    if(auto*d=trace(b)) {
      d->stage=unsigned(s);d->op=op;d->reg=reg;d->expected=expected;d->mask=mask;
      d->actual=d->actual_valid=0;d->stage_polls=0;d->error=0;d->stage_started=b.now();
    }
#else
    (void)b;(void)s;(void)op;(void)reg;(void)expected;(void)mask;
#endif
  }
  template<class Bus> static bool failure(Bus &b, unsigned cause) {
#if POWER_WOM_DIAGNOSTIC
    if(auto*d=trace(b)) {d->failed=d->stage;d->cause=cause;}
#else
    (void)b;(void)cause;
#endif
    return false;
  }
  template<class Bus> static void observed(Bus &b, unsigned char v) {
#if POWER_WOM_DIAGNOSTIC
    if(auto*d=trace(b)) {d->actual=v;d->actual_valid=1;}
#else
    (void)b;(void)v;
#endif
  }
  template<class Bus> static bool wait(Bus &b, unsigned reg, unsigned mask, unsigned expected, unsigned timeout) {
    const auto start = b.now(); unsigned char v = 0;
    do {
#if POWER_WOM_DIAGNOSTIC
      if(auto*d=trace(b)) {++d->polls;++d->stage_polls;}
#endif
      if (!b.read(reg, v)) {
#if POWER_WOM_DIAGNOSTIC
        if(auto*d=trace(b)) d->actual=d->actual_valid=0;
#endif
        return failure(b,1);
      }
      observed(b,v);
      if ((v & mask) == expected) return true;
      b.delay(5);
    } while (b.now() - start < timeout);
    return failure(b,2);
  }
  template<class Bus> static bool read(Bus &b, Stage s, unsigned reg, unsigned expected, bool validate=true) {
    step(b,s,1,reg,expected,validate?0xff:0);
    unsigned char v=0;
    if(!b.read(reg,v)) return failure(b,1);
    observed(b,v);
    return !validate || v==expected || failure(b,3);
  }
  template<class Bus> static bool write(Bus &b, Stage s, unsigned reg, unsigned value) {
    step(b,s,2,reg,value,0xff);
    return b.write(reg,value) || failure(b,1);
  }
  template<class Bus> static bool poll(Bus &b, Stage s, unsigned reg, unsigned mask, unsigned expected, unsigned timeout) {
    step(b,s,3,reg,expected,mask);
    return wait(b,reg,mask,expected,timeout);
  }
  template<class Bus> static bool configure(Bus &b) {
#if POWER_WOM_DIAGNOSTIC
    if constexpr(requires { b.wom_diagnostic(); }) {
      auto &d=b.wom_diagnostic();d={};d.result=1;d.started=b.now();
    }
#endif
    // Same transactions, values, short-circuit order, waits and exact readbacks.
    const bool ok=read(b,Stage::IDENTITY,QMI8658_REG_WHOAMI,QMI8658_REG_WHOAMI_DEFAULT) &&
      write(b,Stage::RESET,QMI8658_REG_RESET,QMI8658_REG_RESET_DEFAULT) &&
      poll(b,Stage::RESET_ACK,QMI8658_REG_RST_RESULT,0xff,QMI8658_REG_RST_RESULT_VAL,500) &&
      write(b,Stage::CTRL1,QMI8658_REG_CTRL1,0x48) &&
      write(b,Stage::DISABLE,QMI8658_REG_CTRL7,0) &&
      write(b,Stage::ODR,QMI8658_REG_CTRL2,0x2d) &&
      write(b,Stage::THRESHOLD,QMI8658_REG_CAL1_L,threshold_mg) &&
      write(b,Stage::INT_CONFIG,QMI8658_REG_CAL1_H,0x80|blanking_samples) &&
      write(b,Stage::APPLY,QMI8658_REG_CTRL9,0x08) &&
      poll(b,Stage::APPLY_ACK,QMI8658_REG_STATUS_INT,0x80,0x80,1000) &&
      write(b,Stage::ACK,QMI8658_REG_CTRL9,0) &&
      poll(b,Stage::ACK_CLEAR,QMI8658_REG_STATUS_INT,0x80,0,1000) &&
      write(b,Stage::ENABLE,QMI8658_REG_CTRL7,1) &&
      read(b,Stage::VALIDATE_CTRL7,QMI8658_REG_CTRL7,1) &&
      read(b,Stage::VALIDATE_CTRL1,QMI8658_REG_CTRL1,0x48) &&
      read(b,Stage::CLEAR_STARTUP,QMI8658_REG_STATUS1,0,false);
#if POWER_WOM_DIAGNOSTIC
    if(auto*d=trace(b)) {
      const auto end=b.now();d->elapsed_ms=end-d->stage_started;
      d->total_ms=end-d->started;d->result=ok?2:3;
    }
#endif
    return ok;
  }
  template<class Bus> static bool status(Bus &b, bool &motion) {
    unsigned char v = 0; motion = false;
    if (!b.read(QMI8658_REG_STATUS1, v)) return false;
    motion = (v & 0x04) != 0; // reading STATUS1 acknowledges WoM
    return true;
  }
};
} // namespace power_management

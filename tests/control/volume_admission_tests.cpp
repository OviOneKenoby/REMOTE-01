#include "control_home/api.hpp"
#include <cstdio>

namespace {
using namespace control_home;
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)

Snapshot ready(std::uint8_t volume = 50) {
  Snapshot value;
  value.connection = Connection::CONNECTED;
  value.session = 1;
  value.capability_revision = 1;
  value.volume_known = true;
  value.volume_set = true;
  value.volume = volume;
  return value;
}

bool exact_reasons() {
  Channel c;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::NOT_CONNECTED);
  c.latest.connection = Connection::CONNECTED;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::NO_SESSION);
  c.latest.session = 1;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::NO_CAPABILITY_REVISION);
  c.latest.capability_revision = 1;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::VOLUME_UNKNOWN);
  c.latest.volume_known = true;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::VOLUME_SET_UNAVAILABLE);
  c.latest.volume_set = true; c.latest.volume = 0;
  CHECK(c.volume_admission(Action::VOLUME_DOWN) == VolumeAdmission::AT_MINIMUM);
  c.latest.volume = 100;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::AT_MAXIMUM);
  c.latest.volume = 50; c.volume_outstanding = true;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::ACCEPTED);
  c.volume_outstanding = false; c.issued = UINT64_MAX;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::TOKEN_LIMIT);
  c.issued = 0; c.count = Channel::capacity;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::QUEUE_FULL);
  c.count = 0;
  CHECK(c.volume_admission(Action::VOLUME_UP) == VolumeAdmission::ACCEPTED);
  return true;
}

bool decision_is_unchanged_and_counted() {
  Channel c; c.publish(ready(68));
  CHECK(c.request(Action::VOLUME_UP));
  CHECK(c.volume_attempts == 1 && c.volume_accepted == 1);
  CHECK(c.last_volume_admission == VolumeAdmission::ACCEPTED);
  CHECK(c.request(Action::VOLUME_UP));
  CHECK(c.volume_attempts == 2 && c.volume_accepted == 2);
  CHECK(c.last_volume_admission == VolumeAdmission::ACCEPTED);
  Snapshot confirmed = ready(70);
  CHECK(c.volume_outstanding&&c.volume_target==70);
  Intent next;CHECK(c.pop(next)&&next.volume==70);
  confirmed.volume=70;c.publish(confirmed);
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid);
  CHECK(c.request(Action::VOLUME_DOWN));
  CHECK(c.volume_attempts == 3 && c.volume_accepted == 3);
  return true;
}

bool exact_resolution_and_lifecycle_fencing() {
  Channel c;c.publish(ready(100));
  CHECK(c.request(Action::VOLUME_DOWN));
  Intent first;CHECK(c.pop(first));
  CHECK(first.volume==99&&c.volume_outstanding&&c.volume_token==first.token);
  CHECK(!c.resolve(first.token+1,VolumeResolution::DEFINITE_FAILURE));
  CHECK(!c.resolve(first.token,VolumeResolution::UNCERTAIN));
  CHECK(c.resolve(first.token,VolumeResolution::CONFIRMED));
  CHECK(c.volume_outstanding&&c.volume_confirmed);
  CHECK(c.resolve(first.token,VolumeResolution::DEFINITE_FAILURE));
  CHECK(!c.volume_outstanding&&c.volume_token==0);

  CHECK(c.request(Action::VOLUME_DOWN));
  Intent second;CHECK(c.pop(second));
  CHECK(c.request(Action::VOLUME_DOWN));
  CHECK(c.last_volume_admission==VolumeAdmission::ACCEPTED);
  c.publish(ready(99));
  CHECK(c.volume_outstanding&&c.volume_target==98);
  Intent queued;CHECK(c.pop(queued)&&queued.volume==98);
  c.publish(ready(98));
  CHECK(!c.volume_outstanding);

  CHECK(c.request(Action::VOLUME_DOWN));
  Intent third;CHECK(c.pop(third));
  auto changed=ready(99);changed.capability_revision=2;c.publish(changed);
  CHECK(c.volume_outstanding&&c.count==0);
  CHECK(c.resolve(third.token,VolumeResolution::DEFINITE_FAILURE));
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid);

  CHECK(c.request(Action::VOLUME_DOWN));
  Intent fourth;CHECK(c.pop(fourth));
  auto disconnected=changed;disconnected.connection=Connection::UNAVAILABLE;c.publish(disconnected);
  CHECK(!c.volume_outstanding);
  c.publish(ready(80));
  CHECK(!c.resolve(fourth.token,VolumeResolution::DEFINITE_FAILURE));
  return true;
}

bool rapid_accumulation_reversal_and_bounds() {
  for (unsigned steps : {1u,3u,5u}) {
    Channel c;c.publish(ready(95));
    for (unsigned i=0;i<steps;++i)CHECK(c.request(Action::VOLUME_DOWN));
    CHECK(c.count==1&&c.desired_volume==95-steps);
    Intent command;CHECK(c.pop(command));CHECK(command.volume==95-steps);
    c.publish(ready(static_cast<std::uint8_t>(95-steps)));
    CHECK(!c.volume_outstanding&&!c.desired_volume_valid&&c.latest.volume==95-steps);
  }
  Channel reversal;reversal.publish(ready(50));
  CHECK(reversal.request(Action::VOLUME_DOWN));
  CHECK(reversal.request(Action::VOLUME_DOWN));
  CHECK(reversal.request(Action::VOLUME_DOWN));
  CHECK(reversal.request(Action::VOLUME_UP));
  CHECK(reversal.request(Action::VOLUME_UP));
  CHECK(reversal.desired_volume==49);
  Intent command;CHECK(reversal.pop(command)&&command.volume==49);
  reversal.publish(ready(49));
  CHECK(!reversal.volume_outstanding&&!reversal.desired_volume_valid);

  Channel cancelled;cancelled.publish(ready(50));
  CHECK(cancelled.request(Action::VOLUME_DOWN));
  CHECK(cancelled.request(Action::VOLUME_UP));
  CHECK(!cancelled.volume_outstanding&&!cancelled.desired_volume_valid);
  CHECK(cancelled.count==0);

  Channel low;low.publish(ready(1));
  CHECK(low.request(Action::VOLUME_DOWN));
  CHECK(!low.request(Action::VOLUME_DOWN));
  CHECK(low.last_volume_admission==VolumeAdmission::AT_MINIMUM);
  Channel high;high.publish(ready(99));
  CHECK(high.request(Action::VOLUME_UP));
  CHECK(!high.request(Action::VOLUME_UP));
  CHECK(high.last_volume_admission==VolumeAdmission::AT_MAXIMUM);
  return true;
}

bool bounded_latency_model_and_provisional_view() {
  Channel c;c.publish(ready(95),0);
  unsigned submissions=0;std::uint64_t response_at=UINT64_MAX;
  std::uint8_t response_volume=0;std::uint64_t first_audio=UINT64_MAX;
  for(std::uint64_t now=0;now<=250;++now) {
    if(now<60 && now%4==0) CHECK(c.request(Action::VOLUME_DOWN,now));
    if(now==response_at) {
      if(first_audio==UINT64_MAX) first_audio=now;
      c.publish(ready(response_volume),now);
      response_at=UINT64_MAX;
    }
    Intent command;
    if(response_at==UINT64_MAX&&c.pop(command)) {
      ++submissions;response_volume=command.volume;response_at=now+30;
    }
    if(now==20) {
      const auto view=c.view();
      CHECK(view.volume==95&&view.desired_volume_known&&view.desired_volume<95&&view.volume_pending);
    }
    if(!c.volume_outstanding&&!c.desired_volume_valid&&c.latest.volume==80) {
      CHECK(first_audio<100);
      CHECK(now-56<250);
      CHECK(submissions<=3);
      CHECK(c.last_authoritative_ms==now);
      std::printf("LATENCY MODEL first_command_ms=0 first_audio_ms=%llu "
                  "final_after_last_ms=%llu ui_refresh_ms=20 submissions=%u\n",
                  static_cast<unsigned long long>(first_audio),
                  static_cast<unsigned long long>(now-56), submissions);
      return true;
    }
  }
  CHECK(false);
  return false;
}

bool external_change_and_invalidation_do_not_replay() {
  Channel c;c.publish(ready(50),1);
  CHECK(c.request(Action::VOLUME_UP,2));
  Intent command;CHECK(c.pop(command)&&command.volume==51);
  auto disconnected=ready(70);disconnected.connection=Connection::UNAVAILABLE;
  c.publish(disconnected,3);
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid&&c.count==0);
  auto next=ready(70);next.session=2;c.publish(next,4);
  CHECK(c.latest.volume==70&&c.count==0);
  return true;
}

bool in_flight_reversal_and_external_authority() {
  Channel reversal;reversal.publish(ready(50),1);
  CHECK(reversal.request(Action::VOLUME_DOWN,2));
  Intent first;CHECK(reversal.pop(first)&&first.volume==49);
  CHECK(reversal.request(Action::VOLUME_DOWN,3));
  CHECK(reversal.request(Action::VOLUME_UP,4));
  CHECK(reversal.request(Action::VOLUME_UP,5));
  CHECK(reversal.desired_volume==50&&reversal.count==0);
  CHECK(reversal.resolve(first.token,VolumeResolution::CONFIRMED));
  reversal.publish(ready(49),6);
  Intent successor;CHECK(reversal.pop(successor)&&successor.volume==50);
  CHECK(reversal.resolve(successor.token,VolumeResolution::CONFIRMED));
  reversal.publish(ready(50),7);
  CHECK(!reversal.volume_outstanding&&reversal.latest.volume==50);

  Channel external;external.publish(ready(50),1);
  CHECK(external.request(Action::VOLUME_DOWN,2));
  Intent command;CHECK(external.pop(command)&&command.volume==49);
  CHECK(external.request(Action::VOLUME_DOWN,3));
  CHECK(external.desired_volume==48);
  CHECK(external.resolve(command.token,VolumeResolution::CONFIRMED));
  external.publish(ready(60),4);
  CHECK(external.latest.volume==60&&!external.volume_outstanding);
  CHECK(!external.desired_volume_valid&&external.count==0);
  return true;
}

bool confirmed_result_does_not_discard_newer_accumulated_target() {
  Channel slow;slow.publish(ready(53),1);
  std::uint64_t now=2;
  for(unsigned i=0;i<10;++i) {
    const auto before=slow.latest.volume;
    CHECK(slow.request(Action::VOLUME_DOWN,now++));
    Intent command;CHECK(slow.pop(command)&&command.volume==before-1);
    CHECK(slow.resolve(command.token,VolumeResolution::CONFIRMED));
    slow.publish(ready(before),now++);
    slow.publish(ready(static_cast<std::uint8_t>(before-1)),now++);
    CHECK(!slow.volume_outstanding&&!slow.desired_volume_valid);
  }
  CHECK(slow.volume_accepted==10&&slow.latest.volume==43);

  Channel c;c.publish(ready(53),1);
  CHECK(c.request(Action::VOLUME_DOWN,2));
  Intent first;CHECK(c.pop(first)&&first.volume==52);
  for(unsigned i=0;i<4;++i)CHECK(c.request(Action::VOLUME_DOWN,3+i));
  CHECK(c.desired_volume==48&&c.volume_accepted==5);
  CHECK(c.resolve(first.token,VolumeResolution::CONFIRMED));
  // The runtime republishes its unchanged pre-command observation until the
  // next acquisition. It must not erase the newer locally accumulated target.
  c.publish(ready(53),7);
  CHECK(c.volume_outstanding&&c.desired_volume==48);
  c.publish(ready(52),8);
  Intent second;CHECK(c.pop(second)&&second.volume==48);
  for(unsigned i=0;i<5;++i)CHECK(c.request(Action::VOLUME_DOWN,9+i));
  CHECK(c.desired_volume==43&&c.volume_accepted==10);
  CHECK(c.resolve(second.token,VolumeResolution::CONFIRMED));
  c.publish(ready(52),14);
  CHECK(c.volume_outstanding&&c.desired_volume==43);
  c.publish(ready(48),15);
  Intent final;CHECK(c.pop(final)&&final.volume==43);
  CHECK(c.resolve(final.token,VolumeResolution::CONFIRMED));
  c.publish(ready(48),16);
  CHECK(c.volume_outstanding&&c.desired_volume==43);
  c.publish(ready(43),17);
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid&&c.latest.volume==43);
  CHECK(c.volume_accepted==slow.volume_accepted&&
        c.latest.volume==slow.latest.volume);

  Channel reversal;reversal.publish(ready(53),1);
  CHECK(reversal.request(Action::VOLUME_DOWN,2));
  Intent down;CHECK(reversal.pop(down)&&down.volume==52);
  CHECK(reversal.request(Action::VOLUME_DOWN,3));
  CHECK(reversal.request(Action::VOLUME_DOWN,4));
  CHECK(reversal.request(Action::VOLUME_UP,5));
  CHECK(reversal.request(Action::VOLUME_UP,6));
  CHECK(reversal.desired_volume==52&&reversal.volume_accepted==5);
  CHECK(reversal.resolve(down.token,VolumeResolution::CONFIRMED));
  reversal.publish(ready(53),7);
  CHECK(reversal.volume_outstanding&&reversal.desired_volume==52);
  reversal.publish(ready(52),8);
  CHECK(!reversal.volume_outstanding&&!reversal.desired_volume_valid&&
        reversal.latest.volume==52);
  return true;
}

bool uncertain_and_invalidation_cancel_safely() {
  Channel c;c.publish(ready(60));
  for(unsigned i=0;i<Channel::capacity*4;++i)
    CHECK(c.request(Action::VOLUME_DOWN));
  CHECK(c.count==1&&c.desired_volume==28&&c.volume_outstanding);
  const auto token=c.volume_token;
  CHECK(!c.resolve(token,VolumeResolution::UNCERTAIN));
  CHECK(c.volume_outstanding&&c.desired_volume_valid);
  CHECK(!c.resolve(token-1,VolumeResolution::DEFINITE_FAILURE));
  auto new_session=ready(60);new_session.session=2;c.publish(new_session);
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid&&c.count==0);
  CHECK(!c.resolve(token,VolumeResolution::DEFINITE_FAILURE));
  return true;
}

bool d009_capability_revision_does_not_drop_accumulated_target() {
  Channel queued;queued.publish(ready(53),1);
  CHECK(queued.request(Action::VOLUME_DOWN,2));
  auto queued_revision=ready(53);queued_revision.capability_revision=2;
  queued.publish(queued_revision,3);
  Intent refreshed;CHECK(queued.pop(refreshed)&&refreshed.volume==52&&
                         refreshed.capability_revision==2);

  Channel c;c.publish(ready(53),1);
  CHECK(c.request(Action::VOLUME_DOWN,2));
  Intent first;CHECK(c.pop(first)&&first.volume==52);
  for(unsigned i=0;i<9;++i)CHECK(c.request(Action::VOLUME_DOWN,3+i));
  CHECK(c.desired_volume==43&&c.volume_accepted==10&&c.count==0);

  // A D-009 playback-context availability update advances the target-wide
  // capability revision while volume authority remains valid.
  auto revised=ready(53);revised.capability_revision=2;c.publish(revised,12);
  CHECK(c.volume_outstanding&&c.desired_volume_valid&&c.desired_volume==43);
  CHECK(c.resolve(first.token,VolumeResolution::CONFIRMED));
  revised.volume=52;c.publish(revised,13);
  Intent successor;CHECK(c.pop(successor)&&successor.volume==43&&
                         successor.capability_revision==2);
  CHECK(c.resolve(successor.token,VolumeResolution::CONFIRMED));
  revised.volume=43;c.publish(revised,14);
  CHECK(!c.volume_outstanding&&!c.desired_volume_valid&&c.latest.volume==43);

  Channel reversal;reversal.publish(ready(50),1);
  CHECK(reversal.request(Action::VOLUME_DOWN,2));
  Intent down;CHECK(reversal.pop(down)&&down.volume==49);
  CHECK(reversal.request(Action::VOLUME_DOWN,3));
  CHECK(reversal.request(Action::VOLUME_DOWN,4));
  CHECK(reversal.request(Action::VOLUME_UP,5));
  CHECK(reversal.request(Action::VOLUME_UP,6));
  auto changed=ready(50);changed.capability_revision=2;
  reversal.publish(changed,7);
  CHECK(reversal.desired_volume==49&&reversal.volume_outstanding);
  CHECK(reversal.resolve(down.token,VolumeResolution::CONFIRMED));
  changed.volume=49;reversal.publish(changed,8);
  CHECK(!reversal.volume_outstanding&&!reversal.desired_volume_valid&&
        reversal.latest.volume==49);
  return true;
}
bool diagnostic_counters_classify_pipeline() {
  Channel c;c.publish(ready(50),1);
  CHECK(c.authoritative_updates==1);
  CHECK(c.request(Action::VOLUME_DOWN,2));
  Intent first;CHECK(c.pop(first)&&c.volume_queued==1);
  CHECK(c.resolve(first.token,VolumeResolution::CONFIRMED));
  CHECK(c.volume_completed==1&&c.volume_failed==0);
  auto applied=ready(49);c.publish(applied,3);
  CHECK(c.authoritative_updates==2&&!c.volume_outstanding);

  CHECK(c.request(Action::VOLUME_DOWN,4));
  Intent failed;CHECK(c.pop(failed));
  CHECK(c.resolve(failed.token,VolumeResolution::DEFINITE_FAILURE));
  CHECK(c.volume_failed==1&&c.volume_cancelled==0);

  CHECK(c.request(Action::VOLUME_DOWN,5));
  Intent session;CHECK(c.pop(session));
  auto disconnected=applied;disconnected.connection=Connection::UNAVAILABLE;
  c.publish(disconnected,6);
  CHECK(c.volume_cancelled==1&&c.cancel_session==1);

  Channel authority;authority.publish(ready(50),1);
  CHECK(authority.request(Action::VOLUME_DOWN,2));
  Intent pending;CHECK(authority.pop(pending));
  auto unavailable=ready(50);unavailable.volume_set=false;
  authority.publish(unavailable,3);
  CHECK(authority.volume_cancelled==1&&authority.cancel_authority==1);

  Channel capability;capability.publish(ready(50),1);
  CHECK(capability.request(Action::VOLUME_DOWN,2));
  CHECK(capability.pop(pending));
  auto missing=ready(50);missing.capability_revision=0;
  capability.publish(missing,3);
  CHECK(capability.cancel_capability==1);

  Channel external;external.publish(ready(50),1);
  CHECK(external.request(Action::VOLUME_DOWN,2));
  CHECK(external.pop(pending));
  CHECK(external.resolve(pending.token,VolumeResolution::CONFIRMED));
  auto changed=ready(51);external.publish(changed,3);
  CHECK(external.volume_cancelled==1&&external.cancel_divergence==1);
  return true;
}
}

int main() {
  const bool results[] = {exact_reasons(), decision_is_unchanged_and_counted(),
                          exact_resolution_and_lifecycle_fencing(),
                          rapid_accumulation_reversal_and_bounds(),
                          uncertain_and_invalidation_cancel_safely(),
                          bounded_latency_model_and_provisional_view(),
                          external_change_and_invalidation_do_not_replay(),
                          in_flight_reversal_and_external_authority(),
                          confirmed_result_does_not_discard_newer_accumulated_target(),
                          d009_capability_revision_does_not_drop_accumulated_target(),
                          diagnostic_counters_classify_pipeline()};
  unsigned pass = 0; for (bool result : results) pass += result;
  std::printf("VOLUME ADMISSION SUMMARY PASS=%u FAIL=%u assertions=%u\n",
              pass, 11 - pass, assertions);
  return pass == 11 ? 0 : 1;
}

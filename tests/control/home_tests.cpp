#include "control_home/development.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
using namespace control_home;
using control::Memory;
using control_home::development::Session;
namespace {
unsigned assertions = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++assertions;                                                              \
    if (!(x)) {                                                                \
      std::printf("FAIL line=%d %s\n", __LINE__, #x);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)
#define START                                                                  \
  Session s(control::Id("home.test.epoch"));                                   \
  CHECK(s.initialize(10))
Intent intent(const Session &s, std::uint64_t token, Action action) {
  const auto x = s.snapshot();
  return {token, action, x.session, x.capability_revision};
}
bool coherent() {
  START;
  auto x = s.snapshot();
  CHECK(x.connection == Connection::CONNECTED &&
        x.playback == Playback::PLAYING && x.volume == 68);
  CHECK(std::strcmp(x.title, "Mock One") == 0 &&
        std::strcmp(x.subtitle, "Synthetic") == 0);
  CHECK(x.previous && x.toggle && x.next);
  return true;
}
bool remote_pending_preserves_playback_availability() {
  Snapshot value;
  value.previous = value.toggle = value.next = true;
  auto remote = development::present_snapshot(value, true, true);
  CHECK(remote.previous && remote.toggle && remote.next);
  auto synthetic = development::present_snapshot(value, true, false);
  CHECK(!synthetic.previous && !synthetic.toggle && !synthetic.next);
  return true;
}
bool connected() {
  START;
  CHECK(s.snapshot().connection == Connection::CONNECTED);
  return true;
}
bool disconnected() {
  START;
  CHECK(s.device.command('d', 11));
  auto x = s.snapshot();
  CHECK(x.connection == Connection::UNAVAILABLE && !x.previous && !x.toggle &&
        !x.next && !x.volume_known && !x.title[0]);
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 12) && s.device.mock.dispatches == 0);
  return true;
}
bool playing() {
  START;
  CHECK(s.snapshot().playback == Playback::PLAYING);
  return true;
}
bool paused() {
  START;
  CHECK(s.device.command('a', 11));
  CHECK(s.snapshot().playback == Playback::PAUSED && s.snapshot().toggle);
  return true;
}
bool unknown() {
  START;
  CHECK(s.device.command('u', 11));
  CHECK(s.snapshot().playback == Playback::UNKNOWN && !s.snapshot().toggle);
  CHECK(!s.handle(intent(s, 1, Action::TOGGLE), 12) && s.device.mock.dispatches == 0);
  return true;
}
bool volume_zero() {
  START;
  CHECK(s.device.command('0', 11));
  CHECK(s.snapshot().volume_known && s.snapshot().volume == 0);
  return true;
}
bool volume_full() {
  START;
  CHECK(s.device.command('1', 11));
  CHECK(s.snapshot().volume_known && s.snapshot().volume == 100);
  return true;
}
bool external_volume() {
  START;
  CHECK(s.device.command('v', 11));
  CHECK(s.snapshot().volume == 25 && s.device.mock.dispatches == 0);
  return true;
}
bool canonical_volume_command_and_terminal_result() {
  START;
  CHECK(s.device.command('v',11));
  auto i=intent(s,1,Action::VOLUME_UP);i.volume=26;
  CHECK(s.handle(i,12));
  CHECK(s.volume_resolution(i.token)==VolumeResolution::CONFIRMED);
  const auto &level=s.store.records[0].get()->request.arguments.find("level")->rational;
  CHECK((level==control::Rational{13,50}));
  CHECK(s.snapshot().volume==26);
  return true;
}
bool external_playback() {
  START;
  CHECK(s.device.command('a', 11));
  CHECK(s.snapshot().playback == Playback::PAUSED &&
        s.device.mock.dispatches == 0);
  return true;
}
bool stale() {
  START;
  CHECK(s.device.command('v', 11));
  auto x = s.snapshot();
  CHECK(s.device.command('s', 12));
  CHECK(s.snapshot() == x);
  return true;
}
bool previous() {
  START;
  CHECK(s.handle(intent(s, 1, Action::PREVIOUS), 11));
  CHECK(s.device.mock.dispatches == 1 &&
        std::strcmp(s.snapshot().title, "Mock Three") == 0);
  CHECK(s.device.mock.last.value.request_id ==
        s.store.records[0].get()->request.request_id);
  CHECK(s.store.records[0].get()->request.media_context_generation.present &&
        s.store.records[0].get()->request.preconditions.size() == 1 &&
        s.store.records[0].get()->request.preconditions[0].field_id.view() ==
            "playback.state");
  return true;
}
bool next() {
  START;
  CHECK(s.handle(intent(s, 1, Action::NEXT), 11));
  CHECK(s.device.mock.dispatches == 1 &&
        std::strcmp(s.snapshot().title, "Mock Two") == 0);
  CHECK(s.store.records[0].get()->request.media_context_generation.present &&
        s.store.records[0].get()->request.preconditions.size() == 1);
  return true;
}
bool play() {
  START;
  CHECK(s.device.command('a', 11));
  CHECK(s.handle(intent(s, 1, Action::TOGGLE), 12));
  CHECK(s.device.mock.dispatches == 1 &&
        s.snapshot().playback == Playback::PLAYING);
  CHECK(s.store.records[0].get()->request.operation_id.view() ==
        "playback.play");
  return true;
}
bool pause() {
  START;
  CHECK(s.handle(intent(s, 1, Action::TOGGLE), 11));
  CHECK(s.device.mock.dispatches == 1 &&
        s.snapshot().playback == Playback::PAUSED);
  CHECK(s.store.records[0].get()->request.operation_id.view() ==
        "playback.pause");
  CHECK(s.store.records[0].get()->request.preconditions.size() == 1);
  return true;
}
bool unsupported() {
  START;
  auto cap = control::MockAdapter::operation(
      s.b, "playback.next", control::Feedback::CORRELATED_RESULT);
  cap.support = control::Support::UNSUPPORTED;
  CHECK(!s.core.capability(s.d, s.t, cap, 11).present);
  CHECK(!s.snapshot().next && !s.handle(intent(s, 1, Action::NEXT), 12) &&
        s.device.mock.dispatches == 0);
  return true;
}
bool removed() {
  START;
  auto count = s.core.target(s.d, s.t)->capabilities.size();
  CHECK(s.device.command('-', 11));
  CHECK(s.core.target(s.d, s.t)->capabilities.size() == count - 1 &&
        !s.snapshot().next);
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 12) && s.device.mock.dispatches == 0);
  return true;
}
bool restored() {
  START;
  CHECK(s.device.command('-', 11));
  CHECK(s.device.command('+', 12));
  CHECK(s.snapshot().next);
  CHECK(s.handle(intent(s, 1, Action::NEXT), 13) && s.device.mock.dispatches == 1);
  return true;
}
bool session() {
  START;
  auto x = s.snapshot();
  auto old = s.core.field(s.d, s.t, "playback.state")->selected.value;
  CHECK(s.device.command('d', 11));
  CHECK(s.device.command('r', 12));
  CHECK(s.snapshot().connection == Connection::CONNECTED &&
        s.snapshot().session == x.session + 1 &&
        s.snapshot().capability_revision > x.capability_revision);
  old.receipt_time.tick_ms = 13;
  CHECK(!s.core.observe(old, 13));
  CHECK(s.snapshot().playback == Playback::PLAYING);
  return true;
}
bool duplicate() {
  START;
  CHECK(s.handle(intent(s, 1, Action::NEXT), 11));
  auto request = s.store.records[0].get()->request.request_id;
  CHECK(s.handle(intent(s, 1, Action::NEXT), 12));
  CHECK(s.device.mock.dispatches == 1 &&
        s.store.records[0].get()->request.request_id == request);
  CHECK(!s.handle(intent(s, 1, Action::PREVIOUS), 13));
  CHECK(s.handle(intent(s, 2, Action::NEXT), 14));
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 15) && s.device.mock.dispatches == 2);
  return true;
}
std::string source(const char *p) {
  std::ifstream f(p);
  return {std::istreambuf_iterator<char>(f), {}};
}
bool home_boundary() {
  auto ui = source("components/user_app/user_app.cpp"),
       api = source("components/control_home/include/control_home/api.hpp");
  CHECK(!ui.empty() && !api.empty());
  CHECK(ui.find("mock_adapter") == std::string::npos &&
        ui.find("development.hpp") == std::string::npos &&
        ui.find("control/core") == std::string::npos);
  CHECK(api.find("MockAdapter") == std::string::npos &&
        api.find("control/core") == std::string::npos);
  return true;
}
bool no_ui_truth() {
  auto ui = source("components/user_app/user_app.cpp");
  auto first = ui.find("static void home_click");
  auto last = ui.find("static void tuner_control_close", first);
  auto callback = ui.substr(first, last - first);
  CHECK(callback.find("control_home::request") != std::string::npos);
  CHECK(callback.find("lv_label_set_text") == std::string::npos &&
        callback.find(".playback") == std::string::npos &&
        callback.find(".submit(") == std::string::npos);
  CHECK(ui.find("bool playing") == std::string::npos);
  return true;
}
bool safe_copy() {
  Snapshot copy;
  {
    START;
    copy = s.snapshot();
    CHECK(s.device.command('a', 11));
    CHECK(copy.playback == Playback::PLAYING &&
          s.snapshot().playback == Playback::PAUSED);
  }
  CHECK(std::strcmp(copy.title, "Mock One") == 0 && copy.volume == 68);
  return true;
}
bool queue_full() {
  Channel c;
  Snapshot ready;
  ready.connection = Connection::CONNECTED;
  ready.session = ready.capability_revision = 1;
  ready.previous = ready.toggle = ready.next = true;
  c.publish(ready);
  for (unsigned i = 0; i < c.capacity; i++)
    CHECK(c.request(Action::NEXT));
  CHECK(!c.request(Action::PREVIOUS) && c.rejected == 1 &&
        c.count == c.capacity);
  for (unsigned i = 0; i < c.capacity; i++) {
    Intent x;
    CHECK(c.pop(x) && x.token == i + 1 && x.action == Action::NEXT);
  }
  Intent x;
  CHECK(!c.pop(x));
  CHECK(c.request(Action::TOGGLE) && c.pop(x) && x.token == 10);
  return true;
}
bool no_leak() {
  auto before = Memory::used;
  {
    START;
    for (unsigned i = 1; i <= 2000; i++) {
      s.core.tick(10 + i * 100);
      s.device.acquire(10 + i * 100);
      auto snapshot = s.snapshot();
      CHECK(snapshot.volume_known && !s.store.blocked);
    }
    CHECK(s.store.samples.size() == 4 && s.store.records[0].get() == nullptr);
  }
  CHECK(Memory::used == before);
  return true;
}
bool unavailable_cap() {
  START;
  CHECK(s.device.command('o', 11));
  CHECK(!s.snapshot().next && !s.handle(intent(s, 1, Action::NEXT), 12));
  CHECK(s.device.command('c', 13) && s.snapshot().next);
  return true;
}
bool revised_cap() {
  START;
  auto revision = s.snapshot().capability_revision;
  CHECK(s.device.command('+', 11));
  CHECK(s.snapshot().capability_revision > revision && s.snapshot().next);
  return true;
}
bool delayed_result() {
  START;
  CHECK(s.device.command('l', 11));
  CHECK(s.handle(intent(s, 1, Action::TOGGLE), 12));
  CHECK(s.device.pending && s.snapshot().playback == Playback::PLAYING &&
        !s.snapshot().toggle);
  CHECK(!s.handle(intent(s, 2, Action::NEXT), 13) && s.device.mock.dispatches == 1);
  CHECK(s.device.command('f', 14));
  CHECK(!s.device.pending && s.snapshot().playback == Playback::PAUSED &&
        s.snapshot().toggle);
  return true;
}
bool pending_keeps_volume_accumulator_available() {
  START;
  CHECK(s.device.command('l',11));
  Channel channel;channel.publish(s.snapshot());
  CHECK(channel.request(Action::VOLUME_DOWN));
  Intent first;CHECK(channel.pop(first));
  CHECK(s.handle(first,12)&&s.pending());
  channel.publish(s.snapshot());
  CHECK(channel.latest.volume_set);
  for(unsigned i=0;i<4;++i)CHECK(channel.request(Action::VOLUME_DOWN));
  CHECK(channel.desired_volume==58&&channel.count==0&&channel.volume_outstanding);
  return true;
}
bool expired_field() {
  START;
  s.core.tick(5010);
  CHECK(s.snapshot().playback == Playback::UNKNOWN &&
        !s.snapshot().volume_known && !s.snapshot().toggle);
  CHECK(!s.handle(intent(s, 1, Action::TOGGLE), 5011));
  CHECK(s.device.mock.dispatches == 0);
  s.device.acquire(5012);
  CHECK(s.snapshot().toggle);
  return true;
}
bool conflict() {
  START;
  auto sample = s.core.field(s.d, s.t, "playback.state")->selected.value;
  sample.value.value.playback = control::Playback::PAUSED;
  sample.receipt_time.tick_ms = 11;
  CHECK(!s.core.observe(sample, 11));
  CHECK(s.snapshot().playback == Playback::CONFLICT && !s.snapshot().toggle);
  CHECK(!s.handle(intent(s, 1, Action::TOGGLE), 12) && s.device.mock.dispatches == 0);
  return true;
}
bool coalesced() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(c.generation == 1);
  for (unsigned i = 0; i < 100; i++)
    c.publish(s.snapshot());
  CHECK(c.generation == 1);
  CHECK(s.device.command('v', 11));
  c.publish(s.snapshot());
  CHECK(c.generation == 2 && c.latest.volume == 25);
  return true;
}
bool queue_owner() {
  START;
  Channel c;
  c.publish(s.snapshot());
  CHECK(c.request(Action::NEXT));
  CHECK(s.device.mock.dispatches == 0);
  Intent i;
  CHECK(c.pop(i));
  CHECK(s.handle(i, 11) && s.device.mock.dispatches == 1);
  return true;
}
bool status_invalidation() {
  START;
  CHECK(!s.core.reachability(s.b, control::Reachability::UNREACHABLE, 11)
             .present);
  CHECK(s.snapshot().connection == Connection::UNAVAILABLE);
  CHECK(s.core.field(s.d, s.t, "volume.level")->selected.present == false);
  CHECK(!s.handle(intent(s, 1, Action::NEXT), 12));
  return true;
}
bool repeat_commands_purge() {
  START;
  for (unsigned i = 1; i <= 150; i++) {
    auto now = 10 + control::U64(i) * 1000;
    s.device.acquire(now);
    CHECK(s.handle(intent(s, i, Action::NEXT), now));
    s.core.tick(now);
    CHECK(!s.store.blocked);
  }
  s.core.tick(210011);
  CHECK(s.device.mock.dispatches == 150 && s.store.effects.size() == 0);
  for (const auto &record : s.store.records)
    CHECK(!record.get());
  return true;
}
} // namespace
namespace {
bool serial_key() {
  START;
  using namespace control_home::development;
  CHECK(input(s, 'a', 11, true).accepted);
  auto reply = input(s, 'p', 12, true);
  CHECK(reply.kind == InputKind::COMMAND && reply.accepted);
  CHECK(s.snapshot().playback == Playback::PLAYING &&
        s.device.mock.dispatches == 0);
  CHECK(input(s, 'm', 13, true).kind == InputKind::TELEMETRY);
  CHECK(input(s, 'm', 13, true).accepted);
  return true;
}
bool serial_cr() {
  START;
  auto before = s.snapshot();
  auto commits = s.journal.commits;
  auto reply = development::input(s, '\r', 11, true);
  CHECK(reply.kind == development::InputKind::IGNORE && !reply.accepted);
  CHECK(s.snapshot() == before && s.journal.commits == commits);
  return true;
}
bool serial_lf() {
  START;
  auto before = s.snapshot();
  auto commits = s.journal.commits;
  auto reply = development::input(s, '\n', 11, true);
  CHECK(reply.kind == development::InputKind::IGNORE && !reply.accepted);
  CHECK(s.snapshot() == before && s.journal.commits == commits);
  return true;
}
bool serial_unknown() {
  START;
  auto before = s.snapshot();
  auto commits = s.journal.commits;
  for (const auto key : {'?', 'X', ' '}) {
    auto reply = development::input(s, key, 11, true);
    CHECK(reply.kind == development::InputKind::UNKNOWN && !reply.accepted);
  }
  CHECK(s.snapshot() == before && s.journal.commits == commits);
  CHECK(development::input(s, 0, 11, true).kind ==
        development::InputKind::IGNORE);
  return true;
}
bool serial_owner() {
  START;
  auto before = s.snapshot();
  auto commits = s.journal.commits;
  CHECK(!development::input(s, 'a', 11, false).accepted);
  CHECK(!development::input(s, 'm', 11, false).accepted);
  CHECK(s.snapshot() == before && s.journal.commits == commits);
  CHECK(development::input(s, 'a', 12, true).accepted);
  CHECK(s.snapshot().playback == Playback::PAUSED);
  const auto runtime = source("components/control_home/runtime.cpp");
  const auto task = runtime.substr(runtime.find("void task("),
                                   runtime.find("} // namespace") -
                                       runtime.find("void task("));
  CHECK(task.find("usb_serial_jtag_driver_install(&input_config)") !=
        std::string::npos);
  CHECK(task.find("usb_serial_jtag_read_bytes(keys, sizeof keys, 0)") !=
        std::string::npos);
  CHECK(task.find("development::input(") != std::string::npos);
  CHECK(task.find("xTaskGetCurrentTaskHandle() == owner") != std::string::npos);
  CHECK(runtime.find("open(\"/dev/secondary\"") == std::string::npos);
  return true;
}
} // namespace
int main() {
  struct Test {
    const char *name;
    bool (*run)();
  };
  const Test tests[] = {
#define TEST(x) {#x, x}
      TEST(coherent),
      TEST(remote_pending_preserves_playback_availability),
      TEST(connected),
      TEST(disconnected),
      TEST(playing),
      TEST(paused),
      TEST(unknown),
      TEST(volume_zero),
      TEST(volume_full),
      TEST(external_volume),
      TEST(canonical_volume_command_and_terminal_result),
      TEST(external_playback),
      TEST(stale),
      TEST(previous),
      TEST(next),
      TEST(play),
      TEST(pause),
      TEST(unsupported),
      TEST(removed),
      TEST(restored),
      TEST(session),
      TEST(duplicate),
      TEST(home_boundary),
      TEST(no_ui_truth),
      TEST(safe_copy),
      TEST(queue_full),
      TEST(no_leak),
      TEST(unavailable_cap),
      TEST(revised_cap),
      TEST(delayed_result),
      TEST(pending_keeps_volume_accumulator_available),
      TEST(expired_field),
      TEST(conflict),
      TEST(coalesced),
      TEST(queue_owner),
      TEST(status_invalidation),
      TEST(repeat_commands_purge),
      TEST(serial_key),
      TEST(serial_cr),
      TEST(serial_lf),
      TEST(serial_unknown),
      TEST(serial_owner)};
  unsigned pass = 0, fail = 0;
  for (const auto &test : tests) {
    std::printf("RUN HOME %s\n", test.name);
    std::fflush(stdout);
    auto baseline = Memory::used;
    bool ok = test.run();
    ++assertions;
    ok = ok && Memory::used == baseline;
    std::printf("%s HOME %s\n", ok ? "PASS" : "FAIL", test.name);
    if (ok)
      ++pass;
    else
      ++fail;
  }
  std::printf("HOME SUMMARY PASS=%u FAIL=%u assertions=%u\n", pass, fail,
              assertions);
  return fail ? 1 : 0;
}

#include "control_home/facade.hpp"
#include "control_home/rtcp_device.hpp"
#include <cstdio>
#include <cstring>

namespace rtcp_client {
struct ClientTestAccess {
  static void configure(Client &client) {
    client.capabilities_ = {};
    client.capabilities_.volume = client.capabilities_.playback = true;
    client.capabilities_.volume_set = client.capabilities_.play = true;
    client.capabilities_.pause = client.capabilities_.previous = true;
    client.capabilities_.next = client.capabilities_.now_playing = true;
    client.capabilities_.revision = 2;
    client.capabilities_.max_age_ms = 2000;
    client.btx_capabilities_ = {1, 2000, 0x3f, 96, 127, true};
  }
  static wire::Snapshot &snapshot(Client &client) { return client.snapshot_; }
  static wire::NowPlaying &media(Client &client) { return client.now_playing_; }
};

Client::Event Client::poll(std::uint64_t, bool) { return Event::none; }
bool Client::set_volume(std::uint64_t, std::uint32_t, std::uint32_t) { return false; }
bool Client::playback(std::uint64_t, wire::PlaybackOperation, std::uint8_t,
                      std::uint64_t) { return false; }
bool Client::bluetooth(std::uint64_t, wire::PlaybackOperation,
                       wire::BtxPlayback, std::uint64_t) { return false; }
} // namespace rtcp_client

namespace control_home {
struct RtcpDeviceTestAccess {
  static bool event(RtcpDevice &device, rtcp_client::Client::Event event,
                    control::U64 now) {
    return device.handle(event, now);
  }
};
} // namespace control_home

namespace {
using namespace control_home;
using rtcp_client::Client;
unsigned passed = 0, failed = 0, assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { ++failed; std::printf("FAIL line=%d expr=%s\n", __LINE__, #x); return false; } } while (0)
#define TEST(x) do { if (x()) ++passed; } while (0)

struct Fixture {
  control::Store store;
  control::MockJournal journal;
  Client client;
  RtcpDevice device{client};
  control::Id d{"ownership.device"}, t{"ownership.target"}, b{"ownership.binding"};
  control::Core core{store, journal, control::Id("ownership.epoch")};
  static bool owner(void *) { return true; }
  control::ingress::Boundary boundary{core, owner, nullptr};
  Facade facade{core, d, t, b};
  std::uint64_t sequence = 0;

  bool initialize() {
    if (!device.install(core, boundary, d, t, b, 1)) return false;
    rtcp_client::ClientTestAccess::configure(client);
    return event(Client::Event::connected, 2) &&
           event(Client::Event::capabilities, 3);
  }
  bool event(Client::Event value, control::U64 now) {
    return RtcpDeviceTestAccess::event(device, value, now);
  }
  bool snapshot(std::uint8_t playback, std::uint8_t volume,
                std::uint64_t generation, control::U64 now) {
    auto &value = rtcp_client::ClientTestAccess::snapshot(client);
    value = {volume, playback, ++sequence, generation};
    return event(Client::Event::snapshot, now);
  }
  bool radio(std::uint8_t flags, std::uint64_t generation,
             std::uint64_t revision, control::U64 now,
             const char *station = "Radio ABF",
             const char *title = "Artist - Track") {
    auto &value = rtcp_client::ClientTestAccess::media(client);
    value = {};
    value.flags = flags;
    value.context = 1;
    value.station_size = std::uint8_t(std::strlen(station));
    value.combined_size = std::uint16_t(std::strlen(title));
    std::memcpy(value.station.data(), station, value.station_size);
    std::memcpy(value.combined.data(), title, value.combined_size);
    value.sequence = ++sequence;
    value.media_generation = generation;
    value.metadata_revision = revision;
    // The production combined-metadata path publishes station/title before an
    // explicit absent artist. Verify the resulting state rather than treating
    // the final absence admission result as the ownership oracle.
    (void)event(Client::Event::now_playing, now);
    return true;
  }
  bool btx_radio(std::uint64_t independent_generation,
                 std::uint64_t revision, control::U64 now) {
    auto &value = rtcp_client::ClientTestAccess::media(client);
    value = {};
    value.btx_source = std::uint8_t(rtcp_client::wire::BtxSource::radio);
    value.sequence = ++sequence;
    value.media_generation = independent_generation;
    value.metadata_revision = revision;
    (void)event(Client::Event::btx_state, now);
    return true;
  }
  bool bluetooth(std::uint8_t playback, std::uint8_t flags,
                 std::uint64_t independent_generation,
                 std::uint64_t revision, control::U64 now,
                 const char *artist = "BT Artist", const char *title = "BT Track") {
    auto &value = rtcp_client::ClientTestAccess::media(client);
    value = {};
    value.btx_source = std::uint8_t(rtcp_client::wire::BtxSource::bluetooth);
    value.btx_connection = std::uint8_t(rtcp_client::wire::BtxConnection::connected);
    value.btx_playback = playback;
    value.btx_flags = flags;
    value.artist_size = std::uint8_t(std::strlen(artist));
    value.title_size = std::uint8_t(std::strlen(title));
    std::memcpy(value.artist.data(), artist, value.artist_size);
    std::memcpy(value.title.data(), title, value.title_size);
    value.sequence = ++sequence;
    value.media_generation = independent_generation;
    value.metadata_revision = revision;
    (void)event(Client::Event::btx_state, now);
    return true;
  }
  Snapshot view() const { return facade.snapshot(); }
};

bool radio_ownership_and_repetition() {
  Fixture f; CHECK(f.initialize());
  CHECK(f.snapshot(0, 55, 7, 10));
  CHECK(f.btx_radio(900, 40, 11));
  CHECK(f.radio(0x23, 7, 1, 12));
  auto value = f.view();
  CHECK(value.playback == Playback::PLAYING && value.toggle);
  CHECK(value.volume_known && value.volume == 55);
  CHECK(std::strcmp(value.subtitle, "Radio ABF") == 0);
  CHECK(std::strcmp(value.title, "Artist - Track") == 0);
  for (unsigned i = 0; i < 8; ++i) {
    CHECK(f.snapshot(0, std::uint8_t(55 + i % 2), 7, 20 + i * 3));
    CHECK(f.btx_radio(900 + i, 50 + i, 21 + i * 3));
    CHECK(f.radio(0x23, 7, 2 + i, 22 + i * 3));
    value = f.view();
    CHECK(value.playback == Playback::PLAYING && value.toggle);
    CHECK(std::strcmp(value.subtitle, "Radio ABF") == 0);
  }
  return true;
}

bool radio_paused_and_stopped() {
  Fixture f; CHECK(f.initialize());
  CHECK(f.snapshot(1, 40, 3, 10));
  CHECK(f.btx_radio(70, 9, 11));
  CHECK(f.radio(0x13, 3, 1, 12));
  CHECK(f.view().playback == Playback::PAUSED && f.view().toggle);
  CHECK(f.snapshot(2, 40, 3, 20));
  CHECK(f.btx_radio(71, 10, 21));
  CHECK(f.radio(0x03, 3, 2, 22));
  CHECK(f.view().playback == Playback::OTHER && !f.view().toggle);
  return true;
}

bool bluetooth_and_bidirectional_handover() {
  Fixture f; CHECK(f.initialize());
  CHECK(f.snapshot(0, 63, 10, 10));
  CHECK(f.btx_radio(2, 1, 11));
  CHECK(f.radio(0x23, 10, 1, 12));
  CHECK(f.snapshot(0, 63, 11, 20));
  CHECK(f.bluetooth(std::uint8_t(rtcp_client::wire::BtxPlayback::playing),
                    0x0b, 1000, 800, 21));
  auto value = f.view();
  CHECK(value.playback == Playback::PLAYING && value.toggle);
  CHECK(std::strcmp(value.subtitle, "Bluetooth") == 0);
  CHECK(std::strcmp(value.title, "BT Artist - BT Track") == 0);

  // The D-009 generation is intentionally far below the independent BTX
  // generation. Returning to Radio must still admit the cached snapshot.
  CHECK(f.snapshot(0, 64, 12, 30));
  CHECK(f.btx_radio(3, 2, 31));
  value = f.view();
  CHECK(value.playback == Playback::PLAYING);
  CHECK(value.playback != Playback::UNKNOWN);
  CHECK(f.radio(0x23, 12, 1, 32, "Radio Return", "Fresh Radio Track"));
  value = f.view();
  CHECK(value.playback == Playback::PLAYING && value.toggle);
  CHECK(std::strcmp(value.subtitle, "Radio Return") == 0);
  CHECK(std::strcmp(value.title, "Fresh Radio Track") == 0);
  CHECK(value.volume_known && value.volume == 64);
  return true;
}

bool display_resume_and_session_invalidation() {
  Fixture f; CHECK(f.initialize());
  CHECK(f.snapshot(0, 71, 4, 10));
  CHECK(f.btx_radio(8000, 9000, 11));
  CHECK(f.radio(0x23, 4, 1, 12));
  const auto before = f.view();
  // DISPLAY_IDLE does not emit an adapter lifecycle event; rendering resumes
  // from the same authoritative Core snapshot.
  const auto after = f.view();
  CHECK(before.playback == Playback::PLAYING && after.playback == Playback::PLAYING);
  CHECK(after.toggle && after.volume_known && after.volume == 71);

  CHECK(f.event(Client::Event::disconnected, 20));
  CHECK(f.view().connection == Connection::UNAVAILABLE);
  CHECK(!f.view().toggle);
  CHECK(f.event(Client::Event::connected, 30));
  CHECK(f.event(Client::Event::capabilities, 31));
  CHECK(f.view().playback == Playback::UNKNOWN && !f.view().toggle);
  CHECK(f.snapshot(0, 72, 5, 32));
  CHECK(f.btx_radio(1, 1, 33));
  CHECK(f.radio(0x23, 5, 1, 34));
  CHECK(f.view().playback == Playback::PLAYING && f.view().toggle);
  CHECK(f.view().volume_known && f.view().volume == 72);
  return true;
}
} // namespace

int main() {
  TEST(radio_ownership_and_repetition);
  TEST(radio_paused_and_stopped);
  TEST(bluetooth_and_bidirectional_handover);
  TEST(display_resume_and_session_invalidation);
  std::printf("RTCP_OWNERSHIP: PASS=%u FAIL=%u assertions=%u\n",
              passed, failed, assertions);
  return failed ? 1 : 0;
}

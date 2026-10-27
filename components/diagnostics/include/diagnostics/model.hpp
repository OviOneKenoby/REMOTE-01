#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <string_view>
namespace diagnostics {
using U64 = std::uint64_t;
inline constexpr std::size_t response_bytes=6248;
struct RtcpCloseEvent {
  std::uint32_t uptime_ms=0,session=0,network_generation=0;
  std::int32_t socket_error=0;
  U64 request=0;
  std::uint8_t state=0,reason=0,awaited=0,response_pending=0;
};
static_assert(sizeof(RtcpCloseEvent)==32);
struct RtcpCloseHistory {
  std::array<RtcpCloseEvent,4> events{};
  std::uint8_t next=0,count=0;
  std::uint16_t reserved=0;
};
static_assert(sizeof(RtcpCloseHistory)==136);
struct RtcpDiagnostic {
  std::array<std::uint8_t,10> counters{};
  std::int16_t error=0;
  std::uint16_t timeout_queued_bytes=UINT16_MAX;
  std::uint8_t server_hello_bytes=0;
  std::uint8_t stage_failure=0;
  std::uint8_t close_reason=0;
  std::uint8_t close_count=0;
  std::uint8_t last_poll_32ms=0;
  std::uint8_t max_poll_32ms=0;
  std::uint8_t timeout_elapsed_32ms=0;
  std::uint8_t timeout_state=0;
  std::uint8_t timeout_unread_count=0;
  void set_stage_failure(std::uint8_t stage,std::uint8_t failure) {
    stage_failure=std::uint8_t((failure<<4)|(stage&0x0f));
  }
  std::uint8_t stage()const{return stage_failure&0x0f;}
  std::uint8_t failure()const{return stage_failure>>4;}
};
static_assert(sizeof(RtcpDiagnostic)==24);
struct BrowserDiagnostic {
  // Saturating boot-local counters followed by bounded correlation evidence.
  std::array<std::uint16_t,5> counters{};
  std::uint16_t mismatch_generation_low16=0;
  std::uint16_t mismatch_revision_low16=0;
  std::uint8_t ui_state=0;
  std::uint8_t pending_operation=0;
  static void increment(std::uint16_t& value) {
    if(value!=UINT16_MAX)++value;
  }
};
static_assert(sizeof(BrowserDiagnostic)==16);
struct Core {
  U64 time=0;
  // Existing resource/owner fields retain their 64-bit representation. The
  // high-rate control diagnostics are saturating boot-local counters.
  std::array<U64,27> values{};
  std::array<std::uint32_t,13> control_values{};
  // Stable order is documented by status_json: six encoder fields followed
  // by nine volume-pipeline fields. Compact storage preserves the 1 KiB cap.
  std::array<std::uint16_t,15> input_values{};
  RtcpDiagnostic rtcp;
  BrowserDiagnostic browser;
};
struct ExternalPower {unsigned raw=0,mv=0,state=0,valid=0,calibrated=0;std::int32_t error=0;U64 read_errors=0;};
struct Power { U64 time=0; std::array<U64,23> values{}; std::uint32_t resets[4][16]{}; ExternalPower external; };
struct Network { U64 time=0; std::array<U64,16> values{}; char ip[16]{}, ssid[33]{}; std::int8_t rssi=0; bool http_dns_off=false; };
struct Snapshots { Core core; Power power; Network network; };
static_assert(sizeof(Snapshots)<=1024);
enum class Event : unsigned { BOOT, NETWORK, NETWORK_ERROR, POWER, POWER_ERROR, CORE, CORE_REFUSED, REBOOT_REQUEST, REBOOT_REFUSED, REBOOT_EXECUTE, VOLUME_FAILURE, UI_POPUP_OPEN, UI_POPUP_CLOSE, UI_POPUP_CONTEXT };
struct Entry { U64 sequence=0,time=0; Event event{}; unsigned detail=0; };
static_assert(sizeof(Entry)==24);
struct Ring {
  std::array<Entry,32> slots{}; U64 next=1, overflow=0;
  void push(U64 time, Event event, unsigned detail);
  U64 oldest() const { return next>33?next-32:1; }
};
struct Address { std::uint32_t ip=0,mask=0; unsigned generation=0; bool ready=false; bool operator==(const Address&) const=default; };
bool normalize(bool ipv6,const unsigned char*,std::size_t,std::uint32_t&);
bool socket_allowed(Address,std::uint32_t local,std::uint32_t peer);
bool host_allowed(std::string_view,std::uint32_t ip);
bool cursor(std::string_view,U64&);
enum class Route { DENY, HOME, STATUS, EVENTS, RTCP_HISTORY };
Route route(std::string_view uri,bool get,std::size_t body);
struct Reboot {
  U64 due=0; bool pending=false;
  enum class Result { ACCEPT, COALESCE, REFUSE };
  Result request(U64 now,bool power_pending,bool writer_active);
  bool execute(U64 now,bool power_pending,bool writer_active);
};
// Checked JSON builder, no allocation and no format-string data injection.
struct Json {
  char *out; std::size_t capacity, used=0; bool ok=true;
  void text(std::string_view); void number(U64); void signed_number(int); void string(std::string_view);
};
bool status_json(const Snapshots&,U64 now,std::string_view project,std::string_view version,std::string_view sha,unsigned reset,char*,std::size_t);
} // namespace diagnostics

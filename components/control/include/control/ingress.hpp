#pragma once
#include "core.hpp"
#include <atomic>
#include <type_traits>
#include <variant>

// Bounded normalization profile, not a wire format or full CCM implementation.
// Producers see Inbox and value records only. Core owning objects stay on owner.
namespace control::ingress {
template <std::size_t N> struct Bytes {
  std::array<char, N + 1> data{};
  std::uint16_t size = 0;
  bool set(std::string_view s) {
    if (s.size() > N || !utf8(s)) return false;
    data = {};
    std::memcpy(data.data(), s.data(), s.size());
    size = static_cast<std::uint16_t>(s.size());
    return true;
  }
  bool valid() const { return size <= N && utf8(view()); }
  std::string_view view() const { return {data.data(), std::min<std::size_t>(size, N)}; }
  bool operator==(const Bytes &) const = default;
};
using Identifier = Bytes<128>;
struct Scope {
  Identifier epoch, binding;
  U64 incarnation = 0, session = 0, mapping = 0,
      device_association = 0, association = 0, revision = 0, media = 0;
  bool operator==(const Scope &) const = default;
};
struct Hop {
  Identifier actor, evidence;
  ProvenanceHop::Role role = ProvenanceHop::Role::TARGET;
};
// Narrow profile: scalar bool/integer/rational/string/power/playback/direction;
// reference/asset fields require a separately measured extension, never coercion.
struct Sample {
  Identifier id, field, sequence_domain;
  ScalarKind kind = ScalarKind::BOOL;
  bool present = false, boolean = false;
  Absence absence = Absence::UNKNOWN;
  U64 unsigned_value = 0;
  std::int64_t signed_value = 0;
  Rational rational;
  Bytes<4096> text;
  Power power = Power::OFF;
  Playback playback = Playback::NOT_PLAYING;
  Direction direction = Direction::UP;
  Confidence confidence = Confidence::UNKNOWN;
  std::array<Hop, 4> provenance{};
  unsigned hops = 0;
  U64 receipt = 0, acquisition = 0, ordinal = 0, max_age = 0;
  Origin origin = Origin::LIVE;
  ClockQuality clock_quality = ClockQuality::UNKNOWN;
  Optional<U64> observation_time, age, media;
  bool clock_contract_present = false;
  Absence clock_contract_absence = Absence::UNKNOWN;
  Bytes<256> clock_contract;
};
struct ProofKey {
  LockKey::Kind kind = LockKey::Kind::TARGET;
  Identifier device, target, resource;
};
struct Result {
  Identifier request, evidence_id;
  std::uint32_t attempt = 1;
  U64 mapping = 0, session = 0;
  AdapterEvidence::Signal signal = AdapterEvidence::Signal::PROGRESS;
  EvidenceKind evidence_kind = EvidenceKind::PROJECT_DESIGN;
  Bytes<256> reference;
  std::array<Bytes<128>, 4> claims{};
  unsigned claim_count = 0;
  bool version_present = false;
  Absence version_absence = Absence::UNKNOWN;
  Bytes<64> version;
  Optional<Delivery> delivery;
  Optional<Progress> progress;
  bool closed = false;
  bool failure_present = false;
  ErrorCode error = ErrorCode::UNAVAILABLE;
  Optional<RejectionOrigin> rejection_origin;
  Bytes<256> detail;
  std::array<ProofKey, Limits::resources + 1> keys{};
  unsigned key_count = 0;
};
struct CapabilityChange {
  Identifier id;
  CapabilityKey::Kind kind = CapabilityKey::Kind::OPERATION;
  Availability availability = Availability::UNKNOWN;
};
enum class Kind { SAMPLE, RESULT, REACHABILITY, RECONNECT, MEDIA, AVAILABILITY, WITHDRAW, SHUTDOWN };
struct Event {
  U64 queue_serial = 0; // inbox-owned local ordering, not an adapter identity
  Scope scope;
  Kind kind = Kind::SAMPLE;
  Reachability reachability = Reachability::UNKNOWN;
  std::variant<std::monostate, Sample, Result, CapabilityChange> payload;
};
static_assert(std::is_trivially_copyable_v<Event>);
enum class Status { APPLIED, POSTED, EMPTY, BUSY, FULL, CLOSED, WRONG_OWNER, STALE, INVALID, CORE_REJECTED, ALLOCATION_REFUSED, FAULT };
struct QueueCounts { std::uint32_t posted, rejected, overflow, busy; };
class Boundary;
class Inbox {
  friend class Boundary;
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
  std::atomic<bool> fault_{false};
  std::atomic<std::uint32_t> posted_{0}, rejected_{0}, overflow_{0}, busy_{0};
  std::array<Event, 8> queue_{};
  Scope scope_;
  unsigned head_ = 0, count_ = 0;
  bool open_ = false;
  U64 serial_ = 0;
  static void increment(std::atomic<std::uint32_t> &n) {
    auto old = n.load();
    for (unsigned i = 0; i < 4 && old != UINT32_MAX; ++i)
      if (n.compare_exchange_strong(old, old + 1)) return;
    if (old != UINT32_MAX) n.store(UINT32_MAX); // bounded contention => diagnostic saturation
  }
  bool lock() { return !lock_.test_and_set(std::memory_order_acquire); }
  void unlock() { lock_.clear(std::memory_order_release); }
public:
  static constexpr unsigned capacity = 8;
  Inbox() = default;
  Inbox(const Inbox &) = delete;
  Status post(const Event &, U64 *issued = nullptr);
  // Caller keeps this captured value; processing never stamps a new scope.
  bool scope(Scope &);
  QueueCounts counts() const { return {posted_.load(), rejected_.load(), overflow_.load(), busy_.load()}; }
};
struct OwnerCounts {
  U64 applied = 0, stale = 0, invalid = 0, refused = 0,
      lifecycle = 0, discarded = 0;
};
class Boundary {
  Core &core_;
  bool (*owner_)(void *);
  void *context_;
  Id binding_;
  U64 incarnation_ = 0;
  bool active_ = false;
  Event scratch_; // caller's persistent/PSRAM wrapper, never a task-stack event
  Scope current() const;
  bool owner() const { return owner_ && owner_(context_); }
  void refresh();
  bool matches(const Scope &, bool revision, bool media) const;
  Status apply(const Event &, U64);
public:
  Inbox inbox;
  OwnerCounts counts;
  Boundary(Core &c, bool (*check)(void *), void *context)
      : core_(c), owner_(check), context_(context) {}
  Boundary(const Boundary &) = delete;
  bool accepting() const { return owner() && active_ && !inbox.fault_.load(); }
  bool attach(const Id &, U64);
  bool shutdown(U64);
  Status process_one(U64);
  // Owner-only existing Core contract. No model duplication, guessed support
  // or producer-side Text/List allocation. Synchronous fast path shares gates.
  bool capability(CapabilityDescriptor, U64);
  bool withdraw(const CapabilityKey &, U64);
  bool reachability(Reachability, U64);
  bool reconnect(U64);
  bool media_changed(U64);
  bool observe(StateObservation, U64);
  bool report(AdapterEvidence, U64);
  // Owner-only bridges for the existing synthetic harness; no truncation.
  bool enqueue(StateObservation, U64);
  bool enqueue(AdapterEvidence, U64);
};
// Conversion functions are owner-only: output never borrows input storage.
bool pack(const StateObservation &, Sample &);
bool pack(const AdapterEvidence &, Result &);
} // namespace control::ingress

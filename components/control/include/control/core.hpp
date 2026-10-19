#pragma once
#include "model.hpp"
#include <array>

namespace control {
class Core;
struct AdapterEvidence {
  Id request_id, binding_id;
  std::uint32_t attempt_index = 1;
  U64 mapping_revision = 1, session_generation = 1;
  Evidence evidence;
  Optional<Delivery> delivery;
  Optional<Progress> progress;
  enum class Signal {
    PROGRESS,
    COMPLETE,
    REFUSE,
    FAIL,
    NO_EXECUTION,
    FUTURE_FENCE
  } signal = Signal::PROGRESS;
  bool closed = false;
  Optional<Error> failure;
  List<LockKey, Limits::resources + 1> covered_keys;
};
class AdapterSink {
public:
  virtual ~AdapterSink() = default;
  virtual bool publish(AdapterEvidence) = 0;
  virtual bool publish(StateObservation) = 0;
};
struct Dispatch {
  const CommandRequest &request;
  const Attempt &attempt;
  Optional<Rational> mapped_numeric;
};
class Adapter {
public:
  virtual ~Adapter() = default;
  virtual Id identity() const = 0;
  virtual Text version() const = 0;
  // May publish into the sink's bounded queue, never mutate Core reentrantly.
  virtual void dispatch(const Dispatch &, AdapterSink &) = 0;
};
struct AdapterRoute {
  Id binding_id;
  Adapter *adapter = nullptr;
};
struct SourceSample {
  Id target_id, binding_id;
  Text field_id;
  StateObservation observation;
  bool invalid = false;
  U64 resync_started = 0;
};
struct CatalogRecord {
  CatalogSnapshot snapshot;
  Id binding_id;
  U64 mapping_revision = 1, session_generation = 1, token_generation = 1;
};
struct EffectKey {
  LockKey key;
  Ordering mode = Ordering::STRICT;
  bool no_past = false, no_future = false;
};
struct Effect {
  Id request_id, binding_id, device_id, target_id;
  std::uint32_t attempt_index = 1;
  U64 mapping_revision = 1, session_generation = 1;
  List<EffectKey, Limits::resources + 1> keys;
  List<Id, 32> evidence_ids;
  bool complete = false, refused = false, uncertain_extent = false;
  Progress progress;
  Feedback feedback = Feedback::CORRELATED_RESULT;
  bool resolved() const {
    for (const auto &k : keys)
      if (!k.no_future)
        return false;
    return true;
  }
  bool zero() const {
    for (const auto &k : keys)
      if (!k.no_past || !k.no_future)
        return false;
    return true;
  }
};
struct Record {
  CommandRequest request;
  CommandResult result;
  ExecutionContract contract;
  List<LockKey, Limits::resources + 1> keys;
  bool stopped = false;
  Optional<Error> stop_error;
};
template <class T> class Owned {
  T *p_ = nullptr;

public:
  Owned() = default;
  Owned(const Owned &) = delete;
  Owned &operator=(const Owned &) = delete;
  Owned(Owned &&o) noexcept { std::swap(p_, o.p_); }
  Owned &operator=(Owned &&o) noexcept {
    reset();
    std::swap(p_, o.p_);
    return *this;
  }
  ~Owned() { reset(); }
  void reset() {
    if (p_) {
      p_->~T();
      Memory::release(p_);
      p_ = nullptr;
    }
  }
  bool create() {
    p_ = static_cast<T *>(Memory::allocate(sizeof(T)));
    if (p_)
      new (p_) T;
    return p_;
  }
  T *get() { return p_; }
  const T *get() const { return p_; }
};
// Caller-owned decision state survives a synthetic Core restart. It is not NVS.
struct Store {
  List<CommonDevice, Limits::devices> devices;
  List<Target, Limits::targets> targets;
  List<Binding, Limits::bindings> bindings;
  List<AdapterRoute, Limits::bindings> routes;
  List<SharedResource, Limits::resources> resources;
  List<SourceSample, Limits::targets * Limits::fields * Limits::bindings>
      samples;
  List<CatalogRecord, Limits::targets * 5> catalogs;
  List<std::pair<Id, SeekWindow>, Limits::targets> windows;
  std::array<Owned<Record>, Limits::records> records;
  List<Effect, Limits::barriers> effects;
  Id clock_epoch;
  U64 now = 0, ingress = 0, watermark = 0, lifetime_counter = 0, epoch_base = 0;
  bool blocked = false;
};
class Journal {
public:
  virtual ~Journal() = default;
  // Must durably commit the decision state before any executable handover.
  // The local mock implementation preserves Store in process only.
  virtual bool commit(const Store &) = 0;
};
struct Reply {
  const CommandResult *result = nullptr;
  Optional<Error> error;
};
class Core final : public AdapterSink {
  Store &s_;
  Journal &journal_;
  CommandResult refusal_;
  Reply refused(const CommandRequest &, const Error &);
  struct Event {
    bool state = false;
    AdapterEvidence evidence;
    StateObservation observation;
  };
  List<Event, Limits::events> events_;
  bool processing_ = false;
  void increment(U64 &value) {
    if (value == UINT64_MAX)
      s_.blocked = true;
    else
      ++value;
  }
  bool begin(U64);
  void end();
  bool persist();
  CapabilityDescriptor *descriptor(Target &, const Id &, std::string_view,
                                   CapabilityKey::Kind);
  const CapabilityDescriptor *descriptor(const Target &, const Id &,
                                         std::string_view,
                                         CapabilityKey::Kind) const;
  Optional<Error> validate(const CommandRequest &, bool handover);
  void fold(Record &);
  void close(Record &, Lifecycle, Outcome, Value<Error>);
  void schedule();
  void evidence(const AdapterEvidence &);
  bool ingest(StateObservation);
  void reduce(Target &, std::string_view);
  bool fresh(const StateObservation &) const;
  void maintenance();
  void revalidate();
  void purge();
  void update_connectivity();
  List<LockKey, Limits::resources + 1> keys(const CommandRequest &);
  bool blocked(const List<LockKey, Limits::resources + 1> &, const Id &) const;
  Effect *effect(const Id &, std::uint32_t);
  Record *record(const Id &);
  bool known_operation(std::string_view) const;
  bool schema(const CommandRequest &) const;
  Id expected_id(U64) const;
  bool matches_id(const Id &, U64) const;

public:
  Core(Store &, Journal &, Id epoch);
  Core(const Core &) = delete;
  Core &operator=(const Core &) = delete;
  const Store &storage() const { return s_; }
  Target *target(const Id &, const Id &);
  Binding *binding(const Id &);
  CommonDevice *device(const Id &);
  Optional<Error> register_device(CommonDevice, U64 now);
  Optional<Error> register_target(Target, U64 now);
  Optional<Error> register_binding(Binding, Adapter &, U64 now);
  Optional<Error> register_resource(SharedResource, U64 now);
  Optional<Error> capability(const Id &, const Id &, CapabilityDescriptor,
                             U64 now);
  Optional<Error> reconnect(const Id &, U64 now);
  Optional<Error> reachability(const Id &, Reachability, U64 now);
  Optional<Error> withdraw(const Id &, const Id &, const CapabilityKey &, U64 now);
  Optional<Error> media_changed(const Id &, const Id &, U64 now);
  U64 begin_resync(const Id &, std::string_view, U64 now);
  Optional<Error> catalog(CatalogSnapshot, const Id &, U64 mapping, U64 session,
                          U64 now);
  Optional<Error> seek_window(const Id &, SeekWindow, U64 now);
  // Constructs a proposal using the next Core-owned pair; submission commits
  // it.
  Reply capture(CommandRequest &, const Id &, const Id &, std::string_view,
                const Arguments &, U64 deadline, U64 now);
  Reply submit(const CommandRequest &, U64 now);
  Reply cancel(const Id &, U64 now);
  void tick(U64 now);
  const CommandResult *result(const Id &) const;
  const FieldSnapshot *field(const Id &, const Id &, std::string_view) const;
  bool observe(StateObservation, U64 now);
  bool report(AdapterEvidence, U64 now);
  bool publish(AdapterEvidence) override;
  bool publish(StateObservation) override;
};
} // namespace control

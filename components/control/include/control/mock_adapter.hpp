#pragma once
#include "core.hpp"

namespace control {
class MockJournal final : public Journal {
public:
  bool writable = true;
  std::size_t commits = 0;
  bool commit(const Store &) override {
    ++commits;
    return writable;
  }
};
class MockAdapter final : public Adapter {
public:
  enum class Mode {
    CONFIRMED,
    OPEN_LOOP,
    REJECT,
    UNAVAILABLE,
    DELAYED,
    NO_RESULT,
    PARTIAL,
    ONGOING_PARTIAL,
    AMBIGUOUS,
    NO_EXECUTION
  };
  Mode mode = Mode::CONFIRMED;
  std::uint32_t dispatches = 0;
  Optional<AdapterEvidence> last;
  Optional<Rational> last_numeric;
  Id identity() const override { return Id("adapter.mock"); }
  Text version() const override { return Text::literal("mock.1"); }
  void dispatch(const Dispatch &, AdapterSink &) override;
  bool install(Core &, const Id &device, const Id &target, const Id &binding,
               Feedback = Feedback::CORRELATED_RESULT,
               Ordering = Ordering::STRICT, U64 now = 0);
  static CapabilityDescriptor operation(const Id &, std::string_view, Feedback);
  static CapabilityDescriptor readable(const Id &, std::string_view);
  StateObservation sample(Core &, const Id &, std::string_view, Value<Scalar>,
                          U64 now, Origin = Origin::LIVE);
  AdapterEvidence completion() const;
  AdapterEvidence proof(const List<LockKey, Limits::resources + 1> &,
                        bool past) const;

private:
  U64 serial_ = 0;
  Id next(const char *);
};
} // namespace control

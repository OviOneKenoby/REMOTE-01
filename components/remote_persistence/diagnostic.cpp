#include "remote_persistence/diagnostic.hpp"

#include <cstdio>

namespace remote_persistence::diagnostic {
namespace {

constexpr std::string_view kPrefix = "!A010 ";
constexpr std::string_view kInitToken = "CONFIRM-A010-REMOTE-CTL-INIT";
constexpr std::string_view kCommit1Token = "CONFIRM-A010-REMOTE-CTL-COMMIT1";
constexpr std::string_view kCommit2Token = "CONFIRM-A010-REMOTE-CTL-COMMIT2";
constexpr std::string_view kEraseToken = "CONFIRM-A010-REMOTE-CTL-ERASE";

bool exact_partition(const View& view) {
    return view.partition_exists && view.type == 1 && view.subtype == 2 &&
           view.address == kExpectedAddress && view.size == kExpectedSize;
}

std::string format(const char* operation, const View& view, bool accepted) {
    char buffer[384]{};
    std::snprintf(buffer, sizeof buffer,
                  "op=%s accepted=%u exists=%u type=%u subtype=%u offset=0x%08lx size=0x%zx state=%s schema=%u generation=%llu next=%llu incomplete=%u used_entries=%zu free_entries=%zu available_entries=%zu payload_logged=0",
                  operation, unsigned(accepted), unsigned(view.partition_exists),
                  unsigned(view.type), unsigned(view.subtype),
                  static_cast<unsigned long>(view.address), view.size,
                  media_state_name(view.state), unsigned(kSchemaVersion),
                  static_cast<unsigned long long>(view.generation),
                  static_cast<unsigned long long>(view.next_generation),
                  unsigned(view.incomplete), view.used_entries,
                  view.free_entries, view.available_entries);
    return buffer;
}

std::string transition(const char* operation, const View& before, const View& after, bool accepted) {
    char buffer[448]{};
    std::snprintf(buffer, sizeof buffer,
                  "op=%s accepted=%u before_state=%s before_generation=%llu before_next=%llu before_incomplete=%u after_state=%s after_generation=%llu after_next=%llu after_incomplete=%u used_entries=%zu free_entries=%zu available_entries=%zu payload_logged=0",
                  operation, unsigned(accepted), media_state_name(before.state),
                  static_cast<unsigned long long>(before.generation),
                  static_cast<unsigned long long>(before.next_generation),
                  unsigned(before.incomplete), media_state_name(after.state),
                  static_cast<unsigned long long>(after.generation),
                  static_cast<unsigned long long>(after.next_generation),
                  unsigned(after.incomplete), after.used_entries,
                  after.free_entries, after.available_entries);
    return buffer;
}

Reply reject(std::string_view operation, const char* reason, bool mutating = false) {
    return {true, false, mutating,
            std::string{"op="} + std::string(operation) + " accepted=0 reason=" + reason + " payload_logged=0"};
}

}  // namespace

const char* media_state_name(MediaState state) {
    switch (state) {
        case MediaState::missing: return "missing";
        case MediaState::wrong_partition: return "wrong_partition";
        case MediaState::erased: return "erased";
        case MediaState::initialized_empty: return "initialized_empty";
        case MediaState::valid: return "valid_recoverable";
        case MediaState::corrupt: return "corrupt";
        case MediaState::ambiguous: return "ambiguous";
        case MediaState::schema_mismatch: return "schema_mismatch";
        case MediaState::storage_error: return "storage_error";
    }
    return "unknown";
}

Reply Engine::execute(std::string_view line) {
    if (!enabled_) return {false, false, false, "diagnostic_disabled"};
    if (!line.starts_with(kPrefix)) return {false, false, false, "not_diagnostic"};
    const auto command = line.substr(kPrefix.size());
    if (command == "HELP") {
        return {true, true, false,
                "commands=HELP,INFO,STATUS,RECOVER,INIT,COMMIT1,COMMIT2,ERASE mutations_require_exact_token payload_logged=0"};
    }
    if (command == "INFO") {
        const auto view = target_.inspect(false);
        return {true, exact_partition(view), false, format("INFO", view, exact_partition(view))};
    }
    if (command == "STATUS" || command == "RECOVER") {
        const auto view = target_.inspect(true);
        const bool accepted = exact_partition(view) && view.state != MediaState::storage_error;
        return {true, accepted, false, format(command == "STATUS" ? "STATUS" : "RECOVER", view, accepted)};
    }

    const auto split = command.find(' ');
    const auto verb = command.substr(0, split);
    const auto token = split == std::string_view::npos ? std::string_view{} : command.substr(split + 1);
    const bool is_mutation = verb == "INIT" || verb == "COMMIT1" || verb == "COMMIT2" || verb == "ERASE";
    if (!is_mutation) return reject("UNKNOWN", "unknown_command");
    const auto expected = verb == "INIT" ? kInitToken : verb == "COMMIT1" ? kCommit1Token : verb == "COMMIT2" ? kCommit2Token : kEraseToken;
    if (token != expected) return reject(verb, "confirmation_token_rejected", true);

    const auto before = target_.inspect(true);
    if (!exact_partition(before)) return reject(verb, "partition_identity_rejected", true);
    if (verb == "INIT") {
        if (before.state != MediaState::erased) return reject("INIT", "requires_erased_partition", true);
        const auto after = target_.initialize();
        const bool accepted = after.state == MediaState::initialized_empty;
        return {true, accepted, true, transition("INIT", before, after, accepted)};
    }
    if (verb == "COMMIT1") {
        if (before.state != MediaState::initialized_empty || before.generation != 0 || before.next_generation != 1 || before.incomplete)
            return reject("COMMIT1", "requires_clean_initialized_generation_0", true);
        const auto after = target_.commit_known(1);
        const bool accepted = after.state == MediaState::valid && after.generation == 1;
        return {true, accepted, true, transition("COMMIT1", before, after, accepted)};
    }
    if (verb == "COMMIT2") {
        if (before.state != MediaState::valid || before.generation != 1 || before.next_generation != 2 || before.incomplete)
            return reject("COMMIT2", "requires_clean_generation_1", true);
        const auto after = target_.commit_known(2);
        const bool accepted = after.state == MediaState::valid && after.generation == 2;
        return {true, accepted, true, transition("COMMIT2", before, after, accepted)};
    }
    const auto after = target_.erase_partition();
    const bool accepted = after.state == MediaState::erased;
    return {true, accepted, true, transition("ERASE", before, after, accepted)};
}

}  // namespace remote_persistence::diagnostic

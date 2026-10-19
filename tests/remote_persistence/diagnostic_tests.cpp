#include "remote_persistence/diagnostic.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace remote_persistence::diagnostic;

namespace {

struct FakeTarget final : Target {
    View view{true, 1, 2, kExpectedAddress, kExpectedSize, MediaState::erased};
    unsigned inspections{0};
    unsigned initializations{0};
    unsigned commits{0};
    unsigned erases{0};

    View inspect(bool) override { ++inspections; return view; }
    View initialize() override {
        ++initializations;
        view.state = MediaState::initialized_empty;
        view.generation = 0;
        view.next_generation = 1;
        view.used_entries = 3;
        view.free_entries = 120;
        view.available_entries = 120;
        return view;
    }
    View commit_known(unsigned payload_number) override {
        ++commits;
        view.state = MediaState::valid;
        view.generation = payload_number;
        view.next_generation = payload_number + 1;
        view.used_entries += 2;
        view.free_entries -= 2;
        view.available_entries -= 2;
        return view;
    }
    View erase_partition() override {
        ++erases;
        view.state = MediaState::erased;
        view.generation = 0;
        view.next_generation = 0;
        view.used_entries = view.free_entries = view.available_entries = 0;
        return view;
    }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_disabled_state() {
    FakeTarget target;
    const auto reply = Engine(target, false).execute("!A010 STATUS");
    require(!reply.recognized && !reply.accepted && target.inspections == 0, "disabled must be inert");
}

void test_command_parsing() {
    FakeTarget target; Engine engine(target, true);
    require(engine.execute("ordinary input").recognized == false, "ordinary input");
    require(engine.execute("!A010 HELP").accepted, "help");
    require(!engine.execute("!A010 UNKNOWN").accepted, "unknown");
}

void test_confirmation_rejection() {
    FakeTarget target; Engine engine(target, true);
    const auto reply = engine.execute("!A010 INIT wrong-token");
    require(reply.recognized && reply.mutating && !reply.accepted && target.initializations == 0, "token rejection");
}

void test_partition_identity_guard() {
    FakeTarget target; target.view.address = 0xE10000; Engine engine(target, true);
    const auto reply = engine.execute("!A010 INIT CONFIRM-A010-REMOTE-CTL-INIT");
    require(!reply.accepted && target.initializations == 0, "address guard");
}

void test_read_only_erased() {
    FakeTarget target; Engine engine(target, true);
    const auto reply = engine.execute("!A010 STATUS");
    require(reply.accepted && reply.text.find("state=erased") != std::string::npos && target.initializations == 0, "erased status");
}

void test_read_only_valid() {
    FakeTarget target; target.view.state = MediaState::valid; target.view.generation = 7; target.view.next_generation = 8;
    const auto reply = Engine(target, true).execute("!A010 RECOVER");
    require(reply.accepted && reply.text.find("state=valid_recoverable") != std::string::npos && reply.text.find("generation=7") != std::string::npos, "valid status");
}

void test_read_only_corrupt() {
    FakeTarget target; target.view.state = MediaState::corrupt;
    const auto reply = Engine(target, true).execute("!A010 STATUS");
    require(reply.recognized && reply.text.find("state=corrupt") != std::string::npos, "corrupt status");
}

void test_read_only_ambiguous() {
    FakeTarget target; target.view.state = MediaState::ambiguous;
    const auto reply = Engine(target, true).execute("!A010 STATUS");
    require(reply.recognized && reply.text.find("state=ambiguous") != std::string::npos, "ambiguous status");
}

void test_init_sequence() {
    FakeTarget target; Engine engine(target, true);
    const auto reply = engine.execute("!A010 INIT CONFIRM-A010-REMOTE-CTL-INIT");
    require(reply.accepted && target.initializations == 1 && target.view.state == MediaState::initialized_empty, "init");
}

void test_commit_sequence() {
    FakeTarget target; Engine engine(target, true);
    require(engine.execute("!A010 INIT CONFIRM-A010-REMOTE-CTL-INIT").accepted, "init");
    require(engine.execute("!A010 COMMIT1 CONFIRM-A010-REMOTE-CTL-COMMIT1").accepted, "commit1");
    require(engine.execute("!A010 COMMIT2 CONFIRM-A010-REMOTE-CTL-COMMIT2").accepted, "commit2");
    require(target.commits == 2 && target.view.generation == 2, "commit sequence");
}

void test_out_of_sequence_commit_rejected() {
    FakeTarget target; Engine engine(target, true);
    const auto reply = engine.execute("!A010 COMMIT2 CONFIRM-A010-REMOTE-CTL-COMMIT2");
    require(!reply.accepted && target.commits == 0, "unsafe successor");
}

void test_erase_guards() {
    FakeTarget target; Engine engine(target, true);
    require(!engine.execute("!A010 ERASE no").accepted && target.erases == 0, "erase token");
    target.view.size = 0xC000;
    require(!engine.execute("!A010 ERASE CONFIRM-A010-REMOTE-CTL-ERASE").accepted && target.erases == 0, "erase partition guard");
}

void test_explicit_erase() {
    FakeTarget target; target.view.state = MediaState::valid; target.view.generation = 2; Engine engine(target, true);
    const auto reply = engine.execute("!A010 ERASE CONFIRM-A010-REMOTE-CTL-ERASE");
    require(reply.accepted && target.erases == 1 && target.view.state == MediaState::erased, "explicit erase");
}

void test_no_payload_logging() {
    FakeTarget target; Engine engine(target, true);
    require(engine.execute("!A010 INIT CONFIRM-A010-REMOTE-CTL-INIT").accepted, "init");
    const auto reply = engine.execute("!A010 COMMIT1 CONFIRM-A010-REMOTE-CTL-COMMIT1");
    require(reply.text.find("A010-DIAG-RECORD") == std::string::npos && reply.text.find("payload_logged=0") != std::string::npos, "payload leak");
}

}  // namespace

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"diagnostic_disabled_state", test_disabled_state},
        {"command_parsing", test_command_parsing},
        {"confirmation_token_rejection", test_confirmation_rejection},
        {"exact_partition_identity_guard", test_partition_identity_guard},
        {"read_only_erased_status", test_read_only_erased},
        {"read_only_valid_status", test_read_only_valid},
        {"read_only_corrupt_status", test_read_only_corrupt},
        {"read_only_ambiguous_status", test_read_only_ambiguous},
        {"explicit_initialization_sequence", test_init_sequence},
        {"first_successor_commit_sequence", test_commit_sequence},
        {"out_of_sequence_commit_rejection", test_out_of_sequence_commit_rejected},
        {"unsafe_erase_rejection", test_erase_guards},
        {"explicit_erase_command", test_explicit_erase},
        {"no_payload_logging", test_no_payload_logging},
    };
    int passed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test(); ++passed; std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    std::cout << "TOTAL " << passed << '/' << std::size(tests) << " PASS\n";
    return EXIT_SUCCESS;
}

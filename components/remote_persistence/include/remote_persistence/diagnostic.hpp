#pragma once

#include "remote_persistence/backend.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#ifndef A010_REMOTE_PERSISTENCE_DIAGNOSTIC
#define A010_REMOTE_PERSISTENCE_DIAGNOSTIC 0
#endif

namespace remote_persistence::diagnostic {

inline constexpr bool kBuildEnabled = A010_REMOTE_PERSISTENCE_DIAGNOSTIC == 1;
inline constexpr std::uint32_t kExpectedAddress = 0xE20000;
inline constexpr std::size_t kExpectedSize = 0x10000;

enum class MediaState {
    missing,
    wrong_partition,
    erased,
    initialized_empty,
    valid,
    corrupt,
    ambiguous,
    schema_mismatch,
    storage_error,
};

struct View {
    bool partition_exists{false};
    std::uint8_t type{0};
    std::uint8_t subtype{0};
    std::uint32_t address{0};
    std::size_t size{0};
    MediaState state{MediaState::missing};
    std::uint64_t generation{0};
    std::uint64_t next_generation{0};
    bool incomplete{false};
    std::size_t used_entries{0};
    std::size_t free_entries{0};
    std::size_t available_entries{0};
};

class Target {
public:
    virtual ~Target() = default;
    virtual View inspect(bool recover_backend) = 0;
    virtual View initialize() = 0;
    virtual View commit_known(unsigned payload_number) = 0;
    virtual View erase_partition() = 0;
};

struct Reply {
    bool recognized{false};
    bool accepted{false};
    bool mutating{false};
    std::string text;
};

class Engine {
public:
    explicit Engine(Target& target, bool enabled = kBuildEnabled) : target_(target), enabled_(enabled) {}
    Reply execute(std::string_view line);

private:
    Target& target_;
    bool enabled_;
};

const char* media_state_name(MediaState state);

// Candidate-only hook for the existing single USB-serial reader. Returns true
// only while consuming a line that begins with the reserved '!A010 ' prefix.
bool consume_uart_byte(std::uint8_t byte);
void announce();

}  // namespace remote_persistence::diagnostic

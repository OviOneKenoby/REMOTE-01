#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace remote_persistence {

inline constexpr char kPartitionLabel[] = "remote_ctl";
inline constexpr char kNamespace[] = "ctl_v1";
inline constexpr std::uint16_t kSchemaVersion = 1;
inline constexpr std::size_t kMaxPayloadBytes = 4096;
inline constexpr std::size_t kMaxRecordCount = 3;

enum class StoreResult { ok, not_found, no_space, version_mismatch, io_error };

class Store {
public:
    virtual ~Store() = default;
    virtual StoreResult list_keys(std::vector<std::string>& keys) = 0;
    virtual StoreResult get(const std::string& key, std::vector<std::uint8_t>& value) = 0;
    virtual StoreResult set(const std::string& key, std::span<const std::uint8_t> value) = 0;
    virtual StoreResult commit() = 0;
};

enum class Status {
    ok,
    empty,
    uninitialized,
    corrupt,
    ambiguous,
    schema_mismatch,
    capacity_exhausted,
    replay_rejected,
    storage_error,
};

struct Recovery {
    Status status{Status::storage_error};
    std::uint64_t active_generation{0};
    std::uint64_t next_generation{0};
    bool incomplete_transaction{false};
    std::vector<std::uint8_t> payload;
};

class Backend {
public:
    explicit Backend(Store& store) : store_(store) {}

    // Initialization is explicit. Merely linking or constructing this class writes nothing.
    Status initialize_erased();
    Recovery recover();
    Status commit(std::uint64_t generation, std::span<const std::uint8_t> payload);

private:
    Store& store_;
};

const char* status_name(Status status);

}  // namespace remote_persistence

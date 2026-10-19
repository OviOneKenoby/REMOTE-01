#include "remote_persistence/backend.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>

extern "C" void remote_persistence_backend_anchor() {}

namespace remote_persistence {
namespace {

constexpr std::uint32_t kSchemaMagic = 0x31534352U;  // RCS1
constexpr std::uint32_t kRootMagic = 0x31544f52U;    // ROT1
constexpr std::uint32_t kFenceMagic = 0x31434e46U;   // FNC1
constexpr std::uint32_t kRecordMagic = 0x31434552U;  // REC1
constexpr std::size_t kSchemaBytes = 16;
constexpr std::size_t kRootBytes = 32;
constexpr std::size_t kFenceBytes = 24;
constexpr std::size_t kRecordHeaderBytes = 28;
constexpr std::uint8_t kNoSlot = 0xff;

void put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}
void put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::uint16_t get16(std::span<const std::uint8_t> in, std::size_t at) {
    return static_cast<std::uint16_t>(in[at]) | static_cast<std::uint16_t>(in[at + 1]) << 8U;
}
std::uint32_t get32(std::span<const std::uint8_t> in, std::size_t at) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(in[at + i]) << (i * 8U);
    return value;
}
std::uint64_t get64(std::span<const std::uint8_t> in, std::size_t at) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(in[at + i]) << (i * 8U);
    return value;
}
std::uint32_t crc32(std::span<const std::uint8_t> data) {
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : data) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
void finish_crc(std::vector<std::uint8_t>& value) { put32(value, crc32(value)); }
bool valid_crc(std::span<const std::uint8_t> value) {
    return value.size() >= 4 && get32(value, value.size() - 4) == crc32(value.first(value.size() - 4));
}
std::string record_key(std::size_t slot) {
    return std::string{"rec"} + static_cast<char>('0' + slot / 10) + static_cast<char>('0' + slot % 10);
}
bool record_slot(const std::string& key, std::size_t& slot) {
    if (key.size() != 5 || key[0] != 'r' || key[1] != 'e' || key[2] != 'c' || key[3] < '0' || key[3] > '9' || key[4] < '0' || key[4] > '9') return false;
    slot = static_cast<std::size_t>(key[3] - '0') * 10 + static_cast<std::size_t>(key[4] - '0');
    return slot < kMaxRecordCount;
}
std::vector<std::uint8_t> schema_value() {
    std::vector<std::uint8_t> out; out.reserve(kSchemaBytes);
    put32(out, kSchemaMagic); put16(out, kSchemaVersion); put16(out, static_cast<std::uint16_t>(kMaxPayloadBytes));
    put16(out, static_cast<std::uint16_t>(kMaxRecordCount)); put16(out, 0); finish_crc(out); return out;
}
std::vector<std::uint8_t> root_value(std::uint64_t generation, std::uint8_t slot, std::uint32_t record_crc) {
    std::vector<std::uint8_t> out; out.reserve(kRootBytes);
    put32(out, kRootMagic); put16(out, kSchemaVersion); put16(out, 0); put64(out, generation);
    out.push_back(slot); for (unsigned i = 0; i < 7; ++i) out.push_back(0); put32(out, record_crc); finish_crc(out); return out;
}
std::vector<std::uint8_t> fence_value(std::uint64_t generation) {
    std::vector<std::uint8_t> out; out.reserve(kFenceBytes);
    put32(out, kFenceMagic); put16(out, kSchemaVersion); put16(out, 0); put64(out, generation); put32(out, 0); finish_crc(out); return out;
}
std::vector<std::uint8_t> record_value(std::uint64_t generation, std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> out; out.reserve(kRecordHeaderBytes + payload.size());
    put32(out, kRecordMagic); put16(out, kSchemaVersion); put16(out, static_cast<std::uint16_t>(kRecordHeaderBytes));
    put64(out, generation); put32(out, static_cast<std::uint32_t>(payload.size())); put32(out, crc32(payload));
    out.insert(out.end(), payload.begin(), payload.end()); finish_crc(out); return out;
}
struct DecodedRecord { std::uint64_t generation; std::uint32_t record_crc; std::vector<std::uint8_t> payload; };
bool decode_record(std::span<const std::uint8_t> in, DecodedRecord& out, bool& schema_bad) {
    schema_bad = false;
    if (in.size() < kRecordHeaderBytes || !valid_crc(in) || get32(in, 0) != kRecordMagic || get16(in, 6) != kRecordHeaderBytes) return false;
    if (get16(in, 4) != kSchemaVersion) { schema_bad = true; return false; }
    const auto size = get32(in, 16);
    if (size > kMaxPayloadBytes || in.size() != kRecordHeaderBytes + size) return false;
    const auto payload = in.subspan(24, size);
    if (get32(in, 20) != crc32(payload)) return false;
    out = {get64(in, 8), get32(in, in.size() - 4), {payload.begin(), payload.end()}}; return out.generation != 0;
}
StoreResult fetch(Store& store, const char* key, std::vector<std::uint8_t>& out) { return store.get(key, out); }
Status store_failure(StoreResult result) { return result == StoreResult::no_space ? Status::capacity_exhausted : result == StoreResult::version_mismatch ? Status::schema_mismatch : Status::storage_error; }

}  // namespace

Status Backend::initialize_erased() {
    std::vector<std::string> keys;
    const auto listed = store_.list_keys(keys);
    if (listed != StoreResult::ok) return store_failure(listed);
    if (!keys.empty()) return Status::ambiguous;
    const auto schema = schema_value(); const auto root = root_value(0, kNoSlot, 0); const auto fence = fence_value(0);
    for (const auto& item : std::array<std::pair<const char*, const std::vector<std::uint8_t>*>, 3>{{{"schema", &schema}, {"root", &root}, {"fence", &fence}}}) {
        const auto result = store_.set(item.first, *item.second); if (result != StoreResult::ok) return store_failure(result);
    }
    const auto result = store_.commit(); return result == StoreResult::ok ? Status::ok : store_failure(result);
}

Recovery Backend::recover() {
    Recovery result;
    std::vector<std::string> keys;
    auto store_result = store_.list_keys(keys);
    if (store_result != StoreResult::ok) { result.status = store_failure(store_result); return result; }
    if (keys.empty()) { result.status = Status::empty; result.next_generation = 1; return result; }
    std::set<std::string> unique_keys(keys.begin(), keys.end());
    if (unique_keys.size() != keys.size()) { result.status = Status::ambiguous; return result; }
    for (const auto& key : keys) { std::size_t ignored = 0; if (key != "schema" && key != "root" && key != "fence" && !record_slot(key, ignored)) { result.status = Status::ambiguous; return result; } }
    std::vector<std::uint8_t> schema, root, fence;
    if (fetch(store_, "schema", schema) != StoreResult::ok || fetch(store_, "root", root) != StoreResult::ok || fetch(store_, "fence", fence) != StoreResult::ok) { result.status = Status::corrupt; return result; }
    if (schema.size() != kSchemaBytes || !valid_crc(schema) || get32(schema, 0) != kSchemaMagic) { result.status = Status::corrupt; return result; }
    if (get16(schema, 4) != kSchemaVersion || get16(schema, 6) != kMaxPayloadBytes || get16(schema, 8) != kMaxRecordCount) { result.status = Status::schema_mismatch; return result; }
    if (root.size() != kRootBytes || !valid_crc(root) || get32(root, 0) != kRootMagic) { result.status = Status::corrupt; return result; }
    if (fence.size() != kFenceBytes || !valid_crc(fence) || get32(fence, 0) != kFenceMagic) { result.status = Status::corrupt; return result; }
    if (get16(root, 4) != kSchemaVersion || get16(fence, 4) != kSchemaVersion) { result.status = Status::schema_mismatch; return result; }
    const auto root_generation = get64(root, 8); const auto root_slot = root[16]; const auto root_crc = get32(root, 24); const auto fence_generation = get64(fence, 8);
    if (fence_generation < root_generation) { result.status = Status::ambiguous; return result; }
    std::set<std::uint64_t> generations; std::vector<DecodedRecord> records(kMaxRecordCount); std::array<bool, kMaxRecordCount> present{};
    for (const auto& key : keys) {
        std::size_t slot = 0; if (!record_slot(key, slot)) continue;
        std::vector<std::uint8_t> raw; if (store_.get(key, raw) != StoreResult::ok) { result.status = Status::corrupt; return result; }
        bool schema_bad = false; if (!decode_record(raw, records[slot], schema_bad)) { result.status = schema_bad ? Status::schema_mismatch : Status::corrupt; return result; }
        if (!generations.insert(records[slot].generation).second) { result.status = Status::ambiguous; return result; }
        if (records[slot].generation > fence_generation) { result.status = Status::ambiguous; return result; }
        present[slot] = true;
    }
    if (root_generation == 0) {
        if (root_slot != kNoSlot || root_crc != 0) { result.status = Status::corrupt; return result; }
    } else {
        if (root_slot >= kMaxRecordCount || !present[root_slot] || records[root_slot].generation != root_generation || records[root_slot].record_crc != root_crc) { result.status = Status::ambiguous; return result; }
        result.payload = records[root_slot].payload;
    }
    result.active_generation = root_generation;
    result.incomplete_transaction = fence_generation > root_generation;
    for (std::size_t slot = 0; slot < kMaxRecordCount; ++slot) {
        if (present[slot] && records[slot].generation > root_generation) result.incomplete_transaction = true;
    }
    if (fence_generation == std::numeric_limits<std::uint64_t>::max()) { result.status = Status::capacity_exhausted; return result; }
    result.next_generation = fence_generation + 1;
    result.status = Status::ok; return result;
}

Status Backend::commit(std::uint64_t generation, std::span<const std::uint8_t> payload) {
    if (payload.size() > kMaxPayloadBytes) return Status::capacity_exhausted;
    const auto recovered = recover();
    if (recovered.status == Status::empty) return Status::uninitialized;
    if (recovered.status != Status::ok) return recovered.status;
    if (generation != recovered.next_generation || generation <= recovered.active_generation) return Status::replay_rejected;
    std::vector<std::string> keys; auto store_result = store_.list_keys(keys); if (store_result != StoreResult::ok) return store_failure(store_result);
    std::array<bool, kMaxRecordCount> used{}; std::array<std::uint64_t, kMaxRecordCount> slot_generation{};
    for (const auto& key : keys) {
        std::size_t slot = 0; if (!record_slot(key, slot)) continue;
        std::vector<std::uint8_t> raw; if (store_.get(key, raw) != StoreResult::ok) return Status::storage_error;
        DecodedRecord decoded{}; bool schema_bad = false; if (!decode_record(raw, decoded, schema_bad)) return schema_bad ? Status::schema_mismatch : Status::corrupt;
        used[slot] = true; slot_generation[slot] = decoded.generation;
    }
    std::vector<std::uint8_t> root_raw; if (store_.get("root", root_raw) != StoreResult::ok || root_raw.size() != kRootBytes) return Status::corrupt;
    const auto active_slot = root_raw[16];
    std::size_t slot = kMaxRecordCount;
    for (std::size_t i = 0; i < kMaxRecordCount; ++i) if (!used[i]) { slot = i; break; }
    if (slot == kMaxRecordCount) {
        for (std::size_t i = 0; i < kMaxRecordCount; ++i) {
            if (i == active_slot) continue;
            if (slot == kMaxRecordCount || slot_generation[i] < slot_generation[slot]) slot = i;
        }
    }
    if (slot == kMaxRecordCount) return Status::capacity_exhausted;
    const auto fence = fence_value(generation);
    if ((store_result = store_.set("fence", fence)) != StoreResult::ok || (store_result = store_.commit()) != StoreResult::ok) return store_failure(store_result);
    const auto record = record_value(generation, payload);
    if ((store_result = store_.set(record_key(slot), record)) != StoreResult::ok || (store_result = store_.commit()) != StoreResult::ok) return store_failure(store_result);
    const auto root = root_value(generation, static_cast<std::uint8_t>(slot), get32(record, record.size() - 4));
    if ((store_result = store_.set("root", root)) != StoreResult::ok || (store_result = store_.commit()) != StoreResult::ok) return store_failure(store_result);
    return Status::ok;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::ok: return "ok"; case Status::empty: return "empty"; case Status::uninitialized: return "uninitialized";
        case Status::corrupt: return "corrupt"; case Status::ambiguous: return "ambiguous"; case Status::schema_mismatch: return "schema_mismatch";
        case Status::capacity_exhausted: return "capacity_exhausted"; case Status::replay_rejected: return "replay_rejected"; case Status::storage_error: return "storage_error";
    }
    return "unknown";
}

}  // namespace remote_persistence

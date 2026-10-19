#include "remote_persistence/diagnostic.hpp"

#if A010_REMOTE_PERSISTENCE_DIAGNOSTIC

#include "remote_persistence/esp_nvs_store.hpp"

#include "esp_log.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <array>
#include <cstring>

namespace remote_persistence::diagnostic {
namespace {

constexpr char kTag[] = "A010_PERSIST_DIAG";
constexpr std::array<std::uint8_t, 24> kPayload1{
    0x41,0x30,0x31,0x30,0x2d,0x44,0x49,0x41,0x47,0x2d,0x52,0x45,
    0x43,0x4f,0x52,0x44,0x2d,0x30,0x30,0x30,0x30,0x30,0x30,0x31};
constexpr std::array<std::uint8_t, 24> kPayload2{
    0x41,0x30,0x31,0x30,0x2d,0x44,0x49,0x41,0x47,0x2d,0x52,0x45,
    0x43,0x4f,0x52,0x44,0x2d,0x30,0x30,0x30,0x30,0x30,0x30,0x32};

const esp_partition_t* partition() {
    return esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
                                    ESP_PARTITION_SUBTYPE_ANY,
                                    kPartitionLabel);
}

bool exact_partition(const esp_partition_t* value) {
    return value != nullptr && value->type == ESP_PARTITION_TYPE_DATA &&
           value->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS &&
           value->address == kExpectedAddress && value->size == kExpectedSize &&
           std::strcmp(value->label, kPartitionLabel) == 0;
}

bool erased(const esp_partition_t* value) {
    std::array<std::uint8_t, 256> buffer{};
    for (std::size_t offset = 0; offset < value->size; offset += buffer.size()) {
        if (esp_partition_read(value, offset, buffer.data(), buffer.size()) != ESP_OK) return false;
        for (const auto byte : buffer) if (byte != 0xff) return false;
    }
    return true;
}

MediaState state_from(Status status, std::uint64_t generation) {
    switch (status) {
        case Status::ok: return generation == 0 ? MediaState::initialized_empty : MediaState::valid;
        case Status::empty: return MediaState::erased;
        case Status::corrupt: return MediaState::corrupt;
        case Status::ambiguous: return MediaState::ambiguous;
        case Status::schema_mismatch: return MediaState::schema_mismatch;
        default: return MediaState::storage_error;
    }
}

class EspTarget final : public Target {
public:
    View inspect(bool recover_backend) override {
        View view;
        const auto* selected = partition();
        if (selected == nullptr) return view;
        view.partition_exists = true;
        view.type = selected->type;
        view.subtype = selected->subtype;
        view.address = selected->address;
        view.size = selected->size;
        if (!exact_partition(selected)) { view.state = MediaState::wrong_partition; return view; }
        if (erased(selected)) { view.state = MediaState::erased; return view; }
        if (!recover_backend) { view.state = MediaState::storage_error; return view; }
        EspNvsStore store;
        const auto opened = store.open(EspNvsStore::Access::read_only);
        if (opened != StoreResult::ok) {
            view.state = opened == StoreResult::version_mismatch ? MediaState::schema_mismatch :
                         opened == StoreResult::no_space ? MediaState::corrupt : MediaState::storage_error;
            return view;
        }
        const auto recovery = Backend(store).recover();
        view.state = state_from(recovery.status, recovery.active_generation);
        view.generation = recovery.active_generation;
        view.next_generation = recovery.next_generation;
        view.incomplete = recovery.incomplete_transaction;
        nvs_stats_t stats{};
        if (nvs_get_stats(kPartitionLabel, &stats) == ESP_OK) {
            view.used_entries = stats.used_entries;
            view.free_entries = stats.free_entries;
            view.available_entries = stats.available_entries;
        }
        return view;
    }

    View initialize() override {
        const auto* selected = partition();
        if (!exact_partition(selected) || !erased(selected)) return inspect(true);
        bool success = false;
        {
            EspNvsStore store;
            success = store.open(EspNvsStore::Access::read_write) == StoreResult::ok &&
                      Backend(store).initialize_erased() == Status::ok;
        }
        if (!success) return inspect(true);
        return inspect(true);
    }

    View commit_known(unsigned payload_number) override {
        bool success = false;
        {
            EspNvsStore store;
            if (store.open(EspNvsStore::Access::read_write) == StoreResult::ok) {
                Backend backend(store);
                const auto before = backend.recover();
                const auto payload = payload_number == 1 ? std::span<const std::uint8_t>(kPayload1) : std::span<const std::uint8_t>(kPayload2);
                success = before.status == Status::ok &&
                          backend.commit(before.next_generation, payload) == Status::ok;
            }
        }
        if (!success) return inspect(true);
        return inspect(true);
    }

    View erase_partition() override {
        const auto* selected = partition();
        if (!exact_partition(selected)) return inspect(false);
        const auto deinit = nvs_flash_deinit_partition(kPartitionLabel);
        if (deinit != ESP_OK && deinit != ESP_ERR_NVS_NOT_INITIALIZED) return inspect(false);
        if (nvs_flash_erase_partition(kPartitionLabel) != ESP_OK) return inspect(false);
        return inspect(false);
    }
};

EspTarget target;
Engine engine(target, true);
std::array<char, 160> line{};
std::size_t line_size = 0;
bool receiving = false;

}  // namespace

void announce() {
    ESP_LOGW(kTag,
             "build=A010_VALIDATION diagnostic=ENABLED persistence_on_boot=0 prefix=!A010 commands=HELP,INFO,STATUS,RECOVER,INIT,COMMIT1,COMMIT2,ERASE");
}

bool consume_uart_byte(std::uint8_t byte) {
    if (!receiving) {
        if (byte != '!') return false;
        receiving = true;
        line_size = 0;
        line[line_size++] = static_cast<char>(byte);
        return true;
    }
    if (byte == '\r') return true;
    if (byte == '\n') {
        const auto reply = engine.execute(std::string_view(line.data(), line_size));
        ESP_LOGI(kTag, "%s", reply.text.c_str());
        receiving = false;
        line_size = 0;
        return true;
    }
    if (line_size + 1 >= line.size()) {
        ESP_LOGE(kTag, "accepted=0 reason=line_too_long payload_logged=0");
        receiving = false;
        line_size = 0;
        return true;
    }
    line[line_size++] = static_cast<char>(byte);
    return true;
}

}  // namespace remote_persistence::diagnostic

#else

namespace remote_persistence::diagnostic {
bool consume_uart_byte(std::uint8_t) { return false; }
void announce() {}
}  // namespace remote_persistence::diagnostic

#endif

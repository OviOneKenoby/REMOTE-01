#include "remote_persistence/esp_nvs_store.hpp"

#include "esp_partition.h"
#include "nvs_flash.h"

#include <cstring>

extern "C" void remote_persistence_esp_nvs_anchor() {}

namespace remote_persistence {
namespace {
StoreResult translate(esp_err_t error) {
    if (error == ESP_OK) return StoreResult::ok;
    if (error == ESP_ERR_NVS_NOT_FOUND) return StoreResult::not_found;
    if (error == ESP_ERR_NVS_NOT_ENOUGH_SPACE || error == ESP_ERR_NVS_NO_FREE_PAGES) return StoreResult::no_space;
    if (error == ESP_ERR_NVS_NEW_VERSION_FOUND) return StoreResult::version_mismatch;
    return StoreResult::io_error;
}
}  // namespace

EspNvsStore::~EspNvsStore() { if (open_) nvs_close(handle_); }

StoreResult EspNvsStore::open(Access access) {
    if (open_) return StoreResult::ok;
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, kPartitionLabel);
    if (partition == nullptr || partition->address != 0xE20000 || partition->size != 0x10000 || std::strcmp(partition->label, kPartitionLabel) != 0) return StoreResult::io_error;
    const auto initialized = nvs_flash_init_partition(kPartitionLabel);
    if (initialized != ESP_OK) return translate(initialized);  // Never erase automatically.
    const auto mode = access == Access::read_only ? NVS_READONLY : NVS_READWRITE;
    const auto opened = nvs_open_from_partition(kPartitionLabel, kNamespace, mode, &handle_);
    if (opened != ESP_OK) return translate(opened);
    open_ = true; return StoreResult::ok;
}

StoreResult EspNvsStore::list_keys(std::vector<std::string>& keys) {
    keys.clear(); if (!open_) return StoreResult::io_error;
    nvs_iterator_t iterator = nullptr;
    auto error = nvs_entry_find(kPartitionLabel, kNamespace, NVS_TYPE_ANY, &iterator);
    while (error == ESP_OK) {
        nvs_entry_info_t info{}; error = nvs_entry_info(iterator, &info); if (error != ESP_OK) break;
        keys.emplace_back(info.key); error = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);
    return error == ESP_ERR_NVS_NOT_FOUND ? StoreResult::ok : translate(error);
}

StoreResult EspNvsStore::get(const std::string& key, std::vector<std::uint8_t>& value) {
    if (!open_) return StoreResult::io_error;
    std::size_t size = 0; auto error = nvs_get_blob(handle_, key.c_str(), nullptr, &size); if (error != ESP_OK) return translate(error);
    value.resize(size); error = nvs_get_blob(handle_, key.c_str(), value.data(), &size); if (error == ESP_OK) value.resize(size); return translate(error);
}
StoreResult EspNvsStore::set(const std::string& key, std::span<const std::uint8_t> value) {
    if (!open_) return StoreResult::io_error;
    return translate(nvs_set_blob(handle_, key.c_str(), value.data(), value.size()));
}
StoreResult EspNvsStore::commit() { return open_ ? translate(nvs_commit(handle_)) : StoreResult::io_error; }

}  // namespace remote_persistence

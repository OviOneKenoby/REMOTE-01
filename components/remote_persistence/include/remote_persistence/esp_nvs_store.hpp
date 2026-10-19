#pragma once

#include "remote_persistence/backend.hpp"

#include "nvs.h"

namespace remote_persistence {

class EspNvsStore final : public Store {
public:
    enum class Access { read_only, read_write };
    EspNvsStore() = default;
    ~EspNvsStore() override;
    EspNvsStore(const EspNvsStore&) = delete;
    EspNvsStore& operator=(const EspNvsStore&) = delete;

    // Explicitly opens only the installed remote_ctl NVS partition. It never erases it.
    StoreResult open(Access access = Access::read_write);
    StoreResult list_keys(std::vector<std::string>& keys) override;
    StoreResult get(const std::string& key, std::vector<std::uint8_t>& value) override;
    StoreResult set(const std::string& key, std::span<const std::uint8_t> value) override;
    StoreResult commit() override;

private:
    nvs_handle_t handle_{0};
    bool open_{false};
};

}  // namespace remote_persistence

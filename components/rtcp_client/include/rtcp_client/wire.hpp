#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace rtcp_client::wire {
enum class Kind:std::uint8_t{identity_request=1,identity_response=2,capability_request=3,capability_response=4,snapshot_request=5,snapshot_response=6,volume_set=7,command_result=8};
struct Identity{std::uint8_t major=0,minor=0,patch=0;char firmware[32]{};};
struct Capabilities{bool volume=false,playback=false,volume_set=false;std::uint64_t revision=0;std::uint32_t max_age_ms=0;};
struct Snapshot{std::uint8_t volume=0,playback=0;std::uint64_t sequence=0,media_generation=0;};
struct Result{std::uint64_t request=0;bool accepted=false;std::uint8_t error=0;};
std::size_t identity_request(std::span<std::uint8_t>);
std::size_t capability_request(std::span<std::uint8_t>);
std::size_t snapshot_request(std::span<std::uint8_t>);
std::size_t volume_set(std::uint64_t,std::uint32_t,std::uint32_t,std::span<std::uint8_t>);
bool parse(std::span<const std::uint8_t>,Identity&);bool parse(std::span<const std::uint8_t>,Capabilities&);
bool parse(std::span<const std::uint8_t>,Snapshot&);bool parse(std::span<const std::uint8_t>,Result&);
}

#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>

namespace rtcp_client::wire {
enum class Kind:std::uint8_t{identity_request=1,identity_response=2,capability_request=3,capability_response=4,snapshot_request=5,snapshot_response=6,volume_set=7,command_result=8,playback_command=9,now_playing_request=10,now_playing_response=11};
enum class PlaybackOperation:std::uint8_t{play=1,pause=2,previous=3,next=4};
struct Identity{std::uint8_t major=0,minor=0,patch=0;char firmware[32]{};};
struct Capabilities{
  bool volume=false,playback=false,volume_set=false,play=false,pause=false,
       previous=false,next=false,now_playing=false;
  std::uint64_t revision=0;std::uint32_t max_age_ms=0;
  bool d009()const{return revision==2;}
  bool supports(PlaybackOperation op)const{
    if(!d009())return false;
    switch(op){case PlaybackOperation::play:return play;case PlaybackOperation::pause:return pause;
      case PlaybackOperation::previous:return previous;case PlaybackOperation::next:return next;}
    return false;
  }
};
struct Snapshot{std::uint8_t volume=0,playback=0;std::uint64_t sequence=0,media_generation=0;};
struct Result{std::uint64_t request=0;bool accepted=false;std::uint8_t error=0;};
struct NowPlaying{
  std::uint8_t flags=0,context=0,station_size=0,artist_size=0,title_size=0;
  std::uint16_t combined_size=0;
  std::uint64_t sequence=0,media_generation=0,metadata_revision=0;
  std::array<char,64>station{};std::array<char,97>artist{};
  std::array<char,256>title{},combined{};
};
std::size_t identity_request(std::span<std::uint8_t>);
std::size_t capability_request(std::span<std::uint8_t>);
std::size_t snapshot_request(std::span<std::uint8_t>);
std::size_t now_playing_request(std::span<std::uint8_t>);
std::size_t volume_set(std::uint64_t,std::uint32_t,std::uint32_t,std::span<std::uint8_t>);
std::size_t playback_command(std::uint64_t,PlaybackOperation,std::uint8_t,std::uint64_t,std::span<std::uint8_t>);
bool parse(std::span<const std::uint8_t>,Identity&);bool parse(std::span<const std::uint8_t>,Capabilities&);
bool parse(std::span<const std::uint8_t>,Snapshot&);bool parse(std::span<const std::uint8_t>,Result&);
bool parse(std::span<const std::uint8_t>,NowPlaying&);
}

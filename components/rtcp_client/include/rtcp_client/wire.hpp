#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>

namespace rtcp_client::wire {
enum class Kind:std::uint8_t{identity_request=1,identity_response=2,capability_request=3,capability_response=4,snapshot_request=5,snapshot_response=6,volume_set=7,command_result=8,playback_command=9,now_playing_request=10,now_playing_response=11,btx_capability_response=12,btx_state_request=13,btx_state_response=14,btx_command=15,full_control_capability_request=16,full_control_capability_response=17,source_state_request=18,source_state_response=19,source_select_request=20,menu_state_request=21,menu_state_response=22};
enum class PlaybackOperation:std::uint8_t{play=1,pause=2,previous=3,next=4};
enum class BtxSource:std::uint8_t{none=0,radio=1,bluetooth=2,other=3};
enum class BtxConnection:std::uint8_t{not_applicable=0,unknown=1,disconnected=2,connected=3};
enum class BtxPlayback:std::uint8_t{unknown=0,stopped=1,playing=2,paused=3};
enum class Source:std::uint8_t{none=0,radio=1,bluetooth=2};
enum class SourceTransition:std::uint8_t{stable=0,starting_bluetooth=1,switching_radio=2,failed=3};
struct Identity{
  std::uint8_t major=0,minor=0,patch=0;char firmware[32]{};
  bool full_control_advertised()const;
};
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
struct BtxCapabilities{
  std::uint64_t revision=0;std::uint16_t freshness_ms=0;
  std::uint8_t features=0,max_artist=0,max_title=0;
  bool negotiated=false;
  bool supports(PlaybackOperation op)const{
    if(!negotiated)return false;
    const auto bit=op==PlaybackOperation::play?2:op==PlaybackOperation::pause?3:
                   op==PlaybackOperation::previous?4:5;
    return (features&(1u<<bit))!=0;
  }
};
struct FullControlCapabilities{
  std::uint64_t revision=0;std::uint32_t features=0;std::uint16_t freshness_ms=0;
  bool negotiated=false;
  bool source_switch()const{return negotiated&&(features&1u);}
  bool menu_projection()const{return negotiated&&(features&2u);}
};
struct SourceState{
  std::uint64_t sequence=0,generation=0;
  Source source=Source::none;SourceTransition transition=SourceTransition::stable;
  bool bluetooth_connected=false;std::uint8_t last_error=0;
};
enum class MenuMode:std::uint8_t{unavailable=0,station_select=1,playing_radio=2,bluetooth=3,browse_letter=4,browse_country=5,browse_station_letter=6,browse_tag=7,browse_results=8,favorites=9,recent=10,idle=11,error=12,other=13};
struct MenuItem{
  std::uint16_t token=0;bool active=false;std::uint8_t name_size=0;
  std::array<char,49>name{};
};
struct MenuState{
  std::uint64_t sequence=0,revision=0;MenuMode mode=MenuMode::unavailable;
  std::uint8_t flags=0,error=0;std::uint16_t selected=0,count=0,window_start=0;
  std::uint8_t window_count=0;std::array<MenuItem,5>items{};
  bool loading()const{return flags&1u;}bool failed()const{return flags&2u;}
};
enum class BtxProbeResult:std::uint8_t{invalid,legacy,enabled};
struct BtxNegotiation{
  enum class Stage:std::uint8_t{first,probe,legacy,enabled};
  Stage stage=Stage::first;
  void reset(){stage=Stage::first;}
  bool first(const Capabilities&value){if(stage!=Stage::first||!value.d009())return false;stage=Stage::probe;return true;}
  BtxProbeResult second(std::span<const std::uint8_t>,const Capabilities&,BtxCapabilities&);
};
struct Snapshot{std::uint8_t volume=0,playback=0;std::uint64_t sequence=0,media_generation=0;};
struct Result{std::uint64_t request=0;bool accepted=false;std::uint8_t error=0;};
struct NowPlaying{
  std::uint8_t flags=0,context=0,station_size=0,artist_size=0,title_size=0;
  std::uint8_t btx_source=0,btx_connection=0,btx_playback=0,btx_flags=0;
  std::uint16_t combined_size=0;
  std::uint64_t sequence=0,media_generation=0,metadata_revision=0;
  std::array<char,64>station{};std::array<char,97>artist{};
  std::array<char,256>title{},combined{};
};
std::size_t identity_request(std::span<std::uint8_t>);
std::size_t capability_request(std::span<std::uint8_t>);
std::size_t snapshot_request(std::span<std::uint8_t>);
std::size_t now_playing_request(std::span<std::uint8_t>);
std::size_t btx_state_request(std::span<std::uint8_t>);
std::size_t full_control_capability_request(std::span<std::uint8_t>);
std::size_t source_state_request(std::span<std::uint8_t>);
std::size_t menu_state_request(std::span<std::uint8_t>);
std::size_t volume_set(std::uint64_t,std::uint32_t,std::uint32_t,std::span<std::uint8_t>);
std::size_t playback_command(std::uint64_t,PlaybackOperation,std::uint8_t,std::uint64_t,std::span<std::uint8_t>);
std::size_t btx_command(std::uint64_t,PlaybackOperation,BtxPlayback,std::uint64_t,std::span<std::uint8_t>);
std::size_t source_select(std::uint64_t,Source,std::uint64_t,std::span<std::uint8_t>);
bool parse(std::span<const std::uint8_t>,Identity&);bool parse(std::span<const std::uint8_t>,Capabilities&);
bool parse(std::span<const std::uint8_t>,BtxCapabilities&);
bool parse(std::span<const std::uint8_t>,FullControlCapabilities&);
bool parse(std::span<const std::uint8_t>,SourceState&);
bool parse(std::span<const std::uint8_t>,MenuState&);
bool parse(std::span<const std::uint8_t>,Snapshot&);bool parse(std::span<const std::uint8_t>,Result&);
bool parse(std::span<const std::uint8_t>,NowPlaying&);
bool parse_btx_state(std::span<const std::uint8_t>,NowPlaying&);
bool permitted(const BtxCapabilities&,const NowPlaying&);
}

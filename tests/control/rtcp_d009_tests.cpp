#include "rtcp_client/wire.hpp"
#include "control_home/rtcp_d009_policy.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
unsigned passed=0,failed=0,assertions=0;
#define CHECK(x) do{++assertions;if(!(x)){++failed;std::printf("FAIL line=%d expr=%s\n",__LINE__,#x);return false;}}while(0)
#define TEST(x) do{if(x())++passed;}while(0)
void p64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=std::uint8_t(v);v>>=8;}}

bool requests_and_commands(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,32>b{};
  CHECK(now_playing_request(b)==1&&b[0]==10);
  CHECK(playback_command(9,PlaybackOperation::next,0,17,b)==19);
  CHECK(b[0]==9&&b[9]==4&&b[10]==0&&b[18]==17);
  CHECK(!playback_command(0,PlaybackOperation::play,0,1,b));
  CHECK(!playback_command(1,PlaybackOperation::play,4,1,b));
  CHECK(!playback_command(1,PlaybackOperation::play,0,0,b));return true;
}

bool capability_gating(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,16>b{};b[0]=4;b[1]=7;
  p64(b.data()+2,1);b[12]=7;b[13]=0xd0;b[14]=1;b[15]=100;
  Capabilities old{};CHECK(parse(b,old));CHECK(!old.d009()&&!old.now_playing);
  b[1]=0xff;p64(b.data()+2,2);Capabilities current{};CHECK(parse(b,current));
  CHECK(current.d009()&&current.now_playing&&current.supports(PlaybackOperation::previous));
  b[1]=7;p64(b.data()+2,2);Capabilities partial{};CHECK(parse(b,partial));
  CHECK(partial.d009()&&!partial.supports(PlaybackOperation::play));return true;
}

std::size_t now(std::span<std::uint8_t>b,std::uint8_t flags,std::uint8_t context,
                const char*station,const char*artist,const char*title,const char*combined){
  const auto sn=std::strlen(station),an=std::strlen(artist),tn=std::strlen(title),cn=std::strlen(combined);
  const auto n=32+sn+an+tn+cn;if(n>b.size())return 0;b[0]=11;b[1]=flags;b[2]=context;
  b[3]=std::uint8_t(sn);b[4]=std::uint8_t(an);b[5]=std::uint8_t(tn);b[6]=std::uint8_t(cn>>8);b[7]=std::uint8_t(cn);
  p64(b.data()+8,5);p64(b.data()+16,7);p64(b.data()+24,9);std::size_t at=32;
  for(auto value:{std::pair{station,sn},std::pair{artist,an},std::pair{title,tn},std::pair{combined,cn}}){std::memcpy(b.data()+at,value.first,value.second);at+=value.second;}
  return n;
}

bool combined_metadata(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,446>b{};
  const auto n=now(b,0xe3,1,"Station", "", "", "Artist - Song");NowPlaying value{};
  CHECK(n==52&&parse(std::span<const std::uint8_t>(b.data(),n),value));
  CHECK(value.flags==0xe3&&value.context==1&&value.sequence==5&&value.media_generation==7&&value.metadata_revision==9);
  CHECK(std::strcmp(value.station.data(),"Station")==0&&std::strcmp(value.combined.data(),"Artist - Song")==0);
  b[1]|=4;CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));return true;
}

bool split_and_context_bounds(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,446>b{};NowPlaying value{};
  auto n=now(b,0x07,2,"Radio","Artist","Title","");
  CHECK(parse(std::span<const std::uint8_t>(b.data(),n),value));
  CHECK(std::strcmp(value.artist.data(),"Artist")==0&&std::strcmp(value.title.data(),"Title")==0);
  b[1]|=0x40;CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));
  n=now(b,0,3,"","","","");CHECK(parse(std::span<const std::uint8_t>(b.data(),n),value));
  b[1]=0x10;CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));return true;
}

bool malformed_text_and_lengths(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,446>b{};NowPlaying value{};
  auto n=now(b,0x03,1,"S","","","Good");b[n-1]=0x7f;
  CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));
  n=now(b,0x03,1,"S","","","Good");b[n-2]=0xc3;
  CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));
  n=now(b,0x03,1,"S","","","Muzic\xc4\x83");
  CHECK(parse(std::span<const std::uint8_t>(b.data(),n),value));
  b[3]=64;CHECK(!parse(std::span<const std::uint8_t>(b.data(),n),value));return true;
}

bool result_bounds(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,11>b{};b[0]=8;p64(b.data()+1,3);Result value{};
  b[9]=1;b[10]=0;CHECK(parse(b,value)&&value.accepted);
  b[9]=0;b[10]=7;CHECK(parse(b,value)&&!value.accepted);
  b[10]=8;CHECK(!parse(b,value));b[10]=0;CHECK(!parse(b,value));return true;
}

bool generation_and_revision_fence(){
  control_home::d009::MediaFence fence;
  auto a=fence.snapshot(1,4);CHECK(a.accepted&&a.new_context);
  auto b=fence.metadata(2,4,8);CHECK(b.accepted&&!b.new_context);
  CHECK(!fence.metadata(2,4,8).accepted);CHECK(!fence.metadata(3,3,9).accepted);
  CHECK(!fence.metadata(3,4,7).accepted);
  auto c=fence.metadata(3,5,1);CHECK(c.accepted&&c.new_context);
  fence.reset();CHECK(fence.sequence==0&&fence.generation==0&&fence.metadata_revision==0);return true;
}

bool result_mapping(){
  using namespace control_home::d009;
  CHECK(classify_result(1)==ResultClass::invalid_argument);
  CHECK(classify_result(2)==ResultClass::unsupported);
  CHECK(classify_result(3)==ResultClass::unavailable);
  CHECK(classify_result(4)==ResultClass::stale);
  CHECK(classify_result(5)==ResultClass::execution_failed);
  CHECK(classify_result(6)==ResultClass::busy);
  CHECK(classify_result(7)==ResultClass::internal);
  CHECK(classify_result(8)==ResultClass::invalid);return true;
}
}

int main(){TEST(requests_and_commands);TEST(capability_gating);TEST(combined_metadata);
  TEST(split_and_context_bounds);TEST(malformed_text_and_lengths);TEST(result_bounds);
  TEST(generation_and_revision_fence);TEST(result_mapping);
  std::printf("RTCP_D009: PASS=%u FAIL=%u assertions=%u\n",passed,failed,assertions);
  return failed?1:0;
}

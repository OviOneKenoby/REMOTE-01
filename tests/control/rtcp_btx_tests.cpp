#include "control_home/api.hpp"
#include "control_home/btx_policy.hpp"
#include "control_home/rtcp_d009_policy.hpp"
#include "rtcp_client/wire.hpp"
#include <array>
#include <cstdio>
#include <cstring>

namespace {
unsigned passed=0,failed=0,assertions=0;
#define CHECK(x) do{++assertions;if(!(x)){++failed;std::printf("FAIL line=%d expr=%s\n",__LINE__,#x);return false;}}while(0)
#define TEST(x) do{if(x())++passed;}while(0)
void p64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=std::uint8_t(v);v>>=8;}}

std::size_t state(std::span<std::uint8_t>b,std::uint8_t source,std::uint8_t connection,
                  std::uint8_t playback,std::uint8_t flags,const char*artist,
                  const char*title,std::uint64_t sequence=7,std::uint64_t generation=3,
                  std::uint64_t revision=5){
  const auto an=std::strlen(artist),tn=std::strlen(title),n=32+an+tn;
  if(n>b.size())return 0;
  b[0]=14;b[1]=1;b[2]=source;b[3]=connection;
  b[4]=playback;b[5]=flags;b[6]=std::uint8_t(an);b[7]=std::uint8_t(tn);
  p64(b.data()+8,sequence);p64(b.data()+16,generation);p64(b.data()+24,revision);
  std::memcpy(b.data()+32,artist,an);std::memcpy(b.data()+32+an,title,tn);return n;
}

bool capability_and_requests(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,32>b{};
  const std::array<std::uint8_t,16> vector{0x0c,0x01,0x00,0x3f,0x60,0x7f,0x07,0xd0,0,0,0,0,0,0,0,1};
  BtxCapabilities c{};CHECK(parse(vector,c)&&c.negotiated&&c.features==0x3f);
  CHECK(c.max_artist==96&&c.max_title==127&&c.freshness_ms==2000&&c.revision==1);
  CHECK(btx_state_request(b)==1&&b[0]==13);
  auto bad=vector;bad[3]|=0x80;CHECK(!parse(bad,c));bad=vector;bad[6]=0;CHECK(!parse(bad,c));
  bad=vector;bad[3]&=~2;CHECK(!parse(bad,c));return true;
}

bool negotiation_lifecycle(){
  using namespace rtcp_client::wire;
  const std::array<std::uint8_t,16> legacy_bytes{4,0xff,0,0,0,0,0,0,0,2,0,0,7,0xd0,1,100};
  const std::array<std::uint8_t,16> btx_bytes{12,1,0,0x3f,96,127,7,0xd0,0,0,0,0,0,0,0,1};
  Capabilities legacy{};CHECK(parse(legacy_bytes,legacy));
  BtxCapabilities extension{};BtxNegotiation probe;
  CHECK(probe.first(legacy));CHECK(probe.second(btx_bytes,legacy,extension)==BtxProbeResult::enabled);
  CHECK(extension.negotiated&&probe.stage==BtxNegotiation::Stage::enabled);
  probe.reset();extension={};CHECK(probe.first(legacy));
  CHECK(probe.second(legacy_bytes,legacy,extension)==BtxProbeResult::legacy&&!extension.negotiated);
  probe.reset();CHECK(probe.stage==BtxNegotiation::Stage::first);
  auto invalid=btx_bytes;invalid[15]=2;CHECK(probe.first(legacy));
  CHECK(probe.second(invalid,legacy,extension)==BtxProbeResult::invalid);
  probe.reset();CHECK(probe.second(btx_bytes,legacy,extension)==BtxProbeResult::invalid);return true;
}

bool command_vector(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,19>b{};
  CHECK(btx_command(0x0102030405060708ULL,PlaybackOperation::pause,
                    BtxPlayback::playing,3,b)==19);
  const std::array<std::uint8_t,19> expected{0x0f,1,2,3,4,5,6,7,8,2,2,0,0,0,0,0,0,0,3};
  CHECK(b==expected);CHECK(!btx_command(0,PlaybackOperation::play,BtxPlayback::unknown,1,b));
  CHECK(!btx_command(1,PlaybackOperation::play,BtxPlayback::unknown,0,b));return true;
}

bool empty_and_maximum_state(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,255>b{};NowPlaying v{};
  auto n=state(b,2,3,0,0x30,"","");CHECK(n==32&&parse_btx_state({b.data(),n},v));
  CHECK(v.btx_source==2&&v.btx_connection==3&&v.sequence==7);
  std::memset(b.data()+32,'A',96);std::memset(b.data()+128,'T',127);
  b[4]=3;b[5]=0x37;b[6]=96;b[7]=127;n=255;
  CHECK(parse_btx_state({b.data(),n},v));CHECK(v.artist_size==96&&v.title_size==127);
  BtxCapabilities c{1,2000,0x3f,96,127,true};CHECK(permitted(c,v));
  c.features&=~0x02;CHECK(!permitted(c,v));c.features=0x3f;c.max_title=126;
  CHECK(!permitted(c,v));
  char title[227]{};CHECK(control_home::btx::format_title(v,title)==226);
  CHECK(title[96]==' '&&title[97]=='-'&&title[98]==' '&&title[225]=='T');return true;
}

bool invalid_state(){
  using namespace rtcp_client::wire;std::array<std::uint8_t,255>b{};NowPlaying v{};
  auto n=state(b,2,3,2,0x3b,"Artist","Title");CHECK(parse_btx_state({b.data(),n},v));
  auto good=b;b[2]=4;CHECK(!parse_btx_state({b.data(),n},v));b=good;b[5]|=0x80;CHECK(!parse_btx_state({b.data(),n},v));
  b=good;b[6]++;CHECK(!parse_btx_state({b.data(),n},v));b=good;b[32]=0xff;CHECK(!parse_btx_state({b.data(),n},v));
  n=state(b,1,0,0,0,"","");CHECK(parse_btx_state({b.data(),n},v));
  n=state(b,1,1,0,0,"","");CHECK(!parse_btx_state({b.data(),n},v));
  n=state(b,2,2,0,4,"","");CHECK(!parse_btx_state({b.data(),n},v));
  n=state(b,2,3,2,4,"","");CHECK(!parse_btx_state({b.data(),n},v));return true;
}

bool presentation_states(){
  using namespace rtcp_client::wire;NowPlaying v{};char out[227]{};
  v.btx_source=2;v.btx_connection=3;
  std::memcpy(v.artist.data(),"Artist",6);v.artist_size=6;
  std::memcpy(v.title.data(),"Title",5);v.title_size=5;
  CHECK(control_home::btx::format_title(v,out)==14&&std::strcmp(out,"Artist - Title")==0);
  v.title_size=0;CHECK(control_home::btx::format_title(v,out)==6&&std::strcmp(out,"Artist")==0);
  v.artist_size=0;v.title_size=5;CHECK(control_home::btx::format_title(v,out)==5&&std::strcmp(out,"Title")==0);
  v.title_size=0;CHECK(control_home::btx::format_title(v,out)==0&&out[0]==0);
  v.btx_connection=2;CHECK(control_home::btx::format_title(v,out)==18&&std::strcmp(out,"Phone disconnected")==0);
  v.btx_connection=1;CHECK(control_home::btx::format_title(v,out)==0);
  v.btx_source=1;CHECK(control_home::btx::format_title(v,out)==0);
  v.btx_source=2;v.btx_connection=3;v.artist_size=6;v.title_size=5;
  CHECK(control_home::btx::format_title(v,v.combined)==14&&
        std::strcmp(v.combined.data(),"Artist - Title")==0);return true;
}

bool ordering_and_storage(){
  control_home::d009::MediaFence fence;
  CHECK(fence.snapshot(1,2).accepted);CHECK(fence.metadata(2,2,3).accepted);
  CHECK(!fence.metadata(2,2,3).accepted);CHECK(!fence.metadata(3,1,4).accepted);
  CHECK(fence.metadata(3,4,1).accepted);
  CHECK(sizeof(control_home::Snapshot)>=227+32);
  return true;
}
}

int main(){TEST(capability_and_requests);TEST(negotiation_lifecycle);TEST(command_vector);TEST(empty_and_maximum_state);
  TEST(invalid_state);TEST(presentation_states);TEST(ordering_and_storage);
  std::printf("RTCP_BTX: PASS=%u FAIL=%u assertions=%u snapshot_bytes=%u\n",passed,failed,assertions,unsigned(sizeof(control_home::Snapshot)));
  return failed?1:0;}

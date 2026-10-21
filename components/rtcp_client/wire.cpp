#include "rtcp_client/wire.hpp"
#include <cstring>
namespace rtcp_client::wire{namespace{
std::uint32_t g32(const std::uint8_t*p){return std::uint32_t(p[0])<<24|std::uint32_t(p[1])<<16|std::uint32_t(p[2])<<8|p[3];}
std::uint64_t g64(const std::uint8_t*p){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v=(v<<8)|p[i];return v;}
void p32(std::uint8_t*p,std::uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
void p64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=v;v>>=8;}}
bool text(const std::uint8_t*p,std::size_t n){
  for(std::size_t at=0;at<n;){const auto c=p[at];std::size_t width=0;std::uint32_t cp=0;
    if(c<0x80){width=1;cp=c;if(cp<0x20||cp==0x7f)return false;}
    else if(c>=0xc2&&c<=0xdf){width=2;cp=c&0x1f;}
    else if(c>=0xe0&&c<=0xef){width=3;cp=c&0x0f;}
    else if(c>=0xf0&&c<=0xf4){width=4;cp=c&0x07;}
    else return false;
    if(at+width>n)return false;
    for(std::size_t i=1;i<width;++i){if((p[at+i]&0xc0)!=0x80)return false;cp=(cp<<6)|(p[at+i]&0x3f);}
    if((width==2&&cp<0x80)||(width==3&&cp<0x800)||(width==4&&cp<0x10000)||
       (cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return false;
    at+=width;
  }
  return true;
}
}
std::size_t identity_request(std::span<std::uint8_t>o){if(o.size()<4)return 0;o[0]=1;o[1]=1;o[2]=1;o[3]=0;return 4;}
std::size_t capability_request(std::span<std::uint8_t>o){if(o.empty())return 0;o[0]=3;return 1;}
std::size_t snapshot_request(std::span<std::uint8_t>o){if(o.empty())return 0;o[0]=5;return 1;}
std::size_t now_playing_request(std::span<std::uint8_t>o){if(o.empty())return 0;o[0]=10;return 1;}
std::size_t volume_set(std::uint64_t id,std::uint32_t n,std::uint32_t d,std::span<std::uint8_t>o){if(o.size()<17||!id||!d||n>d)return 0;o[0]=7;p64(o.data()+1,id);p32(o.data()+9,n);p32(o.data()+13,d);return 17;}
std::size_t playback_command(std::uint64_t id,PlaybackOperation operation,std::uint8_t expected,std::uint64_t generation,std::span<std::uint8_t>o){
  const auto op=std::uint8_t(operation);if(o.size()<19||!id||!generation||op<1||op>4||expected>3)return 0;
  o[0]=9;p64(o.data()+1,id);o[9]=op;o[10]=expected;p64(o.data()+11,generation);return 19;
}
bool parse(std::span<const std::uint8_t>p,Identity&o){if(p.size()<5||p[0]!=2||p[4]>31||p.size()!=std::size_t(5+p[4]))return false;o={p[1],p[2],p[3],{}};std::memcpy(o.firmware,p.data()+5,p[4]);return o.major==1&&o.minor==1&&o.patch==0;}
bool parse(std::span<const std::uint8_t>p,Capabilities&o){if(p.size()!=16||p[0]!=4)return false;const auto bits=p[1];o={};o.volume=bits&1;o.playback=bits&2;o.volume_set=bits&4;o.play=bits&8;o.pause=bits&16;o.previous=bits&32;o.next=bits&64;o.now_playing=bits&128;o.revision=g64(p.data()+2);o.max_age_ms=g32(p.data()+10);return o.volume&&o.playback&&o.volume_set&&o.revision&&o.max_age_ms&&p[14]==1&&p[15]==100;}
bool parse(std::span<const std::uint8_t>p,Snapshot&o){if(p.size()!=19||p[0]!=6||p[1]>100||p[2]>3)return false;o.volume=p[1];o.playback=p[2];o.sequence=g64(p.data()+3);o.media_generation=g64(p.data()+11);return o.sequence&&o.media_generation;}
bool parse(std::span<const std::uint8_t>p,Result&o){if(p.size()!=11||p[0]!=8||p[9]>1)return false;o.request=g64(p.data()+1);o.accepted=p[9];o.error=p[10];return o.request&&((o.accepted&&o.error==0)||(!o.accepted&&o.error>=1&&o.error<=7));}
bool parse(std::span<const std::uint8_t>p,NowPlaying&o){
  if(p.size()<32||p.size()>446||p[0]!=11)return false;
  const auto flags=p[1],context=p[2],station=p[3],artist=p[4],title=p[5];
  const auto combined=std::uint16_t(p[6])<<8|p[7];
  const auto total=std::size_t(32)+station+artist+title+combined;
  if(context>4||station>63||artist>96||combined>255||p.size()!=total)return false;
  const bool station_valid=flags&1,metadata_valid=flags&2,split=flags&4;
  if(station_valid!=(station!=0)||(!station_valid&&station)||
     (station_valid&&context!=1&&context!=2))return false;
  if(!metadata_valid&&(split||artist||title||combined))return false;
  if(metadata_valid&&(!split?(artist||title||!combined):(combined||(!artist&&!title))))return false;
  if((context==0||context==3||context==4)&&flags)return false;
  if(context!=1&&(flags&0xc0))return false;
  if((flags&0x30)==0x30)return false;
  const auto sequence=g64(p.data()+8),generation=g64(p.data()+16),revision=g64(p.data()+24);
  if(!sequence||!generation||!revision)return false;
  std::size_t at=32;
  auto copy=[&](auto&out,std::size_t n){if(n&&!text(p.data()+at,n))return false;std::memcpy(out.data(),p.data()+at,n);out[n]=0;at+=n;return true;};
  NowPlaying value{};value.flags=flags;value.context=context;value.station_size=station;
  value.artist_size=artist;value.title_size=title;value.combined_size=combined;
  value.sequence=sequence;value.media_generation=generation;value.metadata_revision=revision;
  if(!copy(value.station,station)||!copy(value.artist,artist)||!copy(value.title,title)||!copy(value.combined,combined))return false;
  o=value;return true;
}
}

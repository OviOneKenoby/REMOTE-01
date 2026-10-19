#include "rtcp_client/wire.hpp"
#include <cstring>
namespace rtcp_client::wire{namespace{
std::uint32_t g32(const std::uint8_t*p){return std::uint32_t(p[0])<<24|std::uint32_t(p[1])<<16|std::uint32_t(p[2])<<8|p[3];}
std::uint64_t g64(const std::uint8_t*p){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v=(v<<8)|p[i];return v;}
void p32(std::uint8_t*p,std::uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
void p64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=v;v>>=8;}}
}
std::size_t identity_request(std::span<std::uint8_t>o){if(o.size()<4)return 0;o[0]=1;o[1]=1;o[2]=1;o[3]=0;return 4;}
std::size_t capability_request(std::span<std::uint8_t>o){if(o.empty())return 0;o[0]=3;return 1;}
std::size_t snapshot_request(std::span<std::uint8_t>o){if(o.empty())return 0;o[0]=5;return 1;}
std::size_t volume_set(std::uint64_t id,std::uint32_t n,std::uint32_t d,std::span<std::uint8_t>o){if(o.size()<17||!id||!d||n>d)return 0;o[0]=7;p64(o.data()+1,id);p32(o.data()+9,n);p32(o.data()+13,d);return 17;}
bool parse(std::span<const std::uint8_t>p,Identity&o){if(p.size()<5||p[0]!=2||p[4]>31||p.size()!=std::size_t(5+p[4]))return false;o={p[1],p[2],p[3],{}};std::memcpy(o.firmware,p.data()+5,p[4]);return o.major==1&&o.minor==1&&o.patch==0;}
bool parse(std::span<const std::uint8_t>p,Capabilities&o){if(p.size()!=16||p[0]!=4)return false;o.volume=p[1]&1;o.playback=p[1]&2;o.volume_set=p[1]&4;o.revision=g64(p.data()+2);o.max_age_ms=g32(p.data()+10);return o.volume&&o.playback&&o.volume_set&&o.revision&&o.max_age_ms&&p[14]==1&&p[15]==100;}
bool parse(std::span<const std::uint8_t>p,Snapshot&o){if(p.size()!=19||p[0]!=6||p[1]>100||p[2]>3)return false;o.volume=p[1];o.playback=p[2];o.sequence=g64(p.data()+3);o.media_generation=g64(p.data()+11);return o.sequence&&o.media_generation;}
bool parse(std::span<const std::uint8_t>p,Result&o){if(p.size()!=11||p[0]!=8||p[9]>1)return false;o.request=g64(p.data()+1);o.accepted=p[9];o.error=p[10];return o.request&&((o.accepted&&o.error==0)||(!o.accepted&&o.error));}
}

#include "artwork/online.hpp"

#include <cctype>
#include <cstring>

namespace artwork::online {
namespace {
std::string_view release_group_array(std::string_view json) {
  const auto key = json.find("\"release-groups\"");
  if (key == json.npos) return {};
  const auto begin = json.find('[', key + 12);
  return begin == json.npos ? std::string_view{} : json.substr(begin);
}
bool object_extent(std::string_view value, std::size_t begin, std::size_t &end) {
  if (begin >= value.size() || value[begin] != '{') return false;
  unsigned depth=0; bool quoted=false,escaped=false;
  for(std::size_t i=begin;i<value.size();++i){const char c=value[i];
    if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
    else if(c=='"')quoted=true;else if(c=='{')++depth;else if(c=='}'&&--depth==0){end=i+1;return true;}}
  return false;
}
bool string_after(std::string_view value,std::string_view key,std::size_t from,
                  char*out,std::size_t capacity,std::size_t*position=nullptr){
  const auto found=value.find(key,from);if(found==value.npos)return false;
  auto at=value.find(':',found+key.size());if(at==value.npos)return false;
  at=value.find('"',at+1);if(at==value.npos)return false;++at;std::size_t used=0;
  for(;at<value.size();++at){char c=value[at];if(c=='"'){out[used]=0;if(position)*position=at+1;return used!=0;}
    if(c=='\\'){if(++at>=value.size())return false;c=value[at];if(c=='n'||c=='r'||c=='t')c=' ';else if(c=='u')return false;}
    if(used+1>=capacity)return false;
    out[used++]=c;
  }
  return false;
}
int integer_after(std::string_view value,std::string_view key){auto at=value.find(key);
  if(at==value.npos||(at=value.find(':',at+key.size()))==value.npos)return -1;
  while(++at<value.size()&&std::isspace(static_cast<unsigned char>(value[at]))){}
  int result=0;bool any=false;while(at<value.size()&&std::isdigit(static_cast<unsigned char>(value[at]))){any=true;result=result*10+value[at++]-'0';}
  return any?result:-1;}
} // namespace

std::size_t parse_musicbrainz(std::string_view json,Candidate*out,std::size_t capacity){
  if(!out||!capacity)return 0;
  auto array=release_group_array(json);std::size_t count=0,at=1;
  while(count<capacity){at=array.find('{',at);if(at==array.npos)break;std::size_t end=0;
    if(!object_extent(array,at,end))break;
    auto object=array.substr(at,end-at);Candidate candidate{};
    candidate.score=integer_after(object,"\"score\"");const auto credits=object.find("\"artist-credit\"");
    bool good=string_after(object,"\"id\"",0,candidate.release_group.data(),candidate.release_group.size())&&
      string_after(object,"\"title\"",0,candidate.title.data(),candidate.title.size());
    if(good&&credits!=object.npos)good=string_after(object,"\"name\"",credits,candidate.artist.data(),candidate.artist.size());
    if(good)out[count++]=candidate;
    at=end;}
  return count;
}
} // namespace artwork::online

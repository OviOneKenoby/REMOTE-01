#include "rtcp_client/association.hpp"
#include "rtcp_client/wire.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){unsigned n=0;rtcp_client::Association a; a.ipv4={192,168,1,20};for(unsigned i=0;i<16;++i)a.pairing_id[i]=i+1;for(unsigned i=0;i<32;++i)a.psk[i]=i+2;
std::array<std::uint8_t,rtcp_client::association_bytes>b{};assert(rtcp_client::encode(a,b));++n;rtcp_client::Association c;assert(rtcp_client::decode(b,c)&&c.port==46411&&c.psk==a.psk);++n;b[4]=2;assert(!rtcp_client::decode(b,c));++n;
std::uint8_t w[64]{};assert(rtcp_client::wire::identity_request(w)==4&&w[0]==1);++n;assert(rtcp_client::wire::volume_set(42,17,25,w)==17);++n;assert(!rtcp_client::wire::volume_set(0,17,25,w));++n;
std::uint8_t id[]={2,1,1,0,6,'1','.','1','.','1','1'};rtcp_client::wire::Identity identity;assert(rtcp_client::wire::parse(id,identity)&&!strcmp(identity.firmware,"1.1.11"));++n;
std::uint8_t cap[]={4,7,0,0,0,0,0,0,0,1,0,0,7,208,1,100};rtcp_client::wire::Capabilities capabilities;assert(rtcp_client::wire::parse(cap,capabilities));++n;
std::uint8_t snap[]={6,68,1,0,0,0,0,0,0,0,9,0,0,0,0,0,0,0,3};rtcp_client::wire::Snapshot s;assert(rtcp_client::wire::parse(snap,s)&&s.sequence==9);++n;
for(unsigned i=0;i<128;++i){snap[10]=std::uint8_t(i+1);assert(rtcp_client::wire::parse(snap,s)&&s.sequence==std::uint64_t(i+1));}++n;
std::uint8_t result[]={8,0,0,0,0,0,0,0,42,1,0};rtcp_client::wire::Result r;assert(rtcp_client::wire::parse(result,r)&&r.accepted);++n;
std::cout<<"TOTAL "<<n<<"/"<<n<<" PASS\n";}

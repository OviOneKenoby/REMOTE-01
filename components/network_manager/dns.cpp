#include "network_manager/api.hpp"
#include "internal.hpp"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <fcntl.h>
#include <cerrno>
namespace network_manager {
namespace {
// NETWORK owner only; no DNS task, queue, heap record or cross-task socket close.
int socket_fd = -1;
unsigned char rx[513]{}, tx[512]{};
unsigned queries=0, answers=0, rejected=0, errors=0;
bool failure(const char *step) {
  ++errors; ESP_LOGW("NETWORK_DNS", "failure=%s errno=%d", step, errno); dns_stop(); return false;
}
}
bool dns_start() {
  if (socket_fd>=0) return true;
  auto *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  esp_netif_ip_info_t ip{};
  if (!ap || esp_netif_get_ip_info(ap,&ip)!=ESP_OK || ip.ip.addr!=inet_addr("192.168.4.1")) return failure("AP_NETIF");
  // Configure the same supported DNS offer API as the pinned softap_sta example.
  auto stopped=esp_netif_dhcps_stop(ap);
  if (stopped!=ESP_OK && stopped!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) return failure("DHCP_STOP");
  uint8_t offer=2; // DHCPS_OFFER_DNS in the pinned SDK example/dhcpserver options.
  esp_netif_dns_info_t dns{}; dns.ip.type=ESP_IPADDR_TYPE_V4; dns.ip.u_addr.ip4=ip.ip;
  bool ok=esp_netif_dhcps_option(ap,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer,sizeof offer)==ESP_OK &&
    esp_netif_set_dns_info(ap,ESP_NETIF_DNS_MAIN,&dns)==ESP_OK;
  if (esp_netif_dhcps_start(ap)!=ESP_OK || !ok) return failure("DHCP_DNS");
  socket_fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (socket_fd<0) return failure("SOCKET");
  sockaddr_in address{}; address.sin_family=AF_INET; address.sin_port=htons(53); address.sin_addr.s_addr=ip.ip.addr;
  if (bind(socket_fd,reinterpret_cast<sockaddr *>(&address),sizeof address) ||
      fcntl(socket_fd,F_SETFL,O_NONBLOCK)<0) return failure("BIND_NONBLOCK");
  ESP_LOGI("NETWORK_DNS", "running=1 AP_ONLY port=53 owner=NETWORK buffer=512 budget=4"); return true;
}
void dns_stop() { if (socket_fd>=0) { close(socket_fd); socket_fd=-1; } }
bool dns_active() { return socket_fd>=0; }
void dns_tick() {
  if (socket_fd<0) return;
  for(unsigned budget=0;budget<4;++budget) {
    sockaddr_in from{}; socklen_t n=sizeof from;
    int count=recvfrom(socket_fd,rx,sizeof rx,0,reinterpret_cast<sockaddr *>(&from),&n);
    if(count<0) { if(errno!=EAGAIN && errno!=EWOULDBLOCK) ++errors; return; }
    ++queries;
    portal::IPv4 peer{}, sta_ip{}; bool sta_up=false;
    bool ok=from.sin_family==AF_INET && portal::normalize(false,{reinterpret_cast<const unsigned char *>(&from.sin_addr.s_addr),4},peer);
    auto *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(sta && esp_netif_is_netif_up(sta)) {
      sta_up=true; esp_netif_ip_info_t ip{};
      ok=ok && esp_netif_get_ip_info(sta,&ip)==ESP_OK && portal::normalize(false,{reinterpret_cast<const unsigned char *>(&ip.ip.addr),4},sta_ip);
    }
    ok=ok && sta && portal::socket_allowed(portal::address,peer,sta_up,sta_ip);
    auto size=ok?portal::dns_reply(true,{rx,std::size_t(count)},{tx,sizeof tx}):0;
    if(!size) { ++rejected; continue; }
    int sent=sendto(socket_fd,tx,size,0,reinterpret_cast<sockaddr *>(&from),n);
    if(sent!=int(size)) ++errors; else ++answers;
  }
}
void dns_stats(portal::Stats &s) { s.dns=dns_active(); s.dns_queries=queries; s.dns_answers=answers; s.dns_rejected=rejected; s.dns_errors=errors; }
}

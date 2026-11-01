#include "rtcp_client/client.hpp"
#include "rtcp_client/connect_readiness.hpp"
#include "rtcp_client/mdns_discovery.hpp"
#include "network_manager/api.hpp"
#include "esp_random.h"
#include <fcntl.h>
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#include <cerrno>
#include <cstring>

namespace rtcp_client{namespace{
std::uint16_t g16(const std::uint8_t*p){return std::uint16_t(p[0])<<8|p[1];}
std::uint64_t g64(const std::uint8_t*p){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v=(v<<8)|p[i];return v;}
void p16(std::uint8_t*p,std::uint16_t v){p[0]=v>>8;p[1]=v;}
void p64(std::uint8_t*p,std::uint64_t v){for(int i=7;i>=0;--i){p[i]=v;v>>=8;}}
bool eq(const std::uint8_t*a,const std::uint8_t*b,std::size_t n){std::uint8_t d=0;for(std::size_t i=0;i<n;++i)d|=a[i]^b[i];return d==0;}
bool mac(const std::uint8_t*k,std::size_t kn,const std::uint8_t*p,std::size_t n,std::uint8_t o[32]){auto*m=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);return m&&mbedtls_md_hmac(m,k,kn,p,n,o)==0;}
bool labeled(const std::uint8_t*k,const char*l,const std::uint8_t*a,std::size_t an,const std::uint8_t*b,std::size_t bn,const std::uint8_t*c,std::size_t cn,std::uint8_t o[32]){std::array<std::uint8_t,160>x{};auto ln=strlen(l);if(ln+an+bn+cn>x.size())return false;std::size_t at=0;memcpy(x.data()+at,l,ln);at+=ln;memcpy(x.data()+at,a,an);at+=an;memcpy(x.data()+at,b,bn);at+=bn;if(cn)memcpy(x.data()+at,c,cn);bool ok=mac(k,32,x.data(),ln+an+bn+cn,o);memset(x.data(),0,x.size());return ok;}
bool derive(const Association&a,const std::uint8_t cn[16],const std::uint8_t sn[16],const std::uint8_t*t,std::size_t tn,std::uint8_t ck[16],std::uint8_t sk[16],std::uint8_t cp[4],std::uint8_t sp[4],std::uint8_t sid[8]){auto*m=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);std::array<std::uint8_t,47>x{};memcpy(x.data(),"RTCP1 HKDF salt",15);memcpy(x.data()+15,cn,16);memcpy(x.data()+31,sn,16);std::uint8_t salt[32]{},th[32]{};if(!m||mbedtls_md(m,x.data(),x.size(),salt)||mbedtls_md(m,t,tn,th))return false;auto one=[&](const char*l,std::uint8_t*o,std::size_t n){std::array<std::uint8_t,64>info{};auto ln=strlen(l);memcpy(info.data(),l,ln);memcpy(info.data()+ln,th,32);return mbedtls_hkdf(m,salt,32,a.psk.data(),32,info.data(),ln+32,o,n)==0;};bool ok=one("RTCP1 c2s key",ck,16)&&one("RTCP1 s2c key",sk,16)&&one("RTCP1 c2s nonce",cp,4)&&one("RTCP1 s2c nonce",sp,4)&&one("RTCP1 session id",sid,8);memset(salt,0,32);memset(th,0,32);return ok;}
bool encrypt(const std::uint8_t*k,const std::uint8_t*n,const std::uint8_t*aad,const std::uint8_t*p,std::size_t z,std::uint8_t*c,std::uint8_t*tag){mbedtls_gcm_context x;mbedtls_gcm_init(&x);bool ok=!mbedtls_gcm_setkey(&x,MBEDTLS_CIPHER_ID_AES,k,128)&&!mbedtls_gcm_crypt_and_tag(&x,MBEDTLS_GCM_ENCRYPT,z,n,12,aad,28,p,c,16,tag);mbedtls_gcm_free(&x);return ok;}
bool decrypt(const std::uint8_t*k,const std::uint8_t*n,const std::uint8_t*aad,const std::uint8_t*c,std::size_t z,const std::uint8_t*tag,std::uint8_t*p){mbedtls_gcm_context x;mbedtls_gcm_init(&x);bool ok=!mbedtls_gcm_setkey(&x,MBEDTLS_CIPHER_ID_AES,k,128)&&!mbedtls_gcm_auth_decrypt(&x,z,n,12,aad,28,tag,16,c,p);mbedtls_gcm_free(&x);return ok;}
CloseReason close_reason(DiagnosticFailure failure){switch(failure){case DiagnosticFailure::connect:return CloseReason::connect_failed;case DiagnosticFailure::timeout:return CloseReason::response_timeout;case DiagnosticFailure::eof:return CloseReason::peer_eof;case DiagnosticFailure::short_read:return CloseReason::short_read;case DiagnosticFailure::invalid_frame:return CloseReason::invalid_frame;case DiagnosticFailure::proof:return CloseReason::proof_failed;case DiagnosticFailure::send:return CloseReason::send_failed;case DiagnosticFailure::receive:return CloseReason::receive_failed;case DiagnosticFailure::crypto:return CloseReason::crypto_failed;case DiagnosticFailure::identity:return CloseReason::unexpected_record;default:return CloseReason::unexpected_record;}}
}
bool Client::initialize(){if(!load(association_)){state_=State::disabled;diagnostic_.stage=DiagnosticStage::disabled;return false;}state_=State::disconnected;diagnostic_.stage=DiagnosticStage::disconnected;return true;}
void Client::suspend(){discovery_set_network(false);endpoints_.clear();pending_discovery_={};pending_discovery_present_=false;if(state_!=State::disabled&&(socket_>=0||state_!=State::disconnected))close(CloseReason::suspend,false);}
void Client::close(CloseReason reason,bool schedule,std::int32_t error){endpoints_.failed_before_authentication();CloseEvent event{};event.uptime_ms=diagnostic_.previous_poll_ms>UINT32_MAX?UINT32_MAX:std::uint32_t(diagnostic_.previous_poll_ms);event.session=diagnostic_.session;event.network_generation=network_status_.generation;event.socket_error=error;event.request=pending_request_;event.state=std::uint8_t(state_);event.reason=reason;event.awaited=std::uint8_t(awaited_);event.response_pending=awaiting_;diagnostic_.closed(event);if(socket_>=0){lwip_close(socket_);socket_=-1;}memset(c2s_key_.data(),0,16);memset(s2c_key_.data(),0,16);rx_used_=tx_used_=tx_sent_=0;rx_need_=56;tx_counter_=rx_counter_=1;pending_request_=0;awaiting_=false;awaited_=Awaited::none;source_state_due_=menu_state_due_=now_playing_due_=btx_state_due_=false;btx_capabilities_={};full_control_capabilities_={};source_state_={};menu_state_={};catalog_page_={};browser_page_={};btx_negotiation_.reset();now_playing_={};deadline_=0;state_=State::disconnected;if(schedule)retry_at_+=1000;}
bool Client::connect_start(){endpoints_.begin_attempt();const auto endpoint=endpoints_.current(association_);diagnostic_.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);if(!reserve_session(association_)){diagnostic_.fail(DiagnosticFailure::connect);diagnostic_.closed(CloseReason::connect_failed);return false;}socket_=lwip_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(socket_<0){diagnostic_.fail(DiagnosticFailure::connect,errno);diagnostic_.closed(CloseReason::connect_failed,errno);return false;}int no_delay=1;if(lwip_setsockopt(socket_,IPPROTO_TCP,TCP_NODELAY,&no_delay,sizeof no_delay)<0){const int e=errno;diagnostic_.fail(DiagnosticFailure::connect,e);diagnostic_.closed(CloseReason::connect_failed,e);lwip_close(socket_);socket_=-1;return false;}int flags=lwip_fcntl(socket_,F_GETFL,0);if(lwip_fcntl(socket_,F_SETFL,flags|O_NONBLOCK)<0){const int e=errno;diagnostic_.fail(DiagnosticFailure::connect,e);diagnostic_.closed(CloseReason::connect_failed,e);lwip_close(socket_);socket_=-1;return false;}sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(endpoint.port);memcpy(&addr.sin_addr.s_addr,endpoint.ipv4.data(),4);int r=lwip_connect(socket_,reinterpret_cast<sockaddr*>(&addr),sizeof addr);if(r<0&&errno!=EINPROGRESS){const int e=errno;diagnostic_.fail(DiagnosticFailure::connect,e);diagnostic_.closed(CloseReason::connect_failed,e);lwip_close(socket_);socket_=-1;return false;}state_=State::connecting;return true;}
bool Client::make_hello(){memcpy(ch_.data(),"RTCP\x01\x01\x00\x20",8);memcpy(ch_.data()+8,association_.pairing_id.data(),16);esp_fill_random(cn_.data(),16);memcpy(ch_.data()+24,cn_.data(),16);memcpy(tx_.data(),ch_.data(),40);tx_used_=40;tx_sent_=0;return true;}
bool Client::flush(){while(tx_sent_<tx_used_){int n=lwip_send(socket_,tx_.data()+tx_sent_,tx_used_-tx_sent_,MSG_DONTWAIT);if(n>0){tx_sent_+=n;continue;}if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return true;return false;}tx_used_=tx_sent_=0;return true;}
ReadStatus Client::receive(){last_receive_error_=0;for(unsigned budget=0;budget<128&&rx_used_<rx_need_;){int n=lwip_recv(socket_,rx_.data()+rx_used_,rx_need_-rx_used_,MSG_DONTWAIT);const int e=n<0?errno:0;auto status=classify_read(n,e);if(status==ReadStatus::progress){rx_used_+=n;budget+=n;continue;}if(status==ReadStatus::error)last_receive_error_=e;return status;}return ReadStatus::progress;}
bool Client::accept_server_hello(){diagnostic_.mark(DiagnosticStage::server_hello_received,DiagnosticCounter::server_hello_received);if(memcmp(rx_.data(),"RTCP\x01\x02\x00\x30",8)){diagnostic_.fail(DiagnosticFailure::invalid_frame);return false;}diagnostic_.mark(DiagnosticStage::server_frame_valid,DiagnosticCounter::server_frame_valid);memcpy(sh_.data(),rx_.data(),56);memcpy(sn_.data(),rx_.data()+8,16);std::uint8_t proof[32]{};if(!labeled(association_.psk.data(),"RTCP1 server proof",ch_.data(),40,sh_.data(),24,nullptr,0,proof)){diagnostic_.fail(DiagnosticFailure::crypto);return false;}if(!eq(proof,sh_.data()+24,32)){diagnostic_.fail(DiagnosticFailure::proof);return false;}diagnostic_.mark(DiagnosticStage::server_proof_valid,DiagnosticCounter::server_proof_valid);memcpy(tx_.data(),"RTCP\x01\x03\x00\x20",8);if(!labeled(association_.psk.data(),"RTCP1 client proof",ch_.data(),40,sh_.data(),56,tx_.data(),8,tx_.data()+8)){diagnostic_.fail(DiagnosticFailure::crypto);return false;}std::array<std::uint8_t,136>tr{};memcpy(tr.data(),ch_.data(),40);memcpy(tr.data()+40,sh_.data(),56);memcpy(tr.data()+96,tx_.data(),40);if(!derive(association_,cn_.data(),sn_.data(),tr.data(),tr.size(),c2s_key_.data(),s2c_key_.data(),c2s_prefix_.data(),s2c_prefix_.data(),session_id_.data())){diagnostic_.fail(DiagnosticFailure::crypto);return false;}tx_used_=40;tx_sent_=0;rx_used_=0;rx_need_=28;state_=State::authenticated;return true;}
bool Client::send_plain(const std::uint8_t*p,std::size_t z){if(tx_used_||z>512||tx_counter_==UINT64_MAX)return false;memcpy(tx_.data(),"RTCR\x01\x01\x00\x00",8);memcpy(tx_.data()+8,session_id_.data(),8);p64(tx_.data()+16,tx_counter_);p16(tx_.data()+24,z);tx_[26]=tx_[27]=0;std::uint8_t nonce[12]{};memcpy(nonce,c2s_prefix_.data(),4);p64(nonce+4,tx_counter_);if(!encrypt(c2s_key_.data(),nonce,tx_.data(),p,z,tx_.data()+28,tx_.data()+28+z))return false;tx_used_=28+z+16;tx_sent_=0;++tx_counter_;return true;}
bool Client::queue_next(){std::uint8_t p[32]{};std::size_t n=0;Awaited expected=Awaited::none;if(awaiting_)return false;if(state_==State::authenticated){n=wire::identity_request(p);state_=State::identity;expected=Awaited::identity;}else if(state_==State::identity){n=wire::capability_request(p);state_=State::capabilities;expected=Awaited::capabilities;}else if(state_==State::capabilities){n=wire::capability_request(p);state_=State::btx_probe;expected=Awaited::btx_capabilities;}else if(state_==State::btx_probe&&identity_.full_control_advertised()){n=wire::full_control_capability_request(p);state_=State::full_control;expected=Awaited::full_control_capabilities;}else if(state_==State::btx_probe||state_==State::full_control){n=wire::snapshot_request(p);state_=State::ready;expected=Awaited::snapshot;}if(!n)return true;if(!send_plain(p,n))return false;awaiting_=true;awaited_=expected;return true;}
Client::Event Client::accept_record(){if(rx_used_<28)return Event::none;const bool identity_expected=awaited_==Awaited::identity;if(rx_need_==28){if(memcmp(rx_.data(),"RTCR\x01\x01\x00\x00",8)||!eq(rx_.data()+8,session_id_.data(),8)||g64(rx_.data()+16)!=rx_counter_||rx_[26]||rx_[27]){diagnostic_.fail(DiagnosticFailure::invalid_frame);return Event::disconnected;}auto z=g16(rx_.data()+24);if(z>512){diagnostic_.fail(DiagnosticFailure::invalid_frame);return Event::disconnected;}rx_need_=28+z+16;if(rx_used_<rx_need_)return Event::none;}if(identity_expected)diagnostic_.mark(DiagnosticStage::identity_response_received,DiagnosticCounter::identity_response_received);auto z=g16(rx_.data()+24);std::array<std::uint8_t,512>p{};std::uint8_t nonce[12]{};memcpy(nonce,s2c_prefix_.data(),4);p64(nonce+4,rx_counter_);if(!decrypt(s2c_key_.data(),nonce,rx_.data(),rx_.data()+28,z,rx_.data()+28+z,p.data())){diagnostic_.fail(DiagnosticFailure::crypto);return Event::disconnected;}++rx_counter_;Event e=Event::none;auto span=std::span<const std::uint8_t>(p.data(),z);switch(awaited_){
case Awaited::identity:if(wire::parse(span,identity_)){e=Event::identity;diagnostic_.mark(DiagnosticStage::identity_accepted,DiagnosticCounter::identity_accepted);endpoints_.authenticated(association_);}break;
  case Awaited::capabilities:if(wire::parse(span,capabilities_)&&btx_negotiation_.first(capabilities_))e=Event::negotiation;break;
  case Awaited::btx_capabilities:
    if(btx_negotiation_.second(span,capabilities_,btx_capabilities_)!=
       wire::BtxProbeResult::invalid)e=Event::capabilities;
    break;
  case Awaited::full_control_capabilities:if(wire::parse(span,full_control_capabilities_))e=Event::full_control;break;
  case Awaited::snapshot:if(wire::parse(span,snapshot_)){e=Event::snapshot;source_state_due_=full_control_capabilities_.source_switch();menu_state_due_=full_control_capabilities_.menu_projection();btx_state_due_=btx_capabilities_.negotiated;now_playing_due_=!btx_state_due_&&capabilities_.now_playing;}break;
case Awaited::source_state:if(wire::parse(span,source_state_))e=Event::source_state;break;
case Awaited::menu_state:if(wire::parse(span,menu_state_))e=Event::menu_state;break;
case Awaited::catalog:if(wire::parse(span,catalog_page_))e=Event::catalog;break;
case Awaited::browser:if(wire::parse(span,browser_page_))e=Event::browser;break;
case Awaited::now_playing:if(wire::parse(span,now_playing_))e=Event::now_playing;break;
  case Awaited::btx_state:if(wire::parse_btx_state(span,now_playing_)&&wire::permitted(btx_capabilities_,now_playing_)){e=Event::btx_state;now_playing_due_=now_playing_.btx_source==std::uint8_t(wire::BtxSource::radio)&&capabilities_.now_playing;}break;
case Awaited::result:if(wire::parse(span,result_)&&result_.request==pending_request_){pending_request_=0;snapshot_at_=0;source_state_due_=full_control_capabilities_.source_switch();menu_state_due_=full_control_capabilities_.menu_projection();e=Event::result;}break;
case Awaited::none:break;
}
  if(e==Event::none){diagnostic_.fail(DiagnosticFailure::identity);return Event::disconnected;}awaiting_=false;awaited_=Awaited::none;deadline_=0;rx_used_=0;rx_need_=28;if((e==Event::identity||e==Event::negotiation||e==Event::capabilities||e==Event::full_control)&&!queue_next()){diagnostic_.fail(DiagnosticFailure::send);return Event::disconnected;}if(e==Event::capabilities&&!identity_.full_control_advertised())diagnostic_.stage=DiagnosticStage::complete;if(e==Event::full_control)diagnostic_.stage=DiagnosticStage::complete;return e;}
bool Client::set_volume(std::uint64_t id,std::uint32_t n,std::uint32_t d){if(state_!=State::ready||pending_request_||awaiting_)return false;std::uint8_t p[17]{};auto z=wire::volume_set(id,n,d,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
bool Client::playback(std::uint64_t id,wire::PlaybackOperation operation,std::uint8_t expected,std::uint64_t generation){if(state_!=State::ready||pending_request_||awaiting_||!capabilities_.supports(operation))return false;std::uint8_t p[19]{};auto z=wire::playback_command(id,operation,expected,generation,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
bool Client::bluetooth(std::uint64_t id,wire::PlaybackOperation operation,wire::BtxPlayback expected,std::uint64_t generation){if(state_!=State::ready||pending_request_||awaiting_||!btx_capabilities_.supports(operation))return false;std::uint8_t p[19]{};auto z=wire::btx_command(id,operation,expected,generation,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
bool Client::source_select(std::uint64_t id,wire::Source target,std::uint64_t generation){if(state_!=State::ready||pending_request_||awaiting_||!full_control_capabilities_.source_switch())return false;std::uint8_t p[18]{};auto z=wire::source_select(id,target,generation,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
bool Client::catalog_query(wire::CatalogCollection collection,std::uint16_t offset,std::uint8_t limit){if(state_!=State::ready||pending_request_||awaiting_||!full_control_capabilities_.catalog_query())return false;std::uint8_t p[5]{};auto z=wire::catalog_query(collection,offset,limit,p);if(!z||!send_plain(p,z))return false;awaiting_=true;awaited_=Awaited::catalog;return true;}
bool Client::catalog_command(std::uint64_t id,wire::CatalogOperation operation,wire::CatalogCollection collection,std::uint16_t token,std::uint64_t revision){if(state_!=State::ready||pending_request_||awaiting_||!full_control_capabilities_.catalog_command()||(operation==wire::CatalogOperation::play&&!full_control_capabilities_.deferred_catalog_play()))return false;std::uint8_t p[21]{};auto z=wire::catalog_command(id,operation,collection,token,revision,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
bool Client::browser_state(wire::BrowserStage stage,std::uint16_t offset,std::uint8_t limit,std::uint8_t filter,std::uint64_t generation){if(state_!=State::ready||pending_request_||awaiting_||!full_control_capabilities_.radio_browser())return false;std::uint8_t p[14]{};auto z=wire::browser_state_request(stage,offset,limit,filter,generation,p);if(!z||!send_plain(p,z))return false;awaiting_=true;awaited_=Awaited::browser;return true;}
bool Client::browser_command(std::uint64_t id,wire::BrowserOperation operation,std::uint16_t token,std::uint64_t generation,std::uint64_t revision){if(state_!=State::ready||pending_request_||awaiting_||!full_control_capabilities_.radio_browser())return false;std::uint8_t p[29]{};auto z=wire::browser_command(id,operation,token,generation,revision,p);if(!z||!send_plain(p,z))return false;pending_request_=id;awaiting_=true;awaited_=Awaited::result;return true;}
Client::Event Client::poll(std::uint64_t now,bool command_waiting){
  diagnostic_.polled(now);
  if(state_==State::disabled)return Event::none;
  network_manager::Status net{};
  const bool status_available=network_manager::read_status(net);
  const auto network=network_status_.observe(status_available,net.connected,net.generation,socket_>=0);
  if(network==NetworkDecision::unavailable)return Event::none;
  if(network==NetworkDecision::disconnected||network==NetworkDecision::generation_changed){
    discovery_set_network(false);endpoints_.clear();pending_discovery_={};pending_discovery_present_=false;
    if(socket_>=0){close(CloseReason::network_lost,false);return Event::disconnected;}
    return Event::none;
  }
  if(!discovery_ready_&&now>=discovery_at_){
    discovery_ready_=discovery_initialize();
    discovery_at_=discovery_ready_?now:now+discovery_query_interval_ms;
  }
  discovery_set_network(discovery_ready_);
  DiscoveryBatch batch{};
  if(discovery_ready_&&discovery_take(batch)){pending_discovery_=batch;pending_discovery_present_=true;}
  if(awaiting_&&!deadline_)deadline_=now+response_close_deadline_ms;
  if((state_==State::connecting||state_==State::server_hello)&&deadline_&&now>=deadline_){
    diagnostic_.server_hello_bytes=state_==State::server_hello?std::uint16_t(rx_used_):diagnostic_.server_hello_bytes;
    const bool partial=rx_used_!=0;
    if(!partial&&state_!=State::connecting){
      int queued=-1;if(socket_>=0){int available=0;if(lwip_ioctl(socket_,FIONREAD,&available)==0&&available>=0)queued=available;}
      const auto started=deadline_>=3000?deadline_-3000:0;
      diagnostic_.response_timeout(std::uint8_t(state_),now>=started?now-started:0,queued);
    }
    diagnostic_.fail(partial?DiagnosticFailure::short_read:DiagnosticFailure::timeout);
    close(partial?CloseReason::short_read:(state_==State::connecting?CloseReason::connect_timeout:CloseReason::response_timeout));
    return Event::disconnected;
  }
  if(state_==State::disconnected){
    if(pending_discovery_present_){
      endpoints_.replace(pending_discovery_.endpoints.data(),pending_discovery_.count);
      pending_discovery_={};pending_discovery_present_=false;
    }
    if(discovery_ready_&&now>=discovery_at_){
      if(discovery_request())discovery_at_=now+discovery_query_interval_ms;
      else discovery_at_=now+250;
    }
    if(now<retry_at_)return Event::none;
    retry_at_=now+1000;
    if(!connect_start()){endpoints_.failed_before_authentication();return Event::none;}
    deadline_=now+3000;
  }
  if(state_==State::connecting){
    fd_set writable;FD_ZERO(&writable);FD_SET(socket_,&writable);timeval wait{};
    int selected=lwip_select(socket_+1,nullptr,&writable,nullptr,&wait);
    int select_error=selected<0?errno:0;int error=0;socklen_t n=sizeof error;int option_result=0;
    bool ready=selected>0&&FD_ISSET(socket_,&writable);
    if(ready)option_result=lwip_getsockopt(socket_,SOL_SOCKET,SO_ERROR,&error,&n);
    auto check=classify_connect_readiness(selected,ready,select_error,option_result,error);
    if(check.state==ConnectReadiness::pending)return Event::none;
    if(check.state==ConnectReadiness::failed){diagnostic_.fail(DiagnosticFailure::connect,check.error);close(CloseReason::connect_failed,true,check.error);return Event::none;}
    diagnostic_.mark(DiagnosticStage::tcp_connected,DiagnosticCounter::tcp_connected);
    if(!make_hello()){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::none;}
    state_=State::server_hello;
  }
  const auto sending=state_;const bool had_tx=tx_used_!=0;
  if(!flush()){const int e=errno;diagnostic_.fail(DiagnosticFailure::send,e);close(CloseReason::send_failed,true,e);return Event::disconnected;}
  if(had_tx&&!tx_used_){
    if(sending==State::server_hello)diagnostic_.mark(DiagnosticStage::client_hello_sent,DiagnosticCounter::client_hello_sent);
    else if(sending==State::authenticated)diagnostic_.mark(DiagnosticStage::client_finish_sent,DiagnosticCounter::client_finish_sent);
    else if(sending==State::identity)diagnostic_.mark(DiagnosticStage::identity_request_sent,DiagnosticCounter::identity_request_sent);
  }
  if(state_==State::authenticated&&!tx_used_){
    deadline_=0;if(!queue_next()){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::queue_failed);return Event::disconnected;}
    return Event::connected;
  }
  const auto before=rx_used_;const auto reading=receive();
  if(reading==ReadStatus::eof||reading==ReadStatus::error){
    diagnostic_.server_hello_bytes=state_==State::server_hello?std::uint16_t(rx_used_):diagnostic_.server_hello_bytes;
    const bool partial=rx_used_||before;
    if(reading==ReadStatus::eof){diagnostic_.fail(partial?DiagnosticFailure::short_read:DiagnosticFailure::eof);close(partial?CloseReason::short_read:CloseReason::peer_eof);}
    else{const int e=last_receive_error_;diagnostic_.fail(DiagnosticFailure::receive,e);close(CloseReason::receive_failed,true,e);}
    return Event::disconnected;
  }
  if(state_==State::server_hello){
    diagnostic_.server_hello_bytes=std::uint16_t(rx_used_);
    if(rx_used_&&rx_used_<56)diagnostic_.stage=DiagnosticStage::server_hello_receiving;
  }
  if(state_==State::server_hello&&rx_used_==56){
    if(!accept_server_hello()){close(close_reason(diagnostic_.failure),true,diagnostic_.error);return Event::disconnected;}
    return Event::none;
  }
  if(state_>=State::authenticated){
    auto e=accept_record();
    if(e==Event::disconnected){close(close_reason(diagnostic_.failure),true,diagnostic_.error);return e;}
    // Preserve the three-second service deadline and allow only one bounded
    // 64 ms drain window. Paired hardware evidence proved that TUNER can fully
    // write a valid response just after the service boundary while REMOTE is
    // already executing its timeout poll. receive() above remains nonblocking
    // and bounded to 128 bytes; no request is retried or replayed.
    const bool response_deadline_expired = awaiting_&&deadline_&&now>=deadline_;
    if(response_deadline_requires_close(response_deadline_expired,awaiting_)){
      const bool partial=rx_used_!=0;
      if(!partial){
        int queued=-1;if(socket_>=0){int available=0;if(lwip_ioctl(socket_,FIONREAD,&available)==0&&available>=0)queued=available;}
        const auto started=deadline_>=response_close_deadline_ms?
                           deadline_-response_close_deadline_ms:0;
        diagnostic_.response_timeout(std::uint8_t(state_),now>=started?now-started:0,queued);
      }
      diagnostic_.fail(partial?DiagnosticFailure::short_read:DiagnosticFailure::timeout);
      close(partial?CloseReason::short_read:CloseReason::response_timeout);
      return Event::disconnected;
    }
    if(!command_waiting&&state_==State::ready&&!pending_request_&&!awaiting_&&source_state_due_&&!tx_used_){
      std::uint8_t p[1];auto z=wire::source_state_request(p);
      if(!send_plain(p,z)){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::disconnected;}
      awaiting_=true;awaited_=Awaited::source_state;source_state_due_=false;
    }else if(!command_waiting&&state_==State::ready&&!pending_request_&&!awaiting_&&menu_state_due_&&!tx_used_){
      std::uint8_t p[1];auto z=wire::menu_state_request(p);
      if(!send_plain(p,z)){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::disconnected;}
      awaiting_=true;awaited_=Awaited::menu_state;menu_state_due_=false;
    }else if(!command_waiting&&state_==State::ready&&!pending_request_&&!awaiting_&&btx_state_due_&&!tx_used_){
      std::uint8_t p[1];auto z=wire::btx_state_request(p);
      if(!send_plain(p,z)){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::disconnected;}
      awaiting_=true;awaited_=Awaited::btx_state;btx_state_due_=false;
    }else if(!command_waiting&&state_==State::ready&&!pending_request_&&!awaiting_&&now_playing_due_&&!tx_used_){
      std::uint8_t p[1];auto z=wire::now_playing_request(p);
      if(!send_plain(p,z)){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::disconnected;}
      awaiting_=true;awaited_=Awaited::now_playing;now_playing_due_=false;
    }else if(!command_waiting&&state_==State::ready&&!pending_request_&&!awaiting_&&now>=snapshot_at_&&!tx_used_){
      std::uint8_t p[1];auto z=wire::snapshot_request(p);
      if(!send_plain(p,z)){diagnostic_.fail(DiagnosticFailure::send);close(CloseReason::send_failed);return Event::disconnected;}
      awaiting_=true;awaited_=Awaited::snapshot;snapshot_at_=now+1000;
    }
    return e;
  }
  return Event::none;
}
}


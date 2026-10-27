#include "rtcp_client/diagnostic.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace rtcp_client;
int main(){unsigned n=0;Diagnostic d;assert(d.stage==DiagnosticStage::disabled&&d.failure==DiagnosticFailure::none);++n;
d.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);assert(d.counters[0]==1&&d.failure==DiagnosticFailure::none);++n;
d.fail(DiagnosticFailure::timeout,110);assert(d.failure==DiagnosticFailure::timeout&&d.error==110&&d.counters[10]==1);++n;
d.mark(DiagnosticStage::tcp_connected,DiagnosticCounter::tcp_connected);assert(d.failure==DiagnosticFailure::none&&d.error==0&&d.counters[1]==1);++n;
d.server_hello_bytes=17;d.fail(DiagnosticFailure::short_read);assert(d.server_hello_bytes==17&&d.counters[10]==2);++n;
d.closed(CloseReason::short_read,104);assert(d.close_reason==CloseReason::short_read&&d.close_count==1&&d.error==104);++n;
d.mark(DiagnosticStage::identity_accepted,DiagnosticCounter::identity_accepted);CloseEvent event{};event.uptime_ms=123;event.session=d.session;event.network_generation=7;event.socket_error=104;event.request=9;event.state=8;event.reason=CloseReason::receive_failed;event.awaited=12;event.response_pending=1;d.closed(event);assert(d.close_history.count==1&&d.close_history.newest(0).socket_error==104&&d.close_history.newest(0).request==9);++n;
for(unsigned i=0;i<5;++i){event.uptime_ms=200+i;d.closed(event);}assert(d.close_history.count==4&&d.close_history.newest(0).uptime_ms==204&&d.close_history.newest(3).uptime_ms==201);++n;
d.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);assert(d.close_reason==CloseReason::receive_failed&&d.close_count==7&&d.close_history.count==4);++n;
d.close_count=std::numeric_limits<std::uint32_t>::max();d.closed(CloseReason::network_lost);assert(d.close_count==std::numeric_limits<std::uint32_t>::max()&&d.close_reason==CloseReason::network_lost);++n;
d.counters[0]=std::numeric_limits<std::uint32_t>::max();d.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);assert(d.counters[0]==std::numeric_limits<std::uint32_t>::max());++n;
d.polled(0);d.polled(17);d.polled(3061);assert(d.last_poll_interval_ms==3044&&d.max_poll_interval_ms==3044);++n;
d.response_timeout(6,3044,27);assert(d.last_response_timeout_state==6&&d.last_response_timeout_elapsed_ms==3044&&d.last_response_timeout_queued_bytes==27&&d.response_timeouts_with_unread==1);++n;
d.mark(DiagnosticStage::complete,DiagnosticCounter::identity_accepted);assert(d.last_response_timeout_state==6&&d.last_response_timeout_queued_bytes==27&&d.response_timeouts_with_unread==1&&d.close_history.count==4);++n;
d.response_timeouts_with_unread=std::numeric_limits<std::uint32_t>::max();d.response_timeout(7,3000,1);assert(d.response_timeouts_with_unread==std::numeric_limits<std::uint32_t>::max());++n;
std::cout<<"RTCP_DIAGNOSTIC: PASS="<<n<<" FAIL=0 assertions="<<n<<"\n";}

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
d.mark(DiagnosticStage::identity_accepted,DiagnosticCounter::identity_accepted);CloseEvent event{};event.uptime_ms=123;event.session=d.session;event.network_generation=7;event.socket_error=104;event.request=9;event.state=8;event.reason=CloseReason::receive_failed;event.awaited=12;event.response_pending=1;event.request_type=7;event.reset_reason=1;event.poll_last_32ms=2;event.poll_max_32ms=7;d.closed(event);assert(d.close_history.count==1&&d.close_history.newest(0).socket_error==104&&d.close_history.newest(0).request==9&&d.close_history.newest(0).request_type==7&&d.close_history.newest(0).reset_reason==1&&d.close_history.newest(0).poll_last_32ms==2&&d.close_history.newest(0).poll_max_32ms==7);++n;
for(unsigned i=0;i<5;++i){event.uptime_ms=200+i;d.closed(event);}assert(d.close_history.count==4&&d.close_history.newest(0).uptime_ms==204&&d.close_history.newest(3).uptime_ms==201);++n;
d.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);assert(d.close_reason==CloseReason::receive_failed&&d.close_count==7&&d.close_history.count==4);++n;
d.close_count=std::numeric_limits<std::uint32_t>::max();d.closed(CloseReason::network_lost);assert(d.close_count==std::numeric_limits<std::uint32_t>::max()&&d.close_reason==CloseReason::network_lost);++n;
d.counters[0]=std::numeric_limits<std::uint32_t>::max();d.mark(DiagnosticStage::tcp_connecting,DiagnosticCounter::attempts);assert(d.counters[0]==std::numeric_limits<std::uint32_t>::max());++n;
d.polled(0);d.polled(17);d.polled(3061);assert(d.last_poll_interval_ms==3044&&d.max_poll_interval_ms==3044);++n;
d.response_timeout(6,3044,27);assert(d.last_response_timeout_state==6&&d.last_response_timeout_elapsed_ms==3044&&d.last_response_timeout_queued_bytes==27&&d.response_timeouts_with_unread==1);++n;
d.mark(DiagnosticStage::complete,DiagnosticCounter::identity_accepted);assert(d.last_response_timeout_state==6&&d.last_response_timeout_queued_bytes==27&&d.response_timeouts_with_unread==1&&d.close_history.count==4);++n;
d.response_timeouts_with_unread=std::numeric_limits<std::uint32_t>::max();d.response_timeout(7,3000,1);assert(d.response_timeouts_with_unread==std::numeric_limits<std::uint32_t>::max());++n;
Diagnostic first;assert(!first.first_ready_disconnect.valid());++n;
first.closed(CloseReason::connect_failed);assert(!first.first_ready_disconnect.valid());++n;
CloseEvent startup{};startup.uptime_ms=10;startup.state=2;startup.reason=CloseReason::connect_timeout;first.closed(startup,false);assert(!first.first_ready_disconnect.valid());++n;
startup.state=3;startup.reason=CloseReason::response_timeout;first.closed(startup,false);assert(!first.first_ready_disconnect.valid());++n;
CloseEvent ready{};ready.uptime_ms=1234;ready.session=9;ready.network_generation=4;ready.socket_error=104;ready.request=77;ready.state=9;ready.reason=CloseReason::receive_failed;ready.awaited=12;ready.response_pending=1;ready.request_type=28;ready.reset_reason=3;ready.poll_last_32ms=3;ready.poll_max_32ms=255;first.snapshot_accepted();first.closed(ready,true);assert(first.first_ready_disconnect.valid()&&first.first_ready_disconnect.event.uptime_ms==1234&&first.first_ready_disconnect.event.socket_error==104&&first.first_ready_disconnect.event.request==77);++n;
const auto captured=first.first_ready_disconnect.event;for(unsigned i=0;i<5;++i){startup.uptime_ms=2000+i;startup.state=i%2?3:2;startup.reason=i%2?CloseReason::response_timeout:CloseReason::connect_timeout;first.closed(startup,false);}assert(first.close_history.count==4&&first.close_history.newest(0).uptime_ms==2004&&first.first_ready_disconnect.event.uptime_ms==captured.uptime_ms);++n;
first.mark(DiagnosticStage::complete,DiagnosticCounter::identity_accepted);assert(first.first_ready_disconnect.valid()&&first.first_ready_disconnect.event.uptime_ms==captured.uptime_ms);++n;
ready.uptime_ms=9999;ready.reason=CloseReason::peer_eof;first.snapshot_accepted();first.closed(ready,true);assert(first.first_ready_disconnect.event.uptime_ms==captured.uptime_ms&&first.first_ready_disconnect.event.reason==captured.reason);++n;
Diagnostic planned;ready.reason=CloseReason::suspend;planned.snapshot_accepted();planned.closed(ready,true);assert(!planned.first_ready_disconnect.valid());++n;
first.reset_first_ready_disconnect();assert(!first.first_ready_disconnect.valid()&&first.first_ready_disconnect.event.uptime_ms==0);++n;
// Task 003: capture requires State::ready AND an accepted snapshot in this session.
{
  CloseEvent e{};e.state=9;e.session=3;e.request=41;e.request_type=17;e.socket_error=0;
  Diagnostic a;e.reason=CloseReason::send_failed;a.closed(e,true);
  assert(!a.first_ready_disconnect.valid()&&a.close_history.count==1);++n; // (a) first snapshot request send failure: ready state, no snapshot yet
  Diagnostic b;b.session_started();e.reason=CloseReason::response_timeout;b.closed(e,true);
  assert(!b.first_ready_disconnect.valid());++n; // (b) timeout awaiting first snapshot
  e.reason=CloseReason::peer_eof;b.closed(e,true);e.reason=CloseReason::short_read;b.closed(e,true);
  assert(!b.first_ready_disconnect.valid()&&b.close_history.count==3);++n; // (b) EOF/short_read, history still records
  const CloseReason reasons[]={CloseReason::response_timeout,CloseReason::peer_eof,CloseReason::receive_failed};
  for(auto reason:reasons){
    Diagnostic c;c.session_started();c.snapshot_accepted();
    CloseEvent r{};r.state=9;r.session=7;r.request=99;r.request_type=28;r.socket_error=reason==CloseReason::receive_failed?104:0;r.reason=reason;r.uptime_ms=555;
    c.closed(r,true);
    assert(c.first_ready_disconnect.valid()&&c.first_ready_disconnect.event.reason==reason&&c.first_ready_disconnect.event.socket_error==r.socket_error&&c.first_ready_disconnect.event.request==99&&c.first_ready_disconnect.event.request_type==28&&c.first_ready_disconnect.event.session==7);++n; // (c)
  }
  Diagnostic d;d.session_started();d.snapshot_accepted();CloseEvent r{};r.state=9;r.reason=CloseReason::receive_failed;r.socket_error=104;r.request=5;r.uptime_ms=100;d.closed(r,true);
  const auto kept=d.first_ready_disconnect.event;
  for(unsigned i=0;i<3;++i){d.session_started();CloseEvent s{};s.state=3;s.reason=CloseReason::response_timeout;s.uptime_ms=200+i;d.closed(s,false);s.state=2;s.reason=CloseReason::connect_timeout;d.closed(s,false);}
  d.session_started();d.snapshot_accepted();CloseEvent later{};later.state=9;later.reason=CloseReason::peer_eof;later.uptime_ms=999;d.closed(later,true);
  assert(d.first_ready_disconnect.event.uptime_ms==kept.uptime_ms&&d.first_ready_disconnect.event.reason==kept.reason&&d.first_ready_disconnect.event.socket_error==104&&d.close_history.newest(0).uptime_ms==999);++n; // (d)
  Diagnostic f;f.session_started();f.snapshot_accepted();CloseEvent t{};t.state=8;t.reason=CloseReason::network_lost;f.closed(t,false);
  CloseEvent u{};u.state=9;u.reason=CloseReason::response_timeout;f.closed(u,true);
  assert(!f.first_ready_disconnect.valid()&&!f.snapshot_received);++n; // (e) flag consumed by previous close; ready-without-snapshot after restart stays invalid
  Diagnostic g;g.session_started();g.snapshot_accepted();assert(g.snapshot_received);g.session_started();assert(!g.snapshot_received);++n; // flag cleared at session start
  Diagnostic h;h.snapshot_accepted();h.reset_first_ready_disconnect();assert(!h.snapshot_received&&!h.first_ready_disconnect.valid());++n; // initialize clears
  Diagnostic w;w.snapshot_accepted();CloseEvent nl{};nl.state=9;nl.reason=CloseReason::network_lost;w.closed(nl,true);
  assert(w.first_ready_disconnect.valid()&&w.first_ready_disconnect.event.reason==CloseReason::network_lost);++n; // network_lost stays captured
}
std::cout<<"RTCP_DIAGNOSTIC: PASS="<<n<<" FAIL=0 assertions="<<n<<"\n";}

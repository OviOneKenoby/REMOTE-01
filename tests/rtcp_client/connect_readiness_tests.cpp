#include "rtcp_client/connect_readiness.hpp"
#include <cassert>
#include <iostream>

using namespace rtcp_client;
int main() {
  unsigned passed = 0;
  auto check = [&](ConnectCheck actual, ConnectReadiness state, int error) {
    assert(actual.state == state && actual.error == error); ++passed;
  };
  check(classify_connect_readiness(0,false,0,0,0),ConnectReadiness::pending,0);
  check(classify_connect_readiness(-1,false,EINTR,0,0),ConnectReadiness::pending,0);
  check(classify_connect_readiness(1,true,0,0,0),ConnectReadiness::ready,0);
  check(classify_connect_readiness(1,true,0,0,EINPROGRESS),ConnectReadiness::pending,0);
  check(classify_connect_readiness(-1,false,EIO,0,0),ConnectReadiness::failed,EIO);
  check(classify_connect_readiness(1,true,EIO,-1,0),ConnectReadiness::failed,EIO);
  check(classify_connect_readiness(1,true,0,0,ECONNREFUSED),ConnectReadiness::failed,ECONNREFUSED);
  assert(!connect_deadline_expired(2999,3000)); ++passed;
  assert(connect_deadline_expired(3000,3000)); ++passed;
  assert(response_deadline_requires_close(true,true)); ++passed;
  assert(!response_deadline_requires_close(true,false)); ++passed;
  assert(!response_deadline_requires_close(false,true)); ++passed;
  std::cout << passed << "/" << passed << " PASS\n";
}

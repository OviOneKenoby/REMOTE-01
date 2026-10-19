#include "rtcp_client/io_status.hpp"
#include <cassert>
#include <cerrno>
#include <iostream>
using namespace rtcp_client;
int main(){unsigned n=0;
assert(classify_read(8,0)==ReadStatus::progress);++n;
assert(classify_read(0,0)==ReadStatus::eof);++n;
assert(classify_read(-1,EAGAIN)==ReadStatus::would_block);++n;
assert(classify_read(-1,EWOULDBLOCK)==ReadStatus::would_block);++n;
assert(classify_read(-1,ECONNRESET)==ReadStatus::error);++n;
std::cout<<"RTCP_IO_STATUS: PASS="<<n<<" FAIL=0 assertions="<<n<<"\n";}

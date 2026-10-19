#pragma once
#include <cerrno>

namespace rtcp_client {
enum class ReadStatus { progress, would_block, eof, error };

constexpr ReadStatus classify_read(int result, int error) {
  if (result > 0) return ReadStatus::progress;
  if (result == 0) return ReadStatus::eof;
  return error == EAGAIN || error == EWOULDBLOCK ? ReadStatus::would_block
                                                 : ReadStatus::error;
}
} // namespace rtcp_client

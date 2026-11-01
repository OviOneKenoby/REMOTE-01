#include "ota_service/model.hpp"

#include <cstring>

namespace ota_service {
namespace {
constexpr std::size_t image_header_size = 24;
constexpr std::size_t segment_header_size = 8;
constexpr std::size_t app_desc_offset = image_header_size + segment_header_size;
constexpr std::uint8_t image_magic = 0xe9;
constexpr std::uint16_t esp32s3_chip_id = 0x0009;
constexpr std::uint32_t app_desc_magic = 0xabcd5432;
constexpr std::size_t project_offset = app_desc_offset + 48;
constexpr std::size_t project_size = 32;
constexpr char project_name[] = "REMOTE01";

std::uint16_t little16(const unsigned char *p) {
  return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}
std::uint32_t little32(const unsigned char *p) {
  return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
         (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

bool Policy::challenge(U64 now) {
  if (state == State::UPLOADING || state == State::VERIFIED) return false;
  state = State::CHALLENGE;
  expires = now + challenge_lifetime_ms;
  failures = 0;
  return true;
}

AuthResult Policy::authenticate(U64 now, bool proof_matches) {
  if (state == State::UPLOADING || state == State::VERIFIED)
    return AuthResult::BUSY;
  if (state != State::CHALLENGE) return AuthResult::REFUSED;
  if (now > expires) {
    revoke();
    return AuthResult::EXPIRED;
  }
  if (!proof_matches) {
    if (++failures >= maximum_auth_failures) {
      revoke();
      return AuthResult::LOCKED;
    }
    return AuthResult::REFUSED;
  }
  state = State::AUTHENTICATED;
  expires = now + session_lifetime_ms;
  failures = 0;
  return AuthResult::ACCEPTED;
}

bool Policy::begin_upload(U64 now, bool session_matches) {
  if (state != State::AUTHENTICATED || now > expires || !session_matches) {
    if (state == State::AUTHENTICATED && now > expires) revoke();
    return false;
  }
  state = State::UPLOADING;
  expires = 0;
  return true;
}

void Policy::finish(bool verified) {
  if (state == State::UPLOADING)
    state = verified ? State::VERIFIED : State::FAILED;
}
void Policy::revoke() {
  state = State::IDLE;
  expires = 0;
  failures = 0;
}
bool Policy::busy(U64 now) const {
  return state == State::UPLOADING || state == State::VERIFIED ||
         ((state == State::CHALLENGE || state == State::AUTHENTICATED) &&
          now <= expires);
}

bool decode_hex(std::string_view text, unsigned char *out, std::size_t count) {
  if (!out || text.size() != count * 2) return false;
  for (std::size_t i = 0; i < count; ++i) {
    const int hi = nibble(text[2 * i]);
    const int lo = nibble(text[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<unsigned char>((hi << 4) | lo);
  }
  return true;
}

bool constant_time_equal(const unsigned char *a, const unsigned char *b,
                         std::size_t count) {
  if (!a || !b) return false;
  unsigned char difference = 0;
  for (std::size_t i = 0; i < count; ++i) difference |= a[i] ^ b[i];
  return difference == 0;
}

ImageIdentity inspect_image_prefix(const unsigned char *data, std::size_t size) {
  ImageIdentity result{};
  if (!data || size < project_offset + project_size) return result;
  result.header = data[0] == image_magic && data[1] > 0 && data[1] <= 16;
  result.chip = little16(data + 12) == esp32s3_chip_id;
  result.descriptor = little32(data + app_desc_offset) == app_desc_magic;
  const auto *project = reinterpret_cast<const char *>(data + project_offset);
  result.project = std::strncmp(project, project_name, project_size) == 0 &&
                   project[sizeof(project_name) - 1] == '\0';
  return result;
}

bool upload_size_allowed(std::size_t size, std::size_t partition_size) {
  return size >= project_offset + project_size && size <= maximum_image_bytes &&
         size <= partition_size;
}

}  // namespace ota_service

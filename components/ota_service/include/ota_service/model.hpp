#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ota_service {

using U64 = std::uint64_t;

inline constexpr std::size_t digest_bytes = 32;
inline constexpr std::size_t digest_hex_bytes = digest_bytes * 2;
inline constexpr std::size_t session_bytes = 16;
inline constexpr std::size_t session_hex_bytes = session_bytes * 2;
inline constexpr std::size_t image_prefix_bytes = 320;
inline constexpr std::size_t maximum_image_bytes = 0x300000;
inline constexpr U64 challenge_lifetime_ms = 120000;
inline constexpr U64 session_lifetime_ms = 300000;
inline constexpr unsigned maximum_auth_failures = 5;

enum class State : std::uint8_t {
  IDLE,
  CHALLENGE,
  AUTHENTICATED,
  UPLOADING,
  VERIFIED,
  FAILED,
};

enum class AuthResult : std::uint8_t {
  ACCEPTED,
  EXPIRED,
  REFUSED,
  LOCKED,
  BUSY,
};

struct Policy {
  State state = State::IDLE;
  U64 expires = 0;
  unsigned failures = 0;

  bool challenge(U64 now);
  AuthResult authenticate(U64 now, bool proof_matches);
  bool begin_upload(U64 now, bool session_matches);
  void finish(bool verified);
  void revoke();
  bool busy(U64 now) const;
};

struct ImageIdentity {
  bool header = false;
  bool chip = false;
  bool descriptor = false;
  bool project = false;
  bool compatible() const { return header && chip && descriptor && project; }
};

bool decode_hex(std::string_view text, unsigned char *out, std::size_t count);
bool constant_time_equal(const unsigned char *a, const unsigned char *b,
                         std::size_t count);
ImageIdentity inspect_image_prefix(const unsigned char *data, std::size_t size);
bool upload_size_allowed(std::size_t size, std::size_t partition_size);

}  // namespace ota_service

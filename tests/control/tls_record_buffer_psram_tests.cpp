#include "../../components/artwork/include/artwork/tls_record_buffer_psram.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
unsigned checks = 0;
void expect(bool value, const char *message) {
  ++checks;
  if (!value) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
}  // namespace

int main() {
  using namespace artwork::tls_record_buffer;
  expect(resolved_input_buffer_bytes == 16717, "pinned input size");
  expect(resolved_output_buffer_bytes == 4429, "pinned output size");
  expect(is_input_record_buffer_request(1, 16717), "exact request selected");
  expect(is_input_record_buffer_request(16717, 1), "equivalent product selected");
  expect(!is_input_record_buffer_request(1, 16384), "content size not selected");
  expect(!is_input_record_buffer_request(1, 16716), "smaller neighbor internal");
  expect(!is_input_record_buffer_request(1, 16718), "larger neighbor internal");
  expect(!is_input_record_buffer_request(0, 16717), "zero request unchanged");
  expect(!is_input_record_buffer_request(std::numeric_limits<std::size_t>::max(), 2),
         "overflow rejected");
  std::size_t bytes = 0;
  expect(request_size(1, 16717, bytes) && bytes == 16717, "exact checked product");
  expect(!request_size(std::numeric_limits<std::size_t>::max(), 2, bytes),
         "overflow product rejected");
  expect(is_scoped_psram_request(524), "observed RSA limb state admitted");
  expect(is_scoped_psram_request(1036), "observed large RSA limb state admitted");
  expect(is_scoped_psram_request(resolved_output_buffer_bytes),
         "pinned output record buffer admitted");
  expect(!is_scoped_psram_request(0), "zero request rejected");
  expect(!is_scoped_psram_request(4097), "unrecognized oversized request rejected");
  expect(!is_scoped_psram_request(resolved_input_buffer_bytes),
         "record buffer handled only by exact record policy");
  std::cout << "PASS " << checks << "/" << checks
            << " TLS input record-buffer allocation checks\n";
}

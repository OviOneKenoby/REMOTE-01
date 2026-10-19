#pragma once
#include "api.hpp"
namespace network_manager {
// Copied secret-free presentation. No SDK calls or retained network pointers.
struct IpText { char text[16]{}; bool assigned = false; };
IpText ip_text(const Status &);
}

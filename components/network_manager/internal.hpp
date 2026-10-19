#pragma once
#include "network_manager/portal.hpp"
namespace network_manager {
bool web_start(); void web_stop(); unsigned web_stack_min(); portal::Stats web_stats();
bool dns_start(); void dns_stop(); void dns_tick(); void dns_stats(portal::Stats &);
bool dns_active();
}

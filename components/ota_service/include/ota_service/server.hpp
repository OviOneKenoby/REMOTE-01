#pragma once

#include "esp_http_server.h"

namespace ota_service {
bool register_handlers(httpd_handle_t);
}

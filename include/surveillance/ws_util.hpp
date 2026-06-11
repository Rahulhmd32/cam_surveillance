#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace surveillance {

std::string base64_encode(const uint8_t* data, size_t len);
std::string websocket_accept_key(const std::string& client_key);

}  // namespace surveillance

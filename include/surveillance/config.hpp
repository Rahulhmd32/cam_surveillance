#pragma once

#include <string>

namespace surveillance {

struct CloudConfig {
    std::string host = "148.113.39.217";
    int port = 4455;
    std::string device_id = "dmp-di1-surveillance-01";
    std::string vehicle_id = "dmp-di1-devkit-01";
    int heartbeat_interval_s = 10;
    int reconnect_min_s = 1;
    int reconnect_max_s = 30;
};

}  // namespace surveillance

#pragma once

#include "surveillance/config.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace surveillance {

struct CloudFrame {
    std::string image_b64;
    uint64_t frame = 0;
    float fps = 0.0f;
    int64_t ts_ms = 0;
};

std::string build_frame_json(const CloudFrame& frame);
std::string build_registration_json(const CloudConfig& config);
std::string build_heartbeat_json(const CloudConfig& config, const std::string& timestamp);

class CloudVmsClient {
public:
    explicit CloudVmsClient(CloudConfig config);
    ~CloudVmsClient();

    bool start();
    void stop();
    void publish(CloudFrame frame);
    bool connected() const { return connected_.load(); }
    bool registered() const { return registered_.load(); }

private:
    void run();
    void heartbeat_loop();
    bool register_device();
    bool send_heartbeat();
    int connect_websocket();
    bool connected_loop(int fd);

    CloudConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> registered_{false};
    std::thread worker_;
    std::thread heartbeat_worker_;
    std::mutex frame_mu_;
    CloudFrame latest_frame_;
    uint64_t frame_generation_ = 0;
};

}  // namespace surveillance

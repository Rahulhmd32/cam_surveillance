#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace surveillance {

struct CameraFrame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint64_t timestamp_us = 0;
    bool valid = false;
};

class Camera {
public:
    Camera() = default;
    ~Camera();

    bool open(const std::string& device, uint32_t width, uint32_t height);
    CameraFrame read();
    void close();
    bool is_open() const { return handle_ != nullptr; }

private:
    void* handle_ = nullptr;
    std::string device_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t last_checksum_ = 0;
    int stale_count_ = 0;
    static constexpr int kStaleFrameLimit = 30;
};

}  // namespace surveillance

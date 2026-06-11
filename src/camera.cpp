#include "surveillance/camera.hpp"

#include <capture.h>
#include <linux/videodev2.h>

#include <algorithm>
#include <cstring>

namespace surveillance {

Camera::~Camera() { close(); }

bool Camera::open(const std::string& device, uint32_t width, uint32_t height) {
    close();

    auto* capture = new V4L2Capture();
    if (capture->init(device.c_str(), V4L2_PIX_FMT_YUYV, width, height) != 0) {
        delete capture;
        return false;
    }
    if (capture->start() != 0) {
        capture->cleanup();
        delete capture;
        return false;
    }

    handle_ = capture;
    device_ = device;
    width_ = width;
    height_ = height;
    last_checksum_ = 0;
    stale_count_ = 0;
    return true;
}

CameraFrame Camera::read() {
    CameraFrame frame;
    if (!handle_) return frame;

    auto* capture = static_cast<V4L2Capture*>(handle_);
    video_frame_t device_frame{};
    if (capture->getFrame(&device_frame) != 0) return frame;

    frame.width = device_frame.width;
    frame.height = device_frame.height;
    frame.format = device_frame.format;
    frame.timestamp_us = device_frame.timestamp_us;
    frame.data.resize(device_frame.size);
    std::memcpy(frame.data.data(), device_frame.vaddr, device_frame.size);
    frame.valid = capture->releaseFrame(&device_frame) == 0;
    if (!frame.valid) return frame;

    uint32_t checksum = 0;
    const size_t check_bytes = std::min(frame.data.size(), static_cast<size_t>(4096));
    for (size_t i = 0; i < check_bytes; ++i) checksum += frame.data[i];

    if (checksum == last_checksum_) {
        if (++stale_count_ >= kStaleFrameLimit) {
            const std::string device = device_;
            const uint32_t width = width_;
            const uint32_t height = height_;
            close();
            open(device, width, height);
        }
    } else {
        last_checksum_ = checksum;
        stale_count_ = 0;
    }

    return frame;
}

void Camera::close() {
    if (handle_) {
        auto* capture = static_cast<V4L2Capture*>(handle_);
        capture->stop();
        capture->cleanup();
        delete capture;
        handle_ = nullptr;
    }
}

}  // namespace surveillance

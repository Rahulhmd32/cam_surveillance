#include <capture.h>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <linux/videodev2.h>

namespace {

std::atomic<bool> running{true};

void stop_handler(int) {
    running.store(false);
}

}  // namespace

int main(int argc, char** argv) {
    const char* device = argc > 1 ? argv[1] : "/dev/video3";
    const uint32_t width = argc > 2 ? static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 640;
    const uint32_t height = argc > 3 ? static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)) : 480;

    std::signal(SIGINT, stop_handler);
    std::signal(SIGTERM, stop_handler);

    V4L2Capture capture;
    if (capture.init(device, V4L2_PIX_FMT_YUYV, width, height) != 0) {
        std::fprintf(stderr, "Failed to initialize camera %s\n", device);
        return 1;
    }
    if (capture.start() != 0) {
        std::fprintf(stderr, "Failed to start camera %s\n", device);
        capture.cleanup();
        return 1;
    }

    uint64_t frame_count = 0;
    while (running.load()) {
        video_frame_t frame{};
        if (capture.getFrame(&frame) != 0) {
            continue;
        }

        // Send frame.vaddr and frame.size to the cloud before releasing it.
        // The memory remains owned by the camera driver.
        ++frame_count;

        capture.releaseFrame(&frame);
    }

    capture.stop();
    capture.cleanup();
    std::printf("Captured %llu frames\n", static_cast<unsigned long long>(frame_count));
    return 0;
}

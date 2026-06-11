#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace surveillance {
class Camera {
public:
    ~Camera();
    bool open(const std::string& device, uint32_t width, uint32_t height);
    bool read(std::vector<uint8_t>* frame);
    void close();
private:
    struct Buffer { void* data=nullptr; size_t size=0; };
    int fd_=-1;
    std::vector<Buffer> buffers_;
};
}

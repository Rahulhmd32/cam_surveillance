#include "surveillance/cloud_vms_client.hpp"

#include "surveillance/ws_util.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>
#include <vector>

namespace surveillance {
namespace {

bool send_all(int fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool send_all(int fd, const std::string& value) {
    return send_all(fd, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

int connect_tcp(const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    std::string port_text = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0) return -1;

    int fd = -1;
    for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        timeval timeout{5, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    return fd;
}

std::string recv_until_close(int fd, size_t max_bytes = 65536) {
    std::string response;
    char buf[2048];
    while (response.size() < max_bytes) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, static_cast<size_t>(n));
    }
    return response;
}

int http_status(const std::string& response) {
    size_t first = response.find(' ');
    if (first == std::string::npos) return 0;
    return std::atoi(response.c_str() + first + 1);
}

std::string http_header(const std::string& response, const std::string& name) {
    std::string lower_response = response;
    std::string token = name + ":";
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(token.begin(), token.end(), token.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    size_t pos = lower_response.find(token);
    if (pos == std::string::npos) return "";
    pos += token.size();
    while (pos < response.size() && response[pos] == ' ') ++pos;
    size_t end = response.find("\r\n", pos);
    return end == std::string::npos ? "" : response.substr(pos, end - pos);
}

bool http_post(
    const std::string& host, int port, const std::string& path,
    const std::string& body, int* status) {
    int fd = connect_tcp(host, port);
    if (fd < 0) return false;
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << host << ":" << port << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Connection: close\r\n"
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;
    bool sent = send_all(fd, req.str());
    std::string response = sent ? recv_until_close(fd) : "";
    close(fd);
    if (status) *status = http_status(response);
    return sent && status && *status >= 200 && *status < 300;
}

std::string utc_timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    char out[32];
    std::strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return out;
}

bool websocket_send_frame(int fd, uint8_t opcode, const std::string& payload) {
    static thread_local std::mt19937 rng(std::random_device{}());
    uint8_t mask[4] = {
        static_cast<uint8_t>(rng()), static_cast<uint8_t>(rng()),
        static_cast<uint8_t>(rng()), static_cast<uint8_t>(rng())};
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 14);
    frame.push_back(static_cast<uint8_t>(0x80 | opcode));
    if (payload.size() < 126) {
        frame.push_back(static_cast<uint8_t>(0x80 | payload.size()));
    } else if (payload.size() < 65536) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<uint8_t>(payload.size() >> 8));
        frame.push_back(static_cast<uint8_t>(payload.size()));
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>(payload.size() >> (i * 8)));
    }
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < payload.size(); ++i)
        frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
    return send_all(fd, frame.data(), frame.size());
}

std::string websocket_key() {
    std::random_device random;
    uint8_t nonce[16];
    for (uint8_t& byte : nonce) byte = static_cast<uint8_t>(random());
    return base64_encode(nonce, sizeof(nonce));
}

bool recv_exact(int fd, uint8_t* data, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, data + got, len - got, 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

bool websocket_recv_frame(int fd, uint8_t* opcode, std::string* payload) {
    uint8_t head[2];
    if (!recv_exact(fd, head, sizeof(head))) return false;
    *opcode = head[0] & 0x0f;
    bool masked = (head[1] & 0x80) != 0;
    uint64_t len = head[1] & 0x7f;
    if (len == 126) {
        uint8_t ext[2];
        if (!recv_exact(fd, ext, sizeof(ext))) return false;
        len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        uint8_t ext[8];
        if (!recv_exact(fd, ext, sizeof(ext))) return false;
        len = 0;
        for (uint8_t byte : ext) len = (len << 8) | byte;
    }
    if (len > 16 * 1024 * 1024) return false;
    uint8_t mask[4]{};
    if (masked && !recv_exact(fd, mask, sizeof(mask))) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(len));
    if (len && !recv_exact(fd, bytes.data(), bytes.size())) return false;
    if (masked) {
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] ^= mask[i % 4];
    }
    payload->assign(bytes.begin(), bytes.end());
    return true;
}

}  // namespace

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::string build_frame_json(const CloudFrame& frame) {
    std::ostringstream out;
    out << "{\"image\":\"" << json_escape(frame.image_b64)
        << "\",\"frame\":"  << frame.frame
        << ",\"fps\":"      << frame.fps
        << ",\"ts\":"       << frame.ts_ms << "}";
    return out.str();
}

std::string build_registration_json(const CloudConfig& config) {
    std::ostringstream out;
    out << "{\"device_id\":\"" << json_escape(config.device_id)
        << "\",\"device_type\":\"surveillance\"";
    if (!config.vehicle_id.empty())
        out << ",\"vehicle_id\":\"" << json_escape(config.vehicle_id) << "\"";
    out << ",\"transport\":\"relay\""
        << ",\"capabilities\":[\"video_stream\"]"
        << ",\"streams\":[{\"id\":\"cam-0\",\"format\":\"jpeg\","
           "\"description\":\"DI1 surveillance camera\"}]"
        << ",\"metadata\":{\"version\":\"1.0\",\"platform\":\"dmp-di1\"}}";
    return out.str();
}

std::string build_heartbeat_json(
    const CloudConfig& config, const std::string& timestamp) {
    return "{\"device_id\":\"" + json_escape(config.device_id) +
        "\",\"timestamp\":\"" + json_escape(timestamp) + "\"}";
}

CloudVmsClient::CloudVmsClient(CloudConfig config) : config_(std::move(config)) {}

CloudVmsClient::~CloudVmsClient() { stop(); }

bool CloudVmsClient::start() {
    if (config_.host.empty() || config_.device_id.empty() || running_.exchange(true)) return false;
    worker_ = std::thread(&CloudVmsClient::run, this);
    heartbeat_worker_ = std::thread(&CloudVmsClient::heartbeat_loop, this);
    return true;
}

void CloudVmsClient::stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
    if (heartbeat_worker_.joinable()) heartbeat_worker_.join();
}

void CloudVmsClient::publish(CloudFrame frame) {
    std::lock_guard<std::mutex> lock(frame_mu_);
    latest_frame_ = std::move(frame);
    ++frame_generation_;
}

bool CloudVmsClient::register_device() {
    int status = 0;
    bool ok = http_post(config_.host, config_.port, "/api/device/register",
                        build_registration_json(config_), &status);
    registered_.store(ok);
    if (!ok) std::fprintf(stderr, "[cloud] registration failed status=%d\n", status);
    else std::printf("[cloud] registered device_id=%s\n", config_.device_id.c_str());
    return ok;
}

bool CloudVmsClient::send_heartbeat() {
    int status = 0;
    bool ok = http_post(config_.host, config_.port, "/api/device/heartbeat",
                        build_heartbeat_json(config_, utc_timestamp()), &status);
    if (!ok) std::fprintf(stderr, "[cloud] heartbeat failed status=%d\n", status);
    return ok;
}

int CloudVmsClient::connect_websocket() {
    int fd = connect_tcp(config_.host, config_.port);
    if (fd < 0) return -1;
    std::string key = websocket_key();
    std::ostringstream req;
    req << "GET /ws/device/" << config_.device_id << " HTTP/1.1\r\n"
        << "Host: " << config_.host << ":" << config_.port << "\r\n"
        << "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        << "Sec-WebSocket-Key: " << key << "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (!send_all(fd, req.str())) { close(fd); return -1; }
    std::string response;
    char ch;
    while (response.size() < 8192 && recv(fd, &ch, 1, 0) == 1) {
        response += ch;
        if (response.size() >= 4 && response.substr(response.size() - 4) == "\r\n\r\n") break;
    }
    if (http_status(response) != 101 ||
        http_header(response, "Sec-WebSocket-Accept") != websocket_accept_key(key)) {
        close(fd);
        return -1;
    }
    connected_.store(true);
    std::printf("[cloud] websocket connected\n");
    return fd;
}

bool CloudVmsClient::connected_loop(int fd) {
    uint64_t sent_generation = 0;
    while (running_.load()) {
        CloudFrame frame;
        uint64_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(frame_mu_);
            generation = frame_generation_;
            if (generation != sent_generation) frame = latest_frame_;
        }
        if (generation != sent_generation) {
            if (!websocket_send_frame(fd, 0x1, build_frame_json(frame))) return false;
            sent_generation = generation;
        }

        pollfd pfd{fd, POLLIN, 0};
        int ready = poll(&pfd, 1, 25);
        if (ready < 0 || (ready > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))) return false;
        if (ready > 0 && (pfd.revents & POLLIN)) {
            uint8_t opcode = 0;
            std::string message;
            if (!websocket_recv_frame(fd, &opcode, &message)) return false;
            if (opcode == 0x8) return false;
            if (opcode == 0x9) {
                if (!websocket_send_frame(fd, 0xA, message)) return false;
            }
        }
    }
    return true;
}

void CloudVmsClient::heartbeat_loop() {
    auto last = std::chrono::steady_clock::now();
    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();
        if (registered_.load() &&
            now - last >= std::chrono::seconds(config_.heartbeat_interval_s)) {
            send_heartbeat();
            last = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CloudVmsClient::run() {
    auto sleep_while_running = [this](int seconds) {
        for (int elapsed_ms = 0; running_.load() && elapsed_ms < seconds * 1000;
             elapsed_ms += 100)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };
    int delay_s = config_.reconnect_min_s;
    while (running_.load()) {
        if (!register_device()) {
            sleep_while_running(delay_s);
            delay_s = std::min(delay_s * 2, config_.reconnect_max_s);
            continue;
        }
        int fd = connect_websocket();
        if (fd < 0) {
            std::fprintf(stderr, "[cloud] websocket connection failed\n");
            sleep_while_running(delay_s);
            delay_s = std::min(delay_s * 2, config_.reconnect_max_s);
            continue;
        }
        delay_s = config_.reconnect_min_s;
        connected_loop(fd);
        shutdown(fd, SHUT_RDWR);
        close(fd);
        connected_.store(false);
        if (running_.load()) {
            std::fprintf(stderr, "[cloud] disconnected; reconnecting\n");
            sleep_while_running(delay_s);
        }
    }
    connected_.store(false);
    registered_.store(false);
}

}  // namespace surveillance

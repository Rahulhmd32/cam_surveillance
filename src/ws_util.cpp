#include "surveillance/ws_util.hpp"

#include <vector>

namespace surveillance {
namespace {

void sha1_digest(const uint8_t* msg, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    auto rol = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    std::vector<uint8_t> buf(msg, msg + len);
    buf.push_back(0x80);
    while ((buf.size() % 64) != 56) buf.push_back(0);
    for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>(bit_len >> (i * 8)));
    for (size_t off = 0; off < buf.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(buf[off+i*4]) << 24) | (uint32_t(buf[off+i*4+1]) << 16) |
                   (uint32_t(buf[off+i*4+2]) << 8) | buf[off+i*4+3];
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f=(b&c)|(~b&d); k=0x5A827999u; }
            else if (i < 40) { f=b^c^d; k=0x6ED9EBA1u; }
            else if (i < 60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDCu; }
            else { f=b^c^d; k=0xCA62C1D6u; }
            uint32_t tmp=rol(a,5)+f+e+k+w[i]; e=d; d=c; c=rol(b,30); b=a; a=tmp;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
    }
    for (int i = 0; i < 5; ++i) {
        out[i*4]=uint8_t(h[i]>>24); out[i*4+1]=uint8_t(h[i]>>16);
        out[i*4+2]=uint8_t(h[i]>>8); out[i*4+3]=uint8_t(h[i]);
    }
}

}  // namespace

std::string base64_encode(const uint8_t* in, size_t len) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = uint32_t(in[i]) << 16;
        if (i + 1 < len) v |= uint32_t(in[i+1]) << 8;
        if (i + 2 < len) v |= in[i+2];
        out += table[(v >> 18) & 63]; out += table[(v >> 12) & 63];
        out += i + 1 < len ? table[(v >> 6) & 63] : '=';
        out += i + 2 < len ? table[v & 63] : '=';
    }
    return out;
}

std::string websocket_accept_key(const std::string& client_key) {
    std::string value = client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[20];
    sha1_digest(reinterpret_cast<const uint8_t*>(value.data()), value.size(), digest);
    return base64_encode(digest, sizeof(digest));
}

}  // namespace surveillance

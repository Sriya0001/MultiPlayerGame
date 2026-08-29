#include "server/MessageFramer.h"

#include <stdexcept>
#include <cstring>

#include <sys/socket.h>
#include <unistd.h>

// ── Wire format helpers ───────────────────────────────────────────────────────
// Store the 4-byte length in little-endian order.
// (Network byte order / big-endian is conventional for protocols, but
//  little-endian avoids a bswap on x86. Since both ends are controlled by us,
//  the choice is arbitrary — little-endian is documented in protocol.md.)

static uint32_t readU32LE(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

static void writeU32LE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8)  & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

// ── Public API ────────────────────────────────────────────────────────────────

namespace MessageFramer {

std::vector<uint8_t> encode(const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> frame;
    frame.resize(HEADER_SIZE + len);
    writeU32LE(frame.data(), len);
    std::memcpy(frame.data() + HEADER_SIZE, payload.data(), len);
    return frame;
}

bool decode(std::vector<uint8_t>& buffer, std::string& out) {
    // Need at least 4 bytes for the header
    if (buffer.size() < HEADER_SIZE) return false;

    uint32_t payloadLen = readU32LE(buffer.data());

    if (payloadLen > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("Message too large: "
                                 + std::to_string(payloadLen) + " bytes");
    }

    std::size_t totalNeeded = HEADER_SIZE + payloadLen;
    if (buffer.size() < totalNeeded) return false; // need more data

    out.assign(reinterpret_cast<const char*>(buffer.data() + HEADER_SIZE),
               payloadLen);

    // Remove consumed bytes from the front of the buffer
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(totalNeeded));
    return true;
}

bool readExact(int fd, void* buf, std::size_t n) {
    auto* ptr      = reinterpret_cast<uint8_t*>(buf);
    std::size_t remaining = n;

    while (remaining > 0) {
        ssize_t r = ::recv(fd, ptr, remaining, 0);
        if (r <= 0) return false; // disconnect or error
        ptr       += r;
        remaining -= static_cast<std::size_t>(r);
    }
    return true;
}

bool writeAll(int fd, const std::vector<uint8_t>& data) {
    const uint8_t* ptr     = data.data();
    std::size_t    remaining = data.size();

    while (remaining > 0) {
        ssize_t w = ::send(fd, ptr, remaining, MSG_NOSIGNAL);
        if (w <= 0) return false;
        ptr       += w;
        remaining -= static_cast<std::size_t>(w);
    }
    return true;
}

} // namespace MessageFramer

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// MessageFramer
//
// Implements the wire protocol framing layer.
//
// Wire format (little-endian):
//   [4 bytes: uint32_t payload length][N bytes: JSON payload]
//
// This approach:
//   - Works correctly on TCP (which is a byte stream, not a message stream)
//   - Allows the receiver to know exactly how many bytes to read
//   - Supports messages up to 4 GiB (practical limit enforced at 1 MiB)
//
// Newline framing (Milestone 1) was dropped because:
//   - JSON values can legitimately contain '\n' (e.g. in string fields)
//   - It requires scanning the entire buffer to find the delimiter
//   - It doesn't compose well with binary data
// ─────────────────────────────────────────────────────────────────────────────

namespace MessageFramer {

constexpr uint32_t MAX_MESSAGE_SIZE = 1u * 1024u * 1024u; // 1 MiB hard limit
constexpr std::size_t HEADER_SIZE   = 4;                   // uint32_t length prefix

// Encode a JSON string into a length-prefixed wire message.
// Returns the bytes to be sent over the socket.
std::vector<uint8_t> encode(const std::string& payload);

// Attempt to decode one complete message from a raw byte buffer.
//
// Returns true and sets 'out' if a complete message was decoded.
// Removes the consumed bytes from 'buffer'.
// Returns false if more data is needed (not an error).
// Throws std::runtime_error if the message is malformed or too large.
bool decode(std::vector<uint8_t>& buffer, std::string& out);

// Read exactly 'n' bytes from 'fd' into 'buf'.
// Returns false on disconnect or error.
bool readExact(int fd, void* buf, std::size_t n);

// Write all bytes in 'data' to 'fd'.
// Returns false on error.
bool writeAll(int fd, const std::vector<uint8_t>& data);

} // namespace MessageFramer

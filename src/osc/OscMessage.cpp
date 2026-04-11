#include "OscMessage.h"
#include <algorithm>

namespace bbfx {

// OSC uses big-endian (network byte order)
static float readFloat32BE(const uint8_t* p) {
    uint32_t v = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

static int32_t readInt32BE(const uint8_t* p) {
    return static_cast<int32_t>((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static void writeFloat32BE(std::vector<uint8_t>& buf, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static void writeInt32BE(std::vector<uint8_t>& buf, int32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static size_t padTo4(size_t n) { return (n + 3) & ~3; }

static std::string readString(const uint8_t* data, size_t len, size_t& offset) {
    size_t start = offset;
    while (offset < len && data[offset] != 0) ++offset;
    std::string s(reinterpret_cast<const char*>(data + start), offset - start);
    ++offset; // skip null
    offset = padTo4(offset); // pad to 4-byte boundary
    return s;
}

static void writeString(std::vector<uint8_t>& buf, const std::string& s) {
    for (char c : s) buf.push_back(static_cast<uint8_t>(c));
    buf.push_back(0); // null terminator
    while (buf.size() % 4 != 0) buf.push_back(0); // pad
}

bool OscMessage::parse(const uint8_t* data, size_t len, OscMessage& out) {
    if (len < 4) return false;

    // Check for bundle (starts with "#bundle\0")
    if (data[0] == '#') return false; // bundles not supported in simple parse

    size_t offset = 0;

    // Read address
    out.address = readString(data, len, offset);
    if (out.address.empty() || out.address[0] != '/') return false;

    // Read type tag string (starts with ',')
    if (offset >= len) return true; // no args, that's OK (bang)
    std::string typeTags = readString(data, len, offset);
    if (typeTags.empty() || typeTags[0] != ',') return true; // no type tags = bang

    // Parse arguments based on type tags
    for (size_t i = 1; i < typeTags.size(); ++i) {
        if (offset + 4 > len) break;
        switch (typeTags[i]) {
            case 'f':
                out.args.push_back(readFloat32BE(data + offset));
                offset += 4;
                break;
            case 'i':
                out.args.push_back(readInt32BE(data + offset));
                offset += 4;
                break;
            case 's':
                out.args.push_back(readString(data, len, offset));
                break;
            default:
                break; // skip unknown types
        }
    }

    return true;
}

std::vector<uint8_t> OscMessage::serialize(const OscMessage& msg) {
    std::vector<uint8_t> buf;

    // Write address
    writeString(buf, msg.address);

    // Build type tag string
    std::string typeTags = ",";
    for (auto& arg : msg.args) {
        if (std::holds_alternative<float>(arg)) typeTags += 'f';
        else if (std::holds_alternative<int32_t>(arg)) typeTags += 'i';
        else if (std::holds_alternative<std::string>(arg)) typeTags += 's';
    }
    writeString(buf, typeTags);

    // Write arguments
    for (auto& arg : msg.args) {
        if (std::holds_alternative<float>(arg)) {
            writeFloat32BE(buf, std::get<float>(arg));
        } else if (std::holds_alternative<int32_t>(arg)) {
            writeInt32BE(buf, std::get<int32_t>(arg));
        } else if (std::holds_alternative<std::string>(arg)) {
            writeString(buf, std::get<std::string>(arg));
        }
    }

    return buf;
}

} // namespace bbfx

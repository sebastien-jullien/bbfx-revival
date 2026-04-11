#pragma once
#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <cstring>

namespace bbfx {

/// A single argument in an OSC message.
using OscArg = std::variant<float, int32_t, std::string>;

/// A parsed OSC message.
struct OscMessage {
    std::string address;       // e.g., "/bbfx/fader/1"
    std::vector<OscArg> args;  // typed arguments

    float getFloat(size_t i) const {
        if (i < args.size() && std::holds_alternative<float>(args[i]))
            return std::get<float>(args[i]);
        return 0.0f;
    }

    int32_t getInt(size_t i) const {
        if (i < args.size() && std::holds_alternative<int32_t>(args[i]))
            return std::get<int32_t>(args[i]);
        return 0;
    }

    std::string getString(size_t i) const {
        if (i < args.size() && std::holds_alternative<std::string>(args[i]))
            return std::get<std::string>(args[i]);
        return "";
    }

    /// Parse a raw OSC packet into an OscMessage. Returns false on error.
    static bool parse(const uint8_t* data, size_t len, OscMessage& out);

    /// Serialize an OscMessage into a raw OSC packet.
    static std::vector<uint8_t> serialize(const OscMessage& msg);
};

} // namespace bbfx

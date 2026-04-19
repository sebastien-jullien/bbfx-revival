#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot K — reusable gamepad -> DAG port mapping preset.
///
/// Stored as JSON with extension `.bbfx-gamepad-mapping`. A mapping
/// describes which gamepad output (axis/button/sensor) drives which port
/// of the current project's graph, with an optional scale + offset + invert
/// transformation. When loaded, MappingProfile builds Animator links and
/// optionally spawns the required nodes (MapperNode) if they're missing.
struct GamepadMappingEntry {
    std::string source;    // e.g. "leftStickX", "gyroX", "buttonA"
    std::string target;    // "<nodeName>.<portName>"
    float       scale  = 1.0f;
    float       offset = 0.0f;
    bool        invert = false;
};

struct GamepadMappingProfile {
    std::string             name;        // display name
    std::string             deviceType;  // "PS5", "Xbox", "SwitchPro", "Any"
    std::string             description;
    std::vector<GamepadMappingEntry> mappings;

    // File I/O — raw JSON, no network round-trip.
    bool loadFromFile(const std::filesystem::path& path, std::string& errorMessage);
    bool saveToFile  (const std::filesystem::path& path, std::string& errorMessage) const;

    bool loadFromJsonString(const std::string& jsonText, std::string& errorMessage);
    std::string toJsonString() const;
};

} // namespace bbfx

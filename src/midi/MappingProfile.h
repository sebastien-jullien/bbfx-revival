#pragma once
#include "MidiLearnManager.h"
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace bbfx {

/// Binding for an OSC address to a DAG port.
struct OscBinding {
    std::string address;       // OSC address pattern (e.g., "/fader/1")
    std::string targetNode;    // DAG node name
    std::string targetPort;    // DAG port name
    float min = 0.0f;
    float max = 1.0f;
};

/// A complete mapping profile: MIDI bindings + OSC bindings.
/// Can be saved/loaded as standalone .bbfx-mapping files.
class MappingProfile {
public:
    std::string name;
    std::vector<MidiBinding> midiBindings;
    std::vector<OscBinding> oscBindings;

    /// Save profile to a .bbfx-mapping JSON file.
    bool save(const std::string& path) const;

    /// Load profile from a .bbfx-mapping JSON file.
    bool load(const std::string& path);

    /// Apply this profile: push MIDI bindings to MidiLearnManager.
    void apply();

    /// Capture current state from MidiLearnManager into this profile.
    void captureFromManager();

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace bbfx

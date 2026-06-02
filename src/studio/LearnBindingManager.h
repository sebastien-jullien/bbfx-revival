#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace bbfx {

/// Centralised store + applicator for "Learn" bindings (MIDI / Gamepad /
/// Keyboard) targeting any DAG port. Designed to live above the per-input
/// learn helpers (MidiLearnManager, GamepadPanel learn, EffectRack key learn)
/// and unify their persistence.
///
/// The full ImGui LearnPanel UI uses this manager as its model; for
/// headless test purposes the manager exposes a stand-alone API.
class LearnBindingManager {
public:
    enum class SourceType {
        MidiCC = 0,
        MidiNote = 1,
        GamepadButton = 2,
        GamepadAxis = 3,
        KeyboardKey = 4
    };

    struct Binding {
        std::string portPath;     // e.g. "fullscreen.alpha"
        SourceType  sourceType = SourceType::MidiCC;
        int         sourceId   = 0;
        float       scale      = 1.0f;
        float       offset     = 0.0f;
        bool        invert     = false;
    };

    static LearnBindingManager& instance();

    // CRUD
    void addBinding(const Binding& b);
    bool removeBindingAt(size_t index);
    void clearAll();
    size_t bindingCount() const { return mBindings.size(); }
    const Binding& getBinding(size_t i) const { return mBindings.at(i); }
    Binding& getBindingMut(size_t i) { return mBindings.at(i); }
    const std::vector<Binding>& getAllBindings() const { return mBindings; }

    // Apply a raw input value to the matching binding, computing the final
    // remapped value (scale + offset + invert) ready to be pushed onto a port.
    static float applyTransform(const Binding& b, float rawValue);

    // Find the first binding for a given (sourceType, sourceId). Returns -1
    // if not found.
    int findBindingIndex(SourceType type, int sourceId) const;

    // Persistence (extraJson["learn_bindings"])
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::vector<Binding> mBindings;
};

} // namespace bbfx

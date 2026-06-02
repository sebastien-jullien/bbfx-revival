#pragma once
#include "MidiMessage.h"
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <nlohmann/json_fwd.hpp>

namespace bbfx {

/// Target for MIDI Learn binding.
struct MidiLearnTarget {
    std::string type;     // "fader", "trigger", "port"
    int index = -1;       // fader/trigger index
    std::string nodeName; // for port bindings
    std::string portName;
};

/// A single MIDI binding (CC/note → target).
struct MidiBinding {
    int deviceId = -1;
    int channel = 0;       // 0 = any
    std::string midiType;  // "cc" or "note"
    int number = -1;       // CC# or note#
    MidiLearnTarget target;
    float min = 0.0f, max = 1.0f;
};

/// Manages MIDI Learn mode and stores all bindings.
class MidiLearnManager {
public:
    static MidiLearnManager& instance();

    // Learn mode
    void startLearn(const MidiLearnTarget& target);
    void cancelLearn();
    bool isLearning() const { return mLearning; }
    const MidiLearnTarget& getLearnTarget() const { return mLearnTarget; }

    /// Call each frame with latest MIDI messages. If learning, captures first message.
    void processMessages(const std::vector<MidiMessage>& messages);

    // Binding store
    void addBinding(const MidiBinding& binding);
    void removeBinding(int index);
    void clearBindings();   // C5 — teardown propre (annule learn + vide), au lieu de muter getBindings()
    const std::vector<MidiBinding>& getBindings() const { return mBindings; }
    std::vector<MidiBinding>& getBindings() { return mBindings; }

    // Lookup
    const MidiBinding* findBindingForCC(int cc, int channel) const;
    const MidiBinding* findBindingForNote(int note, int channel) const;

    // Serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    MidiLearnManager() = default;
    bool mLearning = false;
    MidiLearnTarget mLearnTarget;
    std::vector<MidiBinding> mBindings;
};

} // namespace bbfx

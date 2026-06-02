#pragma once

#include "../LearnBindingManager.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace bbfx {

class GamepadPanel;
struct MidiMessage;

/// LearnPanel — unified MIDI / Gamepad / Keyboard "Learn" panel.
/// Lives above the lower-level helpers (MidiLearnManager, GamepadPanel
/// learn callback, EffectRack key learn) and uses LearnBindingManager
/// as its persistent model.
///
/// UI: a searchable, filterable table of every DAG port. Each row exposes
///   - the current binding (or "—" if none)
///   - a `Learn` button (single-shot capture)
///   - inline scale / offset / invert editing
///   - a `Clear` button
/// A toolbar offers `Auto-map all` (sequential capture across visible ports)
/// and `Clear All` (with confirm modal).
class LearnPanel {
public:
    LearnPanel() = default;

    void render();

    /// SDL key event → if a key-learn capture is pending, consume the event
    /// and create the binding. Called from StudioApp before global shortcuts.
    bool handleKeyEvent(const SDL_Event& evt);

    /// Latest MIDI messages — capture if learning a MIDI source.
    void processMidiMessages(const std::vector<MidiMessage>& messages);

    /// Apply every binding to its target port — call once per frame from
    /// the main loop, regardless of panel visibility.
    void update();

    /// Gamepad panel pointer for learn delegation (button/axis capture).
    void setGamepadPanel(GamepadPanel* gp) { mGamepadPanel = gp; }

    /// Persistence via LearnBindingManager (proxied here so StudioApp can
    /// integrate the panel into project save/load directly).
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    bool isVisible() const { return mVisible; }
    void setVisible(bool v) { mVisible = v; }
    void toggle() { mVisible = !mVisible; }

private:
    bool mVisible = false;
    GamepadPanel* mGamepadPanel = nullptr;

    // Capture state
    std::string mLearningPortPath;            ///< empty == not learning
    bool mAutoMapAll = false;                 ///< sequential capture across filtered ports
    size_t mAutoMapIndex = 0;
    std::vector<std::string> mAutoMapList;    ///< snapshot of port paths to walk
    bool mPendingClearAll = false;            ///< triggers confirm modal

    // Filters
    char  mSearch[128] = {0};
    std::string mFilterNodeName;              ///< empty = all nodes
    bool  mFilterFloat   = true;
    bool  mFilterInt     = true;
    bool  mFilterBool    = true;
    bool  mFilterTrigger = true;

    // ── UI helpers ───────────────────────────────────────────────────────
    struct PortRef {
        std::string nodePath;     // "nodeName.portName"
        std::string nodeName;
        std::string portName;
        std::string portType;     // "Float" / "Int" / "Bool" / "Trigger"
    };
    std::vector<PortRef> collectPorts() const;
    bool passesFilters(const PortRef& p) const;
    static const char* sourceTypeLabel(LearnBindingManager::SourceType t);
    static std::string bindingLabel(const LearnBindingManager::Binding& b);

    // ── Capture helpers ──────────────────────────────────────────────────
    void startLearn(const std::string& portPath);
    void cancelLearn();
    void completeLearn(LearnBindingManager::SourceType type, int sourceId);
    void advanceAutoMap();
};

} // namespace bbfx

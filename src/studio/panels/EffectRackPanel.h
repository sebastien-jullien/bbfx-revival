#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>
#include <string>
#include <map>
#include <nlohmann/json_fwd.hpp>

namespace bbfx {

class GamepadPanel;

/// Standalone Effect Rack panel — lists all DAG nodes with enable/disable
/// toggles (LED feedback), MIDI learn, Gamepad learn, and Keyboard learn.
/// Extracted from PresetBrowserPanel (v3.5.1 I-1700).
class EffectRackPanel {
public:
    EffectRackPanel() = default;

    void render();

    /// Process an SDL key event for keyboard learn capture.
    /// Returns true if the event was consumed (learn was active and captured).
    bool handleKeyEvent(const SDL_Event& evt);

    /// Apply keyboard bindings each frame (call from StudioApp main loop).
    void processKeyboardBindings();

    /// Apply MIDI + Gamepad bindings each frame (call unconditionally from
    /// StudioApp, even when the panel is not visible).
    void updateBindings();

    /// Gamepad panel pointer for learn delegation.
    void setGamepadPanel(GamepadPanel* gp) { mGamepadPanel = gp; }

    /// JSON serialisation for project save/load.
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    // ── Gamepad learn ────────────────────────────────────────────────────
    GamepadPanel* mGamepadPanel = nullptr;
    std::string mGamepadLearnNodeName;  ///< Node awaiting gamepad binding (empty = not learning)
    std::map<std::string, std::string> mGamepadBindings;  ///< nodeName → sourceToken
    std::map<std::string, bool> mGamepadPrevButtonState;  ///< For edge detection

    // ── Keyboard learn ───────────────────────────────────────────────────
    std::string mKeyLearnNodeName;  ///< Node awaiting key binding (empty = not learning)
    std::map<std::string, SDL_Keycode> mKeyBindings;  ///< nodeName → key
    std::map<std::string, bool> mKeyPrevState;  ///< For edge detection

    // ── Helpers ──────────────────────────────────────────────────────────
    void applyMidiBindings();
    void applyGamepadBindings();
    static const char* sdlKeyName(SDL_Keycode key);
    static bool isConflictingKey(SDL_Keycode key);
};

} // namespace bbfx

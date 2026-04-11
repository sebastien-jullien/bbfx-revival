#pragma once
#include <imgui.h>

namespace bbfx {

/// Panel showing all MIDI bindings from MidiLearnManager (single source of truth).
/// Supports display, Learn, edit, delete.
class MidiMappingPanel {
public:
    void render();
};

} // namespace bbfx

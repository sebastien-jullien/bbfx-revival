#pragma once
#include <string>
#include <sol/forward.hpp>

namespace bbfx {

/// Shows the properties of the node selected in the NodeEditorPanel.
/// Provides sliders, dropdowns, inline Lua editor, and rename/delete actions.
class InspectorPanel {
public:
    explicit InspectorPanel(sol::state& lua);

    void render();
    void setSelectedNode(const std::string& name) { mSelectedNode = name; }

private:
    void renderFloatPorts();
    void renderEnumPorts();
    void renderLuaEditor();
    void renderShaderUniforms();
    void renderRenameDelete();

    sol::state& mLua;
    std::string mSelectedNode;
    char mLuaSourceBuf[8192] = {};
    bool mLuaModified = false;
    std::string mLuaError;

    struct CoalescingState {
        bool active = false;
        std::string nodeName, portName;
        float oldValue = 0.0f;
    };
    CoalescingState mCoalescing;
};

} // namespace bbfx

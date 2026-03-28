#pragma once
#include <string>

namespace bbfx {

/// Shows the properties of the node selected in the NodeEditorPanel.
/// Provides sliders, dropdowns, inline Lua editor, and rename/delete actions.
class InspectorPanel {
public:
    InspectorPanel() = default;

    void render();
    void setSelectedNode(const std::string& name) { mSelectedNode = name; }

private:
    void renderFloatPorts();
    void renderEnumPorts();
    void renderLuaEditor();
    void renderShaderUniforms();
    void renderRenameDelete();

    std::string mSelectedNode;
    char mLuaSourceBuf[8192] = {};
    bool mLuaModified = false;
    std::string mLuaError;
};

} // namespace bbfx

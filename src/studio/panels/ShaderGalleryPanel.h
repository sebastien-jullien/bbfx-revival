#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace bbfx {

class ShaderPreviewRenderer;

/// Panel displaying an animated grid of BBFx procedural shader previews.
/// Double-click applies shader to selected SceneObjectNode.
class ShaderGalleryPanel {
public:
    ShaderGalleryPanel();

    void render();
    void setPreviewRenderer(ShaderPreviewRenderer* renderer) { mRenderer = renderer; }

    /// Callback: (shaderVertPath, shaderFragPath, targetNodeName)
    using ApplyShaderCb = std::function<void(const std::string&, const std::string&, const std::string&)>;
    void setApplyCallback(ApplyShaderCb cb) { mApplyCb = std::move(cb); }

    void setSelectedSceneObject(const std::string& name) { mSelectedSceneObj = name; }

private:
    ShaderPreviewRenderer* mRenderer = nullptr;
    ApplyShaderCb mApplyCb;
    std::string mSelectedSceneObj;
    int mSelectedShader = -1;

    struct ShaderInfo {
        std::string name;      // display name
        std::string fragFile;  // fragment shader filename
        std::string vertFile;  // vertex shader filename (passthrough if empty)
    };
    std::vector<ShaderInfo> mShaders;
};

} // namespace bbfx

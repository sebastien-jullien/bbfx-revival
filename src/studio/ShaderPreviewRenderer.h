#pragma once
#include <imgui.h>
#include <OgrePrerequisites.h>
#include <string>
#include <map>

namespace Ogre { class RenderTexture; }

namespace bbfx {

/// Renders animated shader/material previews to off-screen RTTs for ImGui display.
/// Uses a dedicated Ogre::SceneManager to avoid polluting the main scene.
class ShaderPreviewRenderer {
public:
    ShaderPreviewRenderer();
    ~ShaderPreviewRenderer();

    /// Initialize with OGRE root. Creates dedicated SceneManager + camera + light.
    void initialize(Ogre::Root* root);

    /// Get preview ImTextureID for a BBFx shader (creates RTT on first access).
    ImTextureID getShaderPreview(const std::string& shaderName);

    /// Get preview ImTextureID for an OGRE material (sphere mesh).
    ImTextureID getMaterialPreview(const std::string& materialName);

    /// Update all visible previews (call each frame; internally rate-limited to 15fps).
    void update(float deltaTime);

    /// Mark a preview as visible/invisible (for culling).
    void setVisible(const std::string& name, bool visible);

    /// Clear all cached previews.
    void invalidate();

    bool isInitialized() const { return mSceneMgr != nullptr; }

private:
    struct PreviewEntry {
        Ogre::RenderTexture* renderTarget = nullptr;
        ImTextureID imguiId = 0;
        bool visible = true;
        std::string materialName;  // material applied to the preview mesh
    };

    void createPreviewRTT(const std::string& name, const std::string& materialName, bool isSphere);

    Ogre::Root* mRoot = nullptr;
    Ogre::SceneManager* mSceneMgr = nullptr;
    Ogre::Camera* mCamera = nullptr;
    Ogre::Light* mLight = nullptr;

    std::map<std::string, PreviewEntry> mPreviews;
    float mTime = 0.0f;
    float mUpdateTimer = 0.0f;
    static constexpr float kUpdateInterval = 1.0f / 15.0f; // 15fps
    static constexpr int kPreviewSize = 64;

    ImTextureID mPlaceholder = 0;
    void createPlaceholder();
};

} // namespace bbfx

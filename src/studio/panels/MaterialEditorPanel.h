#pragma once
#include <imgui.h>
#include <string>
#include <functional>

namespace bbfx {

class ShaderPreviewRenderer;
class TextureThumbnailCache;

/// Panel for visually editing OGRE material properties: colors, shininess, texture slots.
/// Includes a real-time sphere preview via ShaderPreviewRenderer.
class MaterialEditorPanel {
public:
    MaterialEditorPanel();

    void render();
    void setMaterial(const std::string& materialName);
    void setPreviewRenderer(ShaderPreviewRenderer* renderer) { mRenderer = renderer; }
    void setThumbCache(TextureThumbnailCache* cache) { mThumbCache = cache; }

    const std::string& getMaterialName() const { return mMaterialName; }

private:
    std::string mMaterialName;
    ShaderPreviewRenderer* mRenderer = nullptr;
    TextureThumbnailCache* mThumbCache = nullptr;

    // Cached material properties for display
    float mDiffuse[3] = {1, 1, 1};
    float mSpecular[3] = {0, 0, 0};
    float mAmbient[3] = {0.2f, 0.2f, 0.2f};
    float mEmissive[3] = {0, 0, 0};
    float mShininess = 0.0f;
    float mAlpha = 1.0f;
    bool mDirty = false;

    void loadFromMaterial();
    void applyToMaterial();
};

} // namespace bbfx

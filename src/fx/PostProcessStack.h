#pragma once

#include "PostProcessEffect.h"
#include <OgreTexture.h>
#include <OgreRenderTexture.h>
#include <OgreRectangle2D.h>
#include <OgreSceneManager.h>
#include <vector>
#include <string>
#include <memory>

namespace bbfx {

/// Fullscreen post-process pipeline that replaces OGRE's broken CompositorManager
/// for off-screen RenderTextures (Studio mode).
///
/// Uses ping-pong RTTs and OGRE's Rectangle2D with existing BBFx/* materials
/// to chain effects without corrupting FBO state shared with ImGui.
class PostProcessStack {
public:
    explicit PostProcessStack(Ogre::SceneManager* sm);
    ~PostProcessStack();

    /// Create/recreate the ping-pong RTTs at the given resolution.
    void init(uint32_t width, uint32_t height);

    /// Resize RTTs if dimensions changed.
    void resize(uint32_t width, uint32_t height);

    // ── Effect management ────────────────────────────────────────────────────

    /// Add an effect by display name and OGRE material name.
    /// Returns true on success (material must exist).
    bool addEffect(const std::string& name, const std::string& materialName);

    /// Remove an effect by name.
    bool removeEffect(const std::string& name);

    /// Reorder an effect to a new position.
    bool reorder(const std::string& name, int newPosition);

    /// Enable/disable an effect by name.
    bool setEnabled(const std::string& name, bool enabled);

    /// Set a uniform parameter on an effect.
    bool setParam(const std::string& effectName, const std::string& param, float value);

    /// Get the ordered list of effects (sorted by order).
    std::vector<PostProcessEffect> getEffects() const;

    /// Find an effect by name (nullptr if not found).
    PostProcessEffect* findEffect(const std::string& name);

    /// Remove all effects.
    void clear();

    // ── Rendering pipeline ───────────────────────────────────────────────────

    /// Apply the effect chain to the source RenderTexture.
    /// After calling, getOutput() returns the processed result.
    /// If no effects are active, getOutput() returns the source directly (no copy).
    void apply(Ogre::RenderTexture* sourceRT);

    /// Get the output texture (result of apply, or source if no effects active).
    Ogre::TexturePtr getOutput() const { return mOutputTex; }

    /// Get the GL texture ID of the output for ImGui::Image().
    unsigned int getOutputGLID() const;

    /// Returns true if any effects are active.
    bool hasActiveEffects() const;

    /// Set cumulative time (used for time-dependent shaders).
    void setTime(float t) { mTime = t; }

private:
    Ogre::SceneManager* mSceneMgr = nullptr;

    // Ping-pong render textures
    Ogre::TexturePtr mPingTex;
    Ogre::TexturePtr mPongTex;
    Ogre::RenderTexture* mPingRT = nullptr;
    Ogre::RenderTexture* mPongRT = nullptr;

    // Feedback RTT — stores previous frame's output for temporal effects
    Ogre::TexturePtr mPrevFrameTex;
    Ogre::RenderTexture* mPrevFrameRT = nullptr;

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    // Output reference (points to either ping, pong, or source)
    Ogre::TexturePtr mOutputTex;

    // Fullscreen quad for rendering effects
    std::unique_ptr<Ogre::Rectangle2D> mQuad;
    Ogre::SceneNode* mQuadNode = nullptr;

    // Effect chain
    std::vector<PostProcessEffect> mEffects;

    float mTime = 0.0f;

    void renderQuad(Ogre::RenderTexture* target, Ogre::TexturePtr source,
                    const std::string& materialName,
                    const std::map<std::string, float>& params,
                    bool needsPrevFrame = false);

    void destroyRTTs();
};

} // namespace bbfx

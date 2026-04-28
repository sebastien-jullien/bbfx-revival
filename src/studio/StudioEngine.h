#pragma once

#include "../core/Engine.h"
#include "OutputManager.h"
#include "../fx/PostProcessStack.h"
#include <OgreTexture.h>
#include <OgreRenderTexture.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <memory>

namespace bbfx {

/// RAII guard that saves and restores GL state (FBO, viewport) around OGRE renders.
/// Prevents ImGui/OGRE GL state conflicts.
class GLStateGuard {
public:
    GLStateGuard();
    ~GLStateGuard();
private:
    int mSavedFBO = 0;
    int mSavedViewport[4] = {};
};

/// Engine subclass for bbfx-studio.
///
/// Differences from headless Engine:
///  - SDL3 window created with SDL_WINDOW_OPENGL so SDL3 owns the GL context.
///  - OGRE initialized with currentGLContext=true (uses SDL3's GL context).
///  - OGRE renders to an off-screen RenderTexture rather than the main window.
///  - startRendering() runs the combined ImGui + OGRE loop.
///  - SDL_GL_SwapWindow called manually at the end of each frame.
class StudioEngine : public Engine {
public:
    explicit StudioEngine(sol::state& lua);
    ~StudioEngine() override;

    // Override: runs the ImGui + OGRE combined render loop.
    void startRendering() override;

    // ── RenderTexture API (used by ViewportPanel) ─────────────────────────────
    void initRenderTexture(uint32_t width, uint32_t height);
    void resizeRenderTexture(uint32_t width, uint32_t height);
    /// Triggers the OGRE render into the RenderTexture. Call before ImGui frame.
    void updateRenderTarget();
    /// Returns the GL texture ID cast to ImTextureID for ImGui::Image().
    ImTextureID getRenderTextureID() const;
    /// Capture the current RenderTexture contents to a PNG file.
    bool captureFrame(const std::string& path);
    /// Returns the OGRE RenderTexture (for debug/FBO access).
    Ogre::RenderTexture* getRenderTarget() const { return mRenderTarget; }

    SDL_GLContext getGLContext() const { return mGLContext; }

    /// Must be called when compositor chain changes invalidate the FBO state.
    /// Forces full RT rebuild on next resize (even if same dimensions).
    void invalidateFBOCache() { mCachedFBO = -1; mForceRTRebuild = true; }

private:
    SDL_GLContext mGLContext = nullptr;

    Ogre::TexturePtr mRenderTex;
    Ogre::RenderTexture* mRenderTarget = nullptr;

    uint32_t mRTWidth = 1280;
    uint32_t mRTHeight = 720;
    int mCachedFBO = -1;        // cached GL FBO ID for the RenderTexture
    bool mForceRTRebuild = false; // force initRenderTexture even if same size

    // ── Output Manager (v3.4 — replaces dual output v3.3) ──────────────────
public:
    /// Backward-compatible v3.3 API — delegates to OutputManager slot 0.
    bool openOutputWindow(int width = 1920, int height = 1080);
    void closeOutputWindow();
    bool isOutputOpen() const;
    void toggleOutputFullscreen();
    void setOutputResolution(int w, int h);
    void setOutputMonitor(int monitorIndex);
    void updateOutputTarget();
    ImTextureID getOutputTextureID() const;

    /// Access the OutputManager for multi-output (v3.4).
    OutputManager* getOutputManager() { return mOutputManager.get(); }

    /// Access the PostProcessStack (v3.5.1).
    PostProcessStack* getPostProcessStack() { return mPostProcessStack.get(); }

private:
    std::unique_ptr<OutputManager> mOutputManager;
    std::unique_ptr<PostProcessStack> mPostProcessStack;
};

} // namespace bbfx

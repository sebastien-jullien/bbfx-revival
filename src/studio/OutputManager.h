#pragma once

#include "WarpProfile.h"
#include "BlendProfile.h"
#include "GridWarpProfile.h"
#include <OgreTexture.h>
#include <OgreRenderTexture.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace bbfx { class TextureShareSender; }

namespace bbfx { class SurfaceMap; }

namespace bbfx {

/// Live state for the WarpWizard test pattern overlay (v3.4 Lot D).
/// Set by WarpWizard, consumed by OutputManager::renderTestPattern().
struct TestPatternState {
    bool  active       = false;
    float clickedPoints[8] = {}; ///< Up to 4 clicked points (x,y pairs, normalised 0-1)
    int   numClicked   = 0;      ///< How many clicks recorded so far (0-4)
};

/// Represents a single output display (projector window).
struct OutputSlot {
    int id = -1;
    SDL_Window* window = nullptr;       ///< Non-Windows platforms (or nullptr on Win32)
#ifdef _WIN32
    void* nativeHWND = nullptr;         ///< HWND — native Win32 output window
    void* nativeDC   = nullptr;         ///< HDC  — device context for wglMakeCurrent
#endif
    Ogre::TexturePtr texture;
    Ogre::RenderTexture* renderTarget = nullptr;
    uint32_t width = 1920;
    uint32_t height = 1080;
    int monitorIndex = -1;
    bool fullscreen = false;
    int cachedFBO = -1;

    // ── Warp (v3.4) ──────────────────────────────────────────────────
    WarpProfile warpProfile;
    bool warpEnabled = false;

    // ── Blend (v3.4) ─────────────────────────────────────────────────
    BlendProfile blendProfile;
    bool blendEnabled = false;

    // ── WarpWizard test pattern (v3.4 Lot D) ─────────────────────────
    TestPatternState testPattern;

    // ── Surface zone linkage (v3.4 Lot E) ────────────────────────────
    int zoneId = -1; ///< Assigned zone id (-1 = unassigned, use main camera)

    // ── Grid Warp (v3.4 Lot K) ───────────────────────────────────────
    GridWarpProfile gridWarpProfile;
    bool            gridWarpEnabled = false;

    // ── Visibility (v3.5.1 — MasterView toggle) ──────────────────────
    bool visible = true; ///< Whether the output window is shown (default: visible)

    // ── Texture sharing output (v3.4 Lot H, cross-platform Lot N) ──────
    bool                              textureShareEnabled = false;
    std::string                       textureShareSourceName; ///< defaults to "BBFx Output <id>"
    std::unique_ptr<TextureShareSender> textureSender;         ///< lazily created via factory

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/// Manages N independent output windows, each with its own SDL_Window and OGRE RenderTexture.
/// Replaces the single-output code in StudioEngine (v3.3 → v3.4 refactor).
class OutputManager {
public:
    explicit OutputManager(SDL_GLContext sharedContext, SDL_Window* mainWindow);
    ~OutputManager();

    /// Create a new output window at the specified resolution. Returns the slot id.
    int addOutput(int width, int height, Ogre::SceneManager* sceneMgr);
    /// Destroy an output window by id.
    void removeOutput(int id);

    /// Get a slot by id (nullptr if not found).
    OutputSlot* getSlot(int id);
    const OutputSlot* getSlot(int id) const;
    /// Get all slots.
    const std::vector<OutputSlot>& getAllSlots() const { return mSlots; }
    /// Number of active outputs.
    int count() const { return static_cast<int>(mSlots.size()); }

    /// Render all output windows (MakeCurrent, update RT, SwapWindow for each).
    void updateAll();

    /// Set the monitor for a given output slot.
    void setMonitor(int id, int monitorIndex);
    /// Change the resolution of an output slot.
    void setResolution(int id, int w, int h, Ogre::SceneManager* sceneMgr);
    /// Toggle fullscreen for a given output slot.
    void toggleFullscreen(int id);

    /// Enable QuadWarp compositor on a slot.
    void enableWarp(int id);
    /// Disable QuadWarp compositor on a slot.
    void disableWarp(int id);
    /// Push current WarpProfile values to the QuadWarp material uniforms.
    void updateWarpParams(int id);
    /// Reset all warps to identity (PANIC).
    void resetAllWarps();

    /// Enable EdgeBlend compositor on a slot (inserted before QuadWarp).
    void enableBlend(int id);
    /// Disable EdgeBlend compositor on a slot.
    void disableBlend(int id);
    /// Push current BlendProfile values to the EdgeBlend material uniforms.
    void updateBlendParams(int id);
    /// Reset all blend profiles to default (PANIC).
    void resetAllBlends();

    /// Auto-blend suggestion result.
    struct BlendSuggestion {
        int slotA, slotB;
        bool horizontal; // true = left/right adjacency, false = top/bottom
        float overlapFraction = 0.15f; // suggested blend width as fraction
    };
    /// Detect pairs of outputs on adjacent monitors and suggest blend widths.
    std::vector<BlendSuggestion> detectAdjacentOutputs() const;

    /// Get the GL texture ID for ImGui preview of a slot.
    ImTextureID getTextureID(int id) const;

    /// Attach a SurfaceMap for zone camera lookup during rendering.
    void setSurfaceMap(SurfaceMap* sm) { mSurfaceMap = sm; }
    SurfaceMap* getSurfaceMap() const  { return mSurfaceMap; }

    /// Set the source OGRE texture whose GL ID is blitted to all output windows.
    /// Typically the StudioEngine's main RenderTexture.
    void setSourceTexture(const Ogre::TexturePtr& tex) { mSourceTexture = tex; }

    /// Enable GridWarp compositor on a slot (v3.4 Lot K).
    void enableGridWarp(int id);
    /// Disable GridWarp compositor on a slot.
    void disableGridWarp(int id);
    /// Push current GridWarpProfile to the GridWarp material uniforms.
    void updateGridWarpParams(int id);
    /// Reset all grid warps to identity (PANIC extension).
    void resetAllGridWarps();

    /// Apply zone warp/blend configuration to a slot (v3.4 Lot O — Scene Switcher).
    void applyZoneWarpBlend(int slotId, const WarpProfile& wp, bool warpEn,
                            const BlendProfile& bp, bool blendEn);

    /// Show / hide an output window (v3.5.1 — MasterView toggle).
    void setOutputVisible(int id, bool visible);
    bool isOutputVisible(int id) const;

    /// Enable / disable texture sharing on a slot (v3.4 Lot H, cross-platform Lot N).
    void enableTextureShare(int id, const std::string& sourceName = "");
    void disableTextureShare(int id);
    bool isTextureShareEnabled(int id) const;

    /// JSON serialisation for project save/load.
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j, Ogre::SceneManager* sceneMgr);

private:
    SurfaceMap* mSurfaceMap = nullptr;      ///< Optional, for zone camera lookup (Lot E)
    Ogre::TexturePtr mSourceTexture;        ///< Main scene texture to blit (set by StudioEngine)
    SDL_GLContext mSharedGLContext = nullptr;
    SDL_Window* mMainWindow = nullptr;
    int mNextId = 0;
    std::vector<OutputSlot> mSlots;

    // ── Blit shader: draws OGRE RenderTexture onto output windows ───
    unsigned int mBlitProg = 0;
    unsigned int mBlitVAO  = 0;

    void initBlitGL();
    void blitToWindow(const OutputSlot& slot, unsigned int glTexId);

#ifdef _WIN32
    /// Create a native Win32 output window (bypasses SDL_CreateWindow which
    /// corrupts AMD driver GL FBO state).  Sets pixel format for GL rendering.
    bool createNativeOutputWindow(OutputSlot& slot);
    /// Destroy a native Win32 output window.
    void destroyNativeOutputWindow(OutputSlot& slot);
    /// Make the native output window's DC current for GL rendering.
    bool makeNativeWindowCurrent(OutputSlot& slot);
    /// Swap buffers on the native output window.
    void swapNativeWindow(OutputSlot& slot);
    /// Restore the main window GL context after rendering to native output.
    void restoreMainContext();
    void* mMainDC = nullptr; ///< HDC of the main SDL window (cached for wglMakeCurrent)
    static bool sWndClassRegistered;
#endif

    // ── WarpWizard test pattern GL resources (v3.4 Lot D) ───────────
    unsigned int mTestPatternProg = 0; ///< GL shader program
    unsigned int mTestPatternVAO  = 0; ///< Fullscreen quad VAO
    unsigned int mTestPatternVBO  = 0; ///< Fullscreen quad VBO

    /// Lazily compile and link the test pattern GL3.3 program.
    void initTestPatternGL();

    /// Render the test pattern overlay for a slot into the current GL context.
    /// Must be called with the slot's GL context already current.
    void renderTestPattern(const OutputSlot& slot);
};

} // namespace bbfx

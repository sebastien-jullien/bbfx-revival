#include "StudioEngine.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"

#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreColourValue.h>
#include <OgreRoot.h>
#include <OgreCompositorManager.h>
#include <OgreCompositorChain.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

// OpenGL — use the loader provided by imgui_impl_opengl3 (already included above)
// On Windows this resolves to the system opengl32.lib. For GL function calls
// we rely on the loader; raw legacy calls (glViewport etc.) are in opengl32.
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

#include <iostream>

namespace bbfx {

// ── GLStateGuard RAII ──────────────────────────────────────────────────────

#ifdef _WIN32
using PFN_glBindFramebuffer = void(APIENTRY*)(unsigned int, unsigned int);
static PFN_glBindFramebuffer sGLBindFBO = nullptr;
static void ensureGLStateGuardFuncs() {
    if (!sGLBindFBO)
        sGLBindFBO = reinterpret_cast<PFN_glBindFramebuffer>(wglGetProcAddress("glBindFramebuffer"));
}
#endif

GLStateGuard::GLStateGuard() {
#ifdef _WIN32
    ensureGLStateGuardFuncs();
    glGetIntegerv(0x8CA6 /*GL_FRAMEBUFFER_BINDING*/, &mSavedFBO);
    glGetIntegerv(GL_VIEWPORT, mSavedViewport);
#endif
}

GLStateGuard::~GLStateGuard() {
#ifdef _WIN32
    if (sGLBindFBO)
        sGLBindFBO(0x8D40 /*GL_FRAMEBUFFER*/, static_cast<unsigned int>(mSavedFBO));
    glViewport(mSavedViewport[0], mSavedViewport[1], mSavedViewport[2], mSavedViewport[3]);
#endif
}

StudioEngine::StudioEngine(sol::state& lua)
    // Phase 1: SDL3 init + window with OpenGL flag, defer OGRE init.
    : Engine(lua, SDL_WINDOW_OPENGL, true)
{
    // Set OpenGL attributes BEFORE creating the GL context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create the OpenGL context via SDL3.
    mGLContext = SDL_GL_CreateContext(mWindow);
    if (!mGLContext) {
        throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    }
    SDL_GL_MakeCurrent(mWindow, mGLContext);
    SDL_GL_SetSwapInterval(1); // vsync

    // Phase 2: OGRE init using the existing SDL3 GL context.
    initOGRE(true /* externalGLContext */);

    // The main render window is not used for rendering in Studio mode —
    // OGRE renders to a RenderTexture displayed via ImGui::Image().
    // Remove the Engine-created viewport from the RenderWindow so the camera
    // has only ONE viewport (the RenderTexture one), preventing aspect ratio conflicts.
    if (mRenderWindow) {
        mRenderWindow->setAutoUpdated(false);
        mRenderWindow->removeAllViewports();
    }

    // Create the initial RenderTexture at default resolution.
    initRenderTexture(mRTWidth, mRTHeight);

    // Create OutputManager (v3.4 — multi-output).
    mOutputManager = std::make_unique<OutputManager>(mGLContext, mWindow);
    mOutputManager->setSourceTexture(mRenderTex);

    // Set window title and resize to a comfortable default for the Studio.
    SDL_SetWindowTitle(mWindow, "BBFx Studio v3.4");
    SDL_SetWindowSize(mWindow, 1400, 900);
    SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    std::cout << "[StudioEngine] Ready (GL context shared, RenderTexture "
              << mRTWidth << "×" << mRTHeight << ")" << std::endl;
}

StudioEngine::~StudioEngine() {
    // Destroy output windows before GL context is destroyed.
    mOutputManager.reset();

    // RenderTexture and Texture are managed by OGRE's TextureManager — let OGRE destroy them.
    mRenderTarget = nullptr;
    mRenderTex.reset();

    if (mGLContext) {
        SDL_GL_DestroyContext(mGLContext);
        mGLContext = nullptr;
    }
}

void StudioEngine::initRenderTexture(uint32_t width, uint32_t height) {
    if (!mRoot) return;

    // Ensure main GL context is current before any OGRE texture operations.
    SDL_GL_MakeCurrent(mWindow, mGLContext);

    // Destroy previous textures if they exist.
    // Drop ALL references before remove() so the old GL FBO is fully freed
    // before createManual() — avoids crash from two RenderTextures coexisting.
    if (mRenderTex) {
        if (mOutputManager)
            mOutputManager->setSourceTexture(nullptr);
        mRenderTarget = nullptr;
        mRenderTex.reset();
        Ogre::TextureManager::getSingleton().remove("StudioRenderTexture",
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    }
    mRTWidth  = width;
    mRTHeight = height;

    // ── RT1: Scene RenderTexture (no compositors) ─────────────────────────
    mRenderTex = Ogre::TextureManager::getSingleton().createManual(
        "StudioRenderTexture",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        width, height, 0,
        Ogre::PF_R8G8B8A8,
        Ogre::TU_RENDERTARGET
    );

    mRenderTarget = mRenderTex->getBuffer()->getRenderTarget();
    if (!mRenderTarget) {
        throw std::runtime_error("[StudioEngine] Failed to get RenderTarget from RenderTexture");
    }
    mRenderTarget->setAutoUpdated(false);

    // Attach main camera to RT1 viewport.
    if (mSceneManager && mSceneManager->hasCamera("MainCamera")) {
        Ogre::Camera* cam = mSceneManager->getCamera("MainCamera");
        auto* vp = mRenderTarget->addViewport(cam);
        vp->setBackgroundColour(Ogre::ColourValue(0.12f, 0.12f, 0.12f));
        vp->setOverlaysEnabled(true);
        cam->setAutoAspectRatio(false);
        Ogre::Real ar = static_cast<Ogre::Real>(width) / static_cast<Ogre::Real>(height);
        cam->setAspectRatio(ar);
    }

    // Update OutputManager's source texture reference (it blits this to output windows).
    if (mOutputManager)
        mOutputManager->setSourceTexture(mRenderTex);

    std::cout << "[StudioEngine] initRenderTexture " << width << "x" << height
              << std::endl;
}

void StudioEngine::resizeRenderTexture(uint32_t width, uint32_t height) {
    // Clamp to minimum 64×64 — smaller textures cause GL driver crashes
    // (e.g. ImGui panels can report 16×1 during layout).
    if (width  < 64) width  = 64;
    if (height < 64) height = 64;
    if (!mForceRTRebuild && width == mRTWidth && height == mRTHeight) return;
    mForceRTRebuild = false;
    initRenderTexture(width, height);
    mCachedFBO = -1;       // new FBO — must re-discover ID
}

void StudioEngine::updateRenderTarget() {
    if (!mRenderTarget || mRenderTarget->getNumViewports() == 0) return;

    // Ensure we're on the main window GL context before OGRE rendering.
    // OutputManager::updateAll() may have switched to an output window context.
    SDL_GL_MakeCurrent(mWindow, mGLContext);



    // Force correct aspect ratio every frame.
    auto* vp = mRenderTarget->getViewport(0);
    if (vp && vp->getCamera()) {
        vp->getCamera()->setAspectRatio(
            static_cast<Ogre::Real>(mRTWidth) / static_cast<Ogre::Real>(mRTHeight));
    }

    // OGRE GL3Plus caches both mActiveRenderTarget and the GL viewport/FBO state.
    // Between frames, ImGui renders to framebuffer 0 with the full window viewport.
    // We must force-bind the correct FBO AND reset glViewport before OGRE renders.
#ifdef _WIN32
    if (mCachedFBO < 0) {
        // First render after creation/resize: let OGRE bind normally, then capture FBO ID.
        mRenderTarget->update();
        glGetIntegerv(0x8CA6 /*GL_FRAMEBUFFER_BINDING*/, &mCachedFBO);
    } else {
        // Subsequent renders: force-bind the FBO and viewport that ImGui clobbered.
        using BindFBOFunc = void(APIENTRY*)(unsigned int, unsigned int);
        static auto sBindFBO = reinterpret_cast<BindFBOFunc>(wglGetProcAddress("glBindFramebuffer"));
        if (sBindFBO)
            sBindFBO(0x8D40 /*GL_FRAMEBUFFER*/, static_cast<unsigned int>(mCachedFBO));
        glViewport(0, 0, static_cast<int>(mRTWidth), static_cast<int>(mRTHeight));
        mRenderTarget->update();
    }
#else
    mRenderTarget->update();
#endif

    // Re-bind our FBO after render (compositors may have changed it)
#ifdef _WIN32
    if (mCachedFBO >= 0) {
        using BindFBOFunc = void(APIENTRY*)(unsigned int, unsigned int);
        static auto sBindFBO2 = reinterpret_cast<BindFBOFunc>(wglGetProcAddress("glBindFramebuffer"));
        if (sBindFBO2)
            sBindFBO2(0x8D40 /*GL_FRAMEBUFFER*/, static_cast<unsigned int>(mCachedFBO));
    }
#endif
}

bool StudioEngine::captureFrame(const std::string& path) {
    if (!mRenderTarget) return false;
    mRenderTarget->update();
    mRenderTarget->writeContentsToFile(path);
    return true;
}

ImTextureID StudioEngine::getRenderTextureID() const {
    if (!mRenderTex) return 0;
    unsigned int glId = 0;
    mRenderTex->getCustomAttribute("GLID", &glId);
    return static_cast<ImTextureID>(glId);
}

// ── Combined ImGui + OGRE render loop ────────────────────────────────────────
// This is overridden by StudioApp which provides the full UI.
// StudioEngine::startRendering() is a minimal fallback.
void StudioEngine::startRendering() {
    // ImGui init (minimal — StudioApp does the real setup)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForOpenGL(mWindow, mGLContext);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    mStopQueued = false;
    SDL_Event evt;
    auto* animator = Animator::instance();
    auto* time = RootTimeNode::instance();
    if (time) time->reset();

    while (!mStopQueued) {
        while (SDL_PollEvent(&evt)) {
            ImGui_ImplSDL3_ProcessEvent(&evt);
            if (mInputManager) mInputManager->handleSDLEvent(evt);
            if (evt.type == SDL_EVENT_QUIT) mStopQueued = true;
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_ESCAPE) mStopQueued = true;
        }

        if (time) time->update();
        if (animator) animator->renderOneFrame();

        // Render OGRE to RenderTexture (off-screen).
        if (mRenderTarget) mRenderTarget->update();

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Minimal viewport (fullscreen image of the OGRE render)
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::Image(getRenderTextureID(), ImGui::GetContentRegionAvail());
        ImGui::End();

        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(mWindow);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

// ── Output Manager delegates (v3.4 — replaces dual output v3.3) ─────────────

bool StudioEngine::openOutputWindow(int width, int height) {
    if (!mOutputManager) return false;
    if (mOutputManager->count() > 0) return true; // already open
    return mOutputManager->addOutput(width, height, mSceneManager) >= 0;
}

void StudioEngine::closeOutputWindow() {
    if (!mOutputManager || mOutputManager->count() == 0) return;
    std::cout << "[StudioEngine::closeOutputWindow] calling removeOutput" << std::endl;
    mOutputManager->removeOutput(mOutputManager->getAllSlots().front().id);
}

bool StudioEngine::isOutputOpen() const {
    return mOutputManager && mOutputManager->count() > 0;
}

void StudioEngine::toggleOutputFullscreen() {
    if (!mOutputManager || mOutputManager->count() == 0) return;
    mOutputManager->toggleFullscreen(mOutputManager->getAllSlots().front().id);
}

void StudioEngine::setOutputResolution(int w, int h) {
    if (!mOutputManager || mOutputManager->count() == 0) return;
    mOutputManager->setResolution(mOutputManager->getAllSlots().front().id, w, h, mSceneManager);
}

void StudioEngine::setOutputMonitor(int idx) {
    if (!mOutputManager || mOutputManager->count() == 0) return;
    mOutputManager->setMonitor(mOutputManager->getAllSlots().front().id, idx);
}

void StudioEngine::updateOutputTarget() {
    if (!mOutputManager) return;
    mOutputManager->updateAll();
}

ImTextureID StudioEngine::getOutputTextureID() const {
    if (!mOutputManager || mOutputManager->count() == 0) return 0;
    return mOutputManager->getTextureID(mOutputManager->getAllSlots().front().id);
}

} // namespace bbfx

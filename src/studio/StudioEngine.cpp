#include "StudioEngine.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"

#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreColourValue.h>
#include <OgreRoot.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

// OpenGL — use the loader provided by imgui_impl_opengl3 (already included above)
// On Windows this resolves to the system opengl32.lib. For GL function calls
// we rely on the loader; raw legacy calls (glViewport etc.) are in opengl32.
#ifdef _WIN32
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

#include <iostream>

namespace bbfx {

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

    // Disable auto-update on the main render window — we render to a RenderTexture.
    if (mRenderWindow) {
        mRenderWindow->setAutoUpdated(false);
    }

    // Create the initial RenderTexture at default resolution.
    initRenderTexture(mRTWidth, mRTHeight);

    // Set window title and resize to a comfortable default for the Studio.
    SDL_SetWindowTitle(mWindow, "BBFx Studio v3.0");
    SDL_SetWindowSize(mWindow, 1400, 900);
    SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    std::cout << "[StudioEngine] Ready (GL context shared, RenderTexture "
              << mRTWidth << "×" << mRTHeight << ")" << std::endl;
}

StudioEngine::~StudioEngine() {
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

    // Destroy previous texture if it exists.
    if (mRenderTex) {
        Ogre::TextureManager::getSingleton().remove("StudioRenderTexture",
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        mRenderTarget = nullptr;
        mRenderTex.reset();
    }

    mRTWidth  = width;
    mRTHeight = height;

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

    // Attach main camera to the RenderTexture viewport.
    if (mSceneManager && mSceneManager->hasCamera("MainCamera")) {
        Ogre::Camera* cam = mSceneManager->getCamera("MainCamera");
        auto* vp = mRenderTarget->addViewport(cam);
        vp->setBackgroundColour(Ogre::ColourValue(0.12f, 0.12f, 0.12f));
        vp->setOverlaysEnabled(true);
    }
}

void StudioEngine::resizeRenderTexture(uint32_t width, uint32_t height) {
    if (width == mRTWidth && height == mRTHeight) return;
    initRenderTexture(width, height);
}

void StudioEngine::updateRenderTarget() {
    if (mRenderTarget && mRenderTarget->getNumViewports() > 0) {
        mRenderTarget->update();
    }
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

} // namespace bbfx

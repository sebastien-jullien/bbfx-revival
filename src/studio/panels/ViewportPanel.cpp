#include "ViewportPanel.h"
#include <imgui.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreViewport.h>
#include <OgreFrameListener.h>
#include <OgreSceneManager.h>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
static PFNGLBINDFRAMEBUFFERPROC sGLBindFramebuffer = nullptr;
#endif

namespace bbfx {

ViewportPanel::ViewportPanel(StudioEngine* engine)
    : mEngine(engine) {}

void ViewportPanel::updateOgreRender() {
    if (!mEngine) return;
    syncSize();

    auto* root = Ogre::Root::getSingletonPtr();
    auto* rt = mEngine->getRenderTarget();
    if (!root || !rt || rt->getNumViewports() == 0) return;

    Ogre::FrameEvent evt;
    evt.timeSinceLastEvent = evt.timeSinceLastFrame = 0.016f;
    root->_fireFrameStarted(evt);

    // OGRE GL3Plus caches mActiveRenderTarget and skips FBO re-bind when the
    // C++ pointer hasn't changed.  Between frames ImGui renders to framebuffer 0
    // behind OGRE's back, so the FBO is unbound but OGRE doesn't know.
    // Fix: capture the FBO ID on the first render, then force-bind it before
    // every subsequent rt->update().
#ifdef _WIN32
    if (mOgreFBO < 0) {
        rt->update();
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mOgreFBO);
    } else {
        if (!sGLBindFramebuffer)
            sGLBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
        if (sGLBindFramebuffer)
            sGLBindFramebuffer(GL_FRAMEBUFFER, (GLuint)mOgreFBO);
        rt->update();
    }
#else
    rt->update();
#endif

    root->_fireFrameEnded(evt);
}

void ViewportPanel::syncSize() {
    if (mPendingW > 0 && mPendingH > 0 &&
        (mPendingW != mLastWidth || mPendingH != mLastHeight)) {
        mEngine->resizeRenderTexture(mPendingW, mPendingH);
        mLastWidth  = mPendingW;
        mLastHeight = mPendingH;
        mOgreFBO = -1; // new render texture → need to recapture FBO ID
    }
}

void ViewportPanel::render() {
    ImGui::Begin("Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    if (panelSize.x < 1.0f) panelSize.x = 1.0f;
    if (panelSize.y < 1.0f) panelSize.y = 1.0f;

    mPendingW = static_cast<uint32_t>(panelSize.x);
    mPendingH = static_cast<uint32_t>(panelSize.y);

    ImTextureID texId = mEngine->getRenderTextureID();
    if (texId) {
        ImGui::Image(texId, panelSize, {0.0f, 1.0f}, {1.0f, 0.0f});
    } else {
        ImGui::TextDisabled("(OGRE RenderTexture not ready)");
    }

    if (mShowOverlay) {
        ImVec2 overlayPos = ImGui::GetItemRectMin();
        overlayPos.x += 8.0f;
        overlayPos.y += 8.0f;
        ImGui::SetNextWindowPos(overlayPos);
        ImGui::SetNextWindowBgAlpha(0.45f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.0f, 4.0f});
        ImGui::Begin("##vpOverlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
        ImGui::Text("%u\xc3\x97%u", mPendingW, mPendingH);
        ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "Design Mode");
        ImGui::End();
        ImGui::PopStyleVar();
    }

    ImGui::End();
}

} // namespace bbfx

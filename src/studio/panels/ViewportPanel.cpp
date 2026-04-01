#include "ViewportPanel.h"
#include <imgui.h>
#include <OgreRoot.h>
#include <OgreFrameListener.h>

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

    // FBO + glViewport fix is now handled inside StudioEngine::updateRenderTarget().
    mEngine->updateRenderTarget();

    root->_fireFrameEnded(evt);
}

void ViewportPanel::syncSize() {
    if (mPendingW > 0 && mPendingH > 0 &&
        (mPendingW != mLastWidth || mPendingH != mLastHeight)) {
        mEngine->resizeRenderTexture(mPendingW, mPendingH);
        mLastWidth  = mPendingW;
        mLastHeight = mPendingH;
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
        ImGui::Image(texId, panelSize, {0.0f, 0.0f}, {1.0f, 1.0f});
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

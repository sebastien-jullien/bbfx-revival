#include "ViewportPanel.h"
#include "../../core/Animator.h"
#include "../nodes/SceneObjectNode.h"
#include <imgui.h>
#include <OgreRoot.h>
#include <OgreFrameListener.h>
#include <SDL3/SDL.h>
#include <cmath>

namespace bbfx {

ViewportPanel::ViewportPanel(StudioEngine* engine)
    : mEngine(engine)
{
    auto* sm  = engine->getSceneManager();
    auto* cam = sm ? sm->getCamera("MainCamera") : nullptr;
    if (sm && cam) {
        mCameraController = std::make_unique<ViewportCameraController>(sm, cam);
        mPicker = std::make_unique<ViewportPicker>(sm, cam);
        mGrid = std::make_unique<ViewportGrid>(sm);
        mGizmo = std::make_unique<ViewportGizmo>(sm, cam);
    }
}

void ViewportPanel::updateOgreRender() {
    if (!mEngine) return;
    syncSize();

    auto* root = Ogre::Root::getSingletonPtr();
    auto* rt = mEngine->getRenderTarget();
    if (!root || !rt || rt->getNumViewports() == 0) return;

    Ogre::FrameEvent evt;
    evt.timeSinceLastEvent = evt.timeSinceLastFrame = 0.016f;
    root->_fireFrameStarted(evt);

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

    // Render toolbar at the top
    mToolbar.render();

    // Sync toolbar state to grid
    if (mGrid) {
        mGrid->setVisible(mToolbar.isGridOn() && mToolbar.areOverlaysOn());
    }

    // Recalculate available size after toolbar
    panelSize = ImGui::GetContentRegionAvail();
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

    // ── Viewport interaction ──────────────────────────────────────────────────
    mIsHovered = ImGui::IsItemHovered();

    auto& io = ImGui::GetIO();
    bool rightDown  = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool altDown    = io.KeyAlt;

    // Detect right-click start/end for mouse capture
    bool wasRightDown = mRightMouseDown;
    mRightMouseDown = rightDown;

    // Lock cursor when right-click starts on viewport, restore position when released
    if (rightDown && !wasRightDown && mIsHovered) {
        mFpsCaptured = true;
        mFpsFirstFrame = true; // skip first delta (relative mode produces garbage)
        mSavedMouseX = io.MousePos.x;
        mSavedMouseY = io.MousePos.y;
        SDL_SetWindowRelativeMouseMode(mEngine->getSDLWindow(), true);
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    if (!rightDown && wasRightDown && mFpsCaptured) {
        mFpsCaptured = false;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        SDL_SetWindowRelativeMouseMode(mEngine->getSDLWindow(), false);
        SDL_WarpMouseInWindow(mEngine->getSDLWindow(),
                              mSavedMouseX, mSavedMouseY);
    }

    // Hovered interactions (picking, gizmo, orbit, pan, zoom)
    if (mIsHovered && mCameraController) {
        float dx = io.MouseDelta.x;
        float dy = io.MouseDelta.y;
        bool leftDown   = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

        // Compute normalized viewport coordinates
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 rectMin  = ImGui::GetItemRectMin();
        ImVec2 rectMax  = ImGui::GetItemRectMax();
        float w = rectMax.x - rectMin.x;
        float h = rectMax.y - rectMin.y;
        float nx = (w > 0) ? (mousePos.x - rectMin.x) / w : 0;
        float ny = (h > 0) ? (mousePos.y - rectMin.y) / h : 0;

        // Sync toolbar state to gizmo
        if (mGizmo) {
            auto toolbarTool = mToolbar.getTool();
            ViewportGizmo::Tool gTool = ViewportGizmo::Tool::Translate;
            if (toolbarTool == ViewportToolbar::Tool::Rotate) gTool = ViewportGizmo::Tool::Rotate;
            if (toolbarTool == ViewportToolbar::Tool::Scale)  gTool = ViewportGizmo::Tool::Scale;
            mGizmo->setTool(gTool);
            mGizmo->setSpace(mToolbar.getSpace() == ViewportToolbar::Space::Local
                             ? ViewportGizmo::Space::Local : ViewportGizmo::Space::World);
            mGizmo->setSnap(mToolbar.isSnapOn() || io.KeyCtrl, mToolbar.getSnapSize());
        }

        // Gizmo mouse events (highest priority)
        bool gizmoConsumed = false;
        if (mGizmo && mGizmo->hasTarget()) {
            // Keyboard mode (G/R/S): LMB confirms, mouse moves apply
            if (mGizmo->isInKeyboardMode()) {
                gizmoConsumed = mGizmo->handleMouseMove(nx, ny, dx, dy, io.KeyCtrl);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    mGizmo->confirmKeyboardTransform();
                    gizmoConsumed = true;
                }
            }
            // Normal drag mode
            else {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !altDown) {
                    gizmoConsumed = mGizmo->handleMouseDown(nx, ny, 0);
                }
                if (mGizmo->isDragging()) {
                    gizmoConsumed = mGizmo->handleMouseMove(nx, ny, dx, dy, io.KeyCtrl);
                }
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && mGizmo->isDragging()) {
                    mGizmo->handleMouseUp(nx, ny, 0);
                    gizmoConsumed = true;
                }
            }
            // Update gizmo visuals
            mGizmo->update();
        }

        // Picking — left click without Alt (only if gizmo didn't consume)
        if (!gizmoConsumed && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !altDown && mPicker) {
            if (w > 0 && h > 0) {
                auto* hit = mPicker->pick(nx, ny);
                if (hit) {
                    mPicker->select(hit);
                    // Set gizmo target
                    auto* dagNode = Animator::instance()->getRegisteredNode(mPicker->getSelectedNodeName());
                    auto* soNode = dynamic_cast<SceneObjectNode*>(dagNode);
                    if (soNode && soNode->getSceneNode()) {
                        if (mGizmo) mGizmo->setTarget(soNode->getSceneNode(), dagNode);
                        if (mCameraController) mCameraController->setOrbitLockTarget(soNode->getSceneNode());
                    } else {
                        if (mGizmo) mGizmo->clearTarget();
                        if (mCameraController) mCameraController->setOrbitLockTarget(nullptr);
                    }
                } else {
                    mPicker->deselect();
                    if (mGizmo) mGizmo->clearTarget();
                    if (mCameraController) mCameraController->setOrbitLockTarget(nullptr);
                }
            }
        }

        // Camera interaction (non-FPS: orbit, pan)
        if (!mFpsCaptured && (dx != 0 || dy != 0)) {
            mCameraController->handleMouseMove(dx, dy, leftDown, rightDown, middleDown, altDown);
        }

        // Mouse wheel — zoom (only in orbit mode, not FPS)
        if (!mFpsCaptured && io.MouseWheel != 0) {
            mCameraController->handleMouseWheel(io.MouseWheel, 0, 0);
        }
    }

    // FPS mode: camera look + ZQSD movement (works even when mouse leaves viewport)
    if (mFpsCaptured && mCameraController) {
        float dx = io.MouseDelta.x;
        float dy = io.MouseDelta.y;

        // Skip first frame delta (relative mouse mode produces a jump)
        if (mFpsFirstFrame) {
            mFpsFirstFrame = false;
            dx = dy = 0;
        }

        bool ctrl = io.KeyCtrl;
        if (!ctrl) {
            mWasLockOn = false;
            if (dx != 0 || dy != 0) {
                mCameraController->handleMouseMove(dx, dy, false, true, false, false);
            }
            mCameraController->handleKeyboard(io.DeltaTime);
        } else {
            bool entering = !mWasLockOn;
            mWasLockOn = true;
            // On entering lock-on, auto-pick what's at screen center
            if (entering && mPicker) {
                auto* hit = mPicker->pick(0.5f, 0.5f);
                if (hit && hit->getParentSceneNode()) {
                    mCameraController->setOrbitLockTarget(hit->getParentSceneNode());
                } else {
                    // Nothing hit — lock onto ground plane intersection
                    auto* cam = mEngine->getSceneManager()->getCamera("MainCamera");
                    if (cam) {
                        auto ray = cam->getCameraToViewportRay(0.5f, 0.5f);
                        if (std::abs(ray.getDirection().y) > 0.001f) {
                            float t = -ray.getOrigin().y / ray.getDirection().y;
                            if (t > 0) {
                                auto groundPt = ray.getPoint(t);
                                mCameraController->setOrbitLockTarget(nullptr);
                                mCameraController->setLockOnCenter(groundPt);
                            }
                        }
                    }
                }
            }
            mCameraController->handleLockOn(io.DeltaTime, dx, dy, entering);
        }
        // Scroll wheel adjusts FPS move speed while RMB held
        if (io.MouseWheel != 0) {
            mCameraController->adjustMoveSpeed(io.MouseWheel);
            // Consume the wheel so other panels (node editor zoom) don't react
            io.MouseWheel = 0;
            io.MouseWheelH = 0;
        }
    }

    // Update smooth transitions
    if (mCameraController) {
        mCameraController->update(ImGui::GetIO().DeltaTime);
    }

    // ── Axis indicator (bottom-left corner) ──────────────────────────────────
    if (mToolbar.areOverlaysOn() && mCameraController) {
        auto* cam = mEngine->getSceneManager()->getCamera("MainCamera");
        if (cam) {
            ImVec2 rectMin = ImGui::GetItemRectMin();
            ImVec2 rectMax = ImGui::GetItemRectMax();
            float cx = rectMin.x + 50.0f;
            float cy = rectMax.y - 50.0f;
            float len = 30.0f;

            // Get camera orientation (view matrix rotation)
            auto viewMat = cam->getViewMatrix();
            auto rot = viewMat.linear(); // 3x3 rotation part

            // Project world axes through view rotation
            Ogre::Vector3 axisX = rot * Ogre::Vector3::UNIT_X;
            Ogre::Vector3 axisY = rot * Ogre::Vector3::UNIT_Y;
            Ogre::Vector3 axisZ = rot * Ogre::Vector3::UNIT_Z;

            auto* dl = ImGui::GetWindowDrawList();
            auto drawAxis = [&](Ogre::Vector3 a, ImU32 col, const char* label) {
                float sx = cx + a.x * len;
                float sy = cy - a.y * len; // screen Y is inverted
                dl->AddLine({cx, cy}, {sx, sy}, col, 2.0f);
                dl->AddText({sx + 2, sy - 6}, col, label);
            };

            drawAxis(axisX, IM_COL32(220, 60, 60, 255), "X");
            drawAxis(axisY, IM_COL32(60, 220, 60, 255), "Y");
            drawAxis(axisZ, IM_COL32(60, 60, 220, 255), "Z");
        }
    }

    // ── Overlay ───────────────────────────────────────────────────────────────
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

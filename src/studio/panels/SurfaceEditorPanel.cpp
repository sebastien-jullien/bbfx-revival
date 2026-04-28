#include "SurfaceEditorPanel.h"
#include "PerformanceModePanel.h"
#include "../SurfaceMap.h"
#include "../OutputManager.h"
#include "../StudioEngine.h"
#include "../ToastSystem.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>
#include <OgreCamera.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>

// stb_image for background image loading (header-only; implementation
// lives in src/media/stb_image_impl.cpp so multiple TUs can share it).
#include <stb_image.h>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <commdlg.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

namespace bbfx {

// ── Zone colour palette ───────────────────────────────────────────────────────

const ImVec4 SurfaceEditorPanel::kZoneColors[] = {
    {0.2f, 0.6f, 1.0f, 0.55f},  // blue
    {0.2f, 1.0f, 0.4f, 0.55f},  // green
    {1.0f, 0.6f, 0.1f, 0.55f},  // orange
    {1.0f, 0.2f, 0.8f, 0.55f},  // magenta
    {0.8f, 1.0f, 0.2f, 0.55f},  // yellow-green
    {0.6f, 0.2f, 1.0f, 0.55f},  // purple
    {0.2f, 1.0f, 1.0f, 0.55f},  // cyan
    {1.0f, 0.4f, 0.4f, 0.55f},  // red
};
const int SurfaceEditorPanel::kNumZoneColors =
    static_cast<int>(sizeof(kZoneColors) / sizeof(kZoneColors[0]));

// ── Destructor ────────────────────────────────────────────────────────────────

SurfaceEditorPanel::~SurfaceEditorPanel() {
    if (mBgTexture) {
        GLuint id = static_cast<GLuint>(mBgTexture);
        glDeleteTextures(1, &id);
        mBgTexture = 0;
    }
}

// GL_CLAMP_TO_EDGE is OpenGL 1.2 — define if not exposed by the Windows GL1.1 header
#ifndef GL_CLAMP_TO_EDGE
#  define GL_CLAMP_TO_EDGE 0x812F
#endif

// ── Background texture loading ────────────────────────────────────────────────

/// Load (or reload) the background image into a GL texture.
/// Called when backgroundImagePath changes.
static ImTextureID loadBgTexture(const std::string& path) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4); // force RGBA
    if (!data) {
        std::cerr << "[SurfaceEditor] Failed to load background image: " << path << std::endl;
        return 0;
    }
    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    std::cout << "[SurfaceEditor] Background loaded: " << path << " (" << w << "x" << h << ")" << std::endl;
    return static_cast<ImTextureID>(texId);
}

// ── Main render ───────────────────────────────────────────────────────────────

void SurfaceEditorPanel::render(StudioEngine* engine) {
    ImGui::Begin("Surface Editor");

    if (!mSurfaceMap) {
        ImGui::TextDisabled("No SurfaceMap available.");
        ImGui::End();
        return;
    }

    // Reload background GL texture if path changed
    if (mSurfaceMap->backgroundImagePath != mLoadedBgPath) {
        if (mBgTexture) {
            GLuint id = static_cast<GLuint>(mBgTexture);
            glDeleteTextures(1, &id);
        }
        mBgTexture = mSurfaceMap->backgroundImagePath.empty()
                   ? 0
                   : loadBgTexture(mSurfaceMap->backgroundImagePath);
        mLoadedBgPath = mSurfaceMap->backgroundImagePath;
    }

    renderToolbar(engine);
    ImGui::Separator();

    // Split: canvas on left, properties on right.
    float availW = ImGui::GetContentRegionAvail().x;
    float propW  = (mSelectedZoneId >= 0) ? 240.f : 0.f;
    float canvW  = availW - propW - (propW > 0 ? 8.f : 0.f);
    float canvH  = std::max(200.f, ImGui::GetContentRegionAvail().y - 30.f);

    ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = {canvW, canvH};

    renderCanvas(canvasPos, canvasSize);
    ImGui::SetCursorScreenPos({canvasPos.x + canvW + 8.f, canvasPos.y});
    ImGui::SetNextItemWidth(propW);

    if (mSelectedZoneId >= 0 && propW > 0.f) {
        ImGui::BeginGroup();
        renderProperties(engine);
        ImGui::EndGroup();
    }

    // Dimensions label.
    ImGui::SetCursorScreenPos({canvasPos.x, canvasPos.y + canvH + 4.f});
    ImGui::TextDisabled("Canvas: %.1fm x %.1fm | Zones: %d",
                        mSurfaceMap->canvasWidth, mSurfaceMap->canvasHeight,
                        mSurfaceMap->size());

    ImGui::End();
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

void SurfaceEditorPanel::renderToolbar(StudioEngine* engine) {
    if (ImGui::Button("Add Zone")) {
        float cx = 0.25f + (mSurfaceMap->size() % 4) * 0.05f;
        float cy = 0.25f;
        mSelectedZoneId = mSurfaceMap->addZone("", cx, cy, 0.4f, 0.4f);
        ToastSystem::instance().toast("Zone added");
    }
    ImGui::SameLine();

    if (mSelectedZoneId >= 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.f));
        if (ImGui::Button("Remove Zone")) {
            mSurfaceMap->removeZone(mSelectedZoneId);
            mSelectedZoneId = -1;
            ToastSystem::instance().toast("Zone removed");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    if (ImGui::Button("Reset All")) {
        ImGui::OpenPopup("ConfirmResetAll");
    }
    if (ImGui::BeginPopup("ConfirmResetAll")) {
        ImGui::Text("Remove ALL zones?");
        if (ImGui::Button("Yes")) { mSurfaceMap->clear(); mSelectedZoneId = -1; ToastSystem::instance().toast("All zones cleared"); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Capture Scene button (v3.4 Lot O)
    if (ImGui::Button("Capture Scene")) {
        ImGui::OpenPopup("CaptureScenePopup");
    }
    if (ImGui::BeginPopup("CaptureScenePopup")) {
        if (mPerfPanel && mSurfaceMap) {
            const auto& chordSnaps = mPerfPanel->getChordSnapshots();
            if (chordSnaps.empty()) {
                ImGui::TextDisabled("No chords — create chords in Timeline first");
            } else {
                for (const auto& [name, _] : chordSnaps) {
                    bool hasScene = mPerfPanel->hasChordZoneSnapshot(name);
                    char label[128];
                    snprintf(label, sizeof(label), "%s%s", name.c_str(), hasScene ? " (has scene)" : "");
                    if (ImGui::MenuItem(label)) {
                        Ogre::Camera* cam = nullptr;
                        if (engine && engine->getSceneManager()->hasCamera("MainCamera"))
                            cam = engine->getSceneManager()->getCamera("MainCamera");
                        mPerfPanel->captureChordZoneSnapshot(name, *mSurfaceMap, cam);
                        ToastSystem::instance().toast("Scene captured for chord '" + name + "' (" +
                            std::to_string(mSurfaceMap->size()) + " zones)");
                    }
                }
            }
        } else {
            ImGui::TextDisabled("Performance mode not available");
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Import Background")) {
#ifdef _WIN32
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Images (*.png;*.jpg)\0*.png;*.jpg\0All Files\0*.*\0";
        ofn.lpstrFile   = filename;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrTitle  = "Select Background Image";
        ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) {
            mSurfaceMap->backgroundImagePath = filename;
            mLoadedBgPath.clear(); // Force reload
            mBgTexture = 0;
        }
#endif
    }
    ImGui::SameLine();

    if (!mSurfaceMap->backgroundImagePath.empty()) {
        if (ImGui::Button("Clear Background")) {
            mSurfaceMap->backgroundImagePath.clear();
            mBgTexture = 0;
            mLoadedBgPath.clear();
        }
    }

    // Canvas size controls.
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("Canvas:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.f);
    ImGui::DragFloat("##cw", &mSurfaceMap->canvasWidth, 0.1f, 0.5f, 100.f, "%.1fm");
    ImGui::SameLine();
    ImGui::TextDisabled("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.f);
    ImGui::DragFloat("##ch", &mSurfaceMap->canvasHeight, 0.1f, 0.5f, 50.f, "%.1fm");
}

// ── Canvas ────────────────────────────────────────────────────────────────────

ImVec2 SurfaceEditorPanel::canvasToNorm(ImVec2 canvasPos, ImVec2 canvasSize, ImVec2 screenPos) const {
    return {
        (screenPos.x - canvasPos.x) / canvasSize.x,
        (screenPos.y - canvasPos.y) / canvasSize.y
    };
}

int SurfaceEditorPanel::zoneAtPos(float nx, float ny) const {
    if (!mSurfaceMap) return -1;
    // Iterate in reverse to hit top-most zone first.
    const auto& zones = mSurfaceMap->getAllZones();
    for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i) {
        const auto& z = zones[i];
        if (nx >= z.x && nx <= z.x + z.width &&
            ny >= z.y && ny <= z.y + z.height) {
            return z.id;
        }
    }
    return -1;
}

void SurfaceEditorPanel::renderCanvas(ImVec2 canvasPos, ImVec2 canvasSize) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Canvas background.
    dl->AddRectFilled(canvasPos,
                      {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                      IM_COL32(30, 30, 35, 255));
    dl->AddRect(canvasPos,
                {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                IM_COL32(80, 80, 90, 255), 2.f);

    // Grid (1m spacing).
    float scaleX = canvasSize.x / mSurfaceMap->canvasWidth;
    float scaleY = canvasSize.y / mSurfaceMap->canvasHeight;
    ImU32 gridCol = IM_COL32(55, 55, 65, 255);
    for (float mx = 1.f; mx < mSurfaceMap->canvasWidth; mx += 1.f) {
        float px = canvasPos.x + mx * scaleX;
        dl->AddLine({px, canvasPos.y}, {px, canvasPos.y + canvasSize.y}, gridCol);
    }
    for (float my = 1.f; my < mSurfaceMap->canvasHeight; my += 1.f) {
        float py = canvasPos.y + my * scaleY;
        dl->AddLine({canvasPos.x, py}, {canvasPos.x + canvasSize.x, py}, gridCol);
    }

    // Background image (if any) — draw as tinted rect (actual texture loading not
    // implemented here; path is displayed as text for now).
    if (mBgTexture) {
        dl->AddImage(mBgTexture, canvasPos,
                     {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                     {0, 0}, {1, 1}, IM_COL32(255, 255, 255, 120));
    }

    // Zones.
    const auto& zones = mSurfaceMap->getAllZones();
    for (size_t zi = 0; zi < zones.size(); ++zi) {
        const auto& z = zones[zi];
        ImVec4 colV = kZoneColors[zi % kNumZoneColors];
        if (z.outputSlotId < 0) { colV.x *= 0.4f; colV.y *= 0.4f; colV.z *= 0.4f; } // grey if unassigned

        ImVec2 zMin = {canvasPos.x + z.x * canvasSize.x,        canvasPos.y + z.y * canvasSize.y};
        ImVec2 zMax = {canvasPos.x + (z.x + z.width) * canvasSize.x, canvasPos.y + (z.y + z.height) * canvasSize.y};

        ImU32 fillCol   = ImGui::ColorConvertFloat4ToU32(colV);
        ImU32 borderCol = (z.id == mSelectedZoneId) ? IM_COL32(255, 255, 80, 255) : IM_COL32(180, 180, 200, 200);
        float borderW   = (z.id == mSelectedZoneId) ? 2.5f : 1.0f;

        dl->AddRectFilled(zMin, zMax, fillCol, 3.f);
        dl->AddRect(zMin, zMax, borderCol, 3.f, 0, borderW);

        // Zone name.
        char label[128];
        snprintf(label, sizeof(label), "%s", z.name.c_str());
        float cx = (zMin.x + zMax.x) * 0.5f;
        float cy = (zMin.y + zMax.y) * 0.5f;
        dl->AddText({cx - ImGui::CalcTextSize(label).x * 0.5f, cy - 8.f},
                    IM_COL32(255, 255, 255, 230), label);

        // Output assignment.
        if (z.outputSlotId >= 0) {
            char outLabel[32];
            snprintf(outLabel, sizeof(outLabel), "→ Out %d", z.outputSlotId);
            dl->AddText({cx - ImGui::CalcTextSize(outLabel).x * 0.5f, cy + 4.f},
                        IM_COL32(200, 230, 255, 200), outLabel);
        }

        // Resize handles (8 handles) for selected zone.
        if (z.id == mSelectedZoneId) {
            const float hs = 6.f; // handle size
            ImVec2 handles[8] = {
                {zMin.x, zMin.y},                         // TL
                {(zMin.x + zMax.x) * 0.5f, zMin.y},       // T
                {zMax.x, zMin.y},                          // TR
                {zMax.x, (zMin.y + zMax.y) * 0.5f},       // R
                {zMax.x, zMax.y},                          // BR
                {(zMin.x + zMax.x) * 0.5f, zMax.y},       // B
                {zMin.x, zMax.y},                          // BL
                {zMin.x, (zMin.y + zMax.y) * 0.5f},       // L
            };
            for (const auto& h : handles) {
                dl->AddRectFilled({h.x - hs, h.y - hs}, {h.x + hs, h.y + hs},
                                  IM_COL32(255, 255, 255, 200));
            }
        }
    }

    // Context menu (right-click on empty canvas).
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##canvas", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 normPos  = canvasToNorm(canvasPos, canvasSize, mousePos);

    // Right-click context menu.
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        int hitId = zoneAtPos(normPos.x, normPos.y);
        if (hitId >= 0) {
            mSelectedZoneId = hitId;
            ImGui::OpenPopup("ZoneCtxMenu");
        } else {
            ImGui::OpenPopup("CanvasCtxMenu");
        }
    }

    if (ImGui::BeginPopup("ZoneCtxMenu")) {
        auto* zone = mSurfaceMap->getZone(mSelectedZoneId);
        if (zone) {
            ImGui::TextDisabled("Zone: %s", zone->name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                mSurfaceMap->removeZone(mSelectedZoneId);
                mSelectedZoneId = -1;
                ToastSystem::instance().toast("Zone removed");
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("CanvasCtxMenu")) {
        if (ImGui::MenuItem("Add Zone Here")) {
            float w = 0.3f, h = 0.3f;
            float x = std::clamp(normPos.x - w * 0.5f, 0.f, 1.f - w);
            float y = std::clamp(normPos.y - h * 0.5f, 0.f, 1.f - h);
            mSelectedZoneId = mSurfaceMap->addZone("", x, y, w, h);
            ToastSystem::instance().toast("Zone added");
        }
        ImGui::EndPopup();
    }

    // Left-click: check resize handles first, then select zone or start drag.
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // Check resize handles for the selected zone.
            bool startedResize = false;
            if (mSelectedZoneId >= 0) {
                auto* selZone = mSurfaceMap->getZone(mSelectedZoneId);
                if (selZone) {
                    float hx[8], hy[8];
                    float zx = selZone->x, zy = selZone->y;
                    float zw = selZone->width, zh = selZone->height;
                    hx[0] = zx;        hy[0] = zy;           // TL
                    hx[1] = zx+zw*0.5f; hy[1] = zy;          // T
                    hx[2] = zx+zw;     hy[2] = zy;           // TR
                    hx[3] = zx+zw;     hy[3] = zy+zh*0.5f;   // R
                    hx[4] = zx+zw;     hy[4] = zy+zh;        // BR
                    hx[5] = zx+zw*0.5f; hy[5] = zy+zh;       // B
                    hx[6] = zx;        hy[6] = zy+zh;        // BL
                    hx[7] = zx;        hy[7] = zy+zh*0.5f;   // L

                    float hitRadius = (canvasSize.x > 0) ? 8.0f / canvasSize.x : 0.02f;
                    for (int i = 0; i < 8; ++i) {
                        float ddx = normPos.x - hx[i];
                        float ddy = normPos.y - hy[i];
                        if (ddx*ddx + ddy*ddy < hitRadius*hitRadius) {
                            mResizing     = true;
                            mResizeHandle = i;
                            mDragOriginX  = normPos.x;
                            mDragOriginY  = normPos.y;
                            mDragZoneX    = selZone->x;
                            mDragZoneY    = selZone->y;
                            mDragZoneW    = selZone->width;
                            mDragZoneH    = selZone->height;
                            startedResize = true;
                            break;
                        }
                    }
                }
            }

            if (!startedResize) {
                int hitId = zoneAtPos(normPos.x, normPos.y);
                mSelectedZoneId = hitId;
                if (hitId >= 0) {
                    auto* zone = mSurfaceMap->getZone(hitId);
                    if (zone) {
                        mDragging    = true;
                        mDragOriginX = normPos.x;
                        mDragOriginY = normPos.y;
                        mDragZoneX   = zone->x;
                        mDragZoneY   = zone->y;
                        mDragZoneW   = zone->width;
                        mDragZoneH   = zone->height;
                    }
                }
            }
        }
    }

    // Resize zone via handles.
    constexpr float kMinSize = 0.05f;
    if (mResizing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto* zone = mSelectedZoneId >= 0 ? mSurfaceMap->getZone(mSelectedZoneId) : nullptr;
        if (zone) {
            float dx = normPos.x - mDragOriginX;
            float dy = normPos.y - mDragOriginY;
            float nx = mDragZoneX, ny = mDragZoneY;
            float nw = mDragZoneW, nh = mDragZoneH;

            switch (mResizeHandle) {
                case 0: nx += dx; ny += dy; nw -= dx; nh -= dy; break; // TL
                case 1:           ny += dy;           nh -= dy; break; // T
                case 2:           ny += dy; nw += dx; nh -= dy; break; // TR
                case 3:                     nw += dx;           break; // R
                case 4:                     nw += dx; nh += dy; break; // BR
                case 5:                               nh += dy; break; // B
                case 6: nx += dx;           nw -= dx; nh += dy; break; // BL
                case 7: nx += dx;           nw -= dx;           break; // L
            }

            // Enforce minimum size
            if (nw < kMinSize) { nw = kMinSize; nx = mDragZoneX + mDragZoneW - kMinSize; }
            if (nh < kMinSize) { nh = kMinSize; ny = mDragZoneY + mDragZoneH - kMinSize; }
            // Clamp to canvas bounds [0, 1]
            if (nx < 0.f) { nw += nx; nx = 0.f; }
            if (ny < 0.f) { nh += ny; ny = 0.f; }
            if (nx + nw > 1.f) nw = 1.f - nx;
            if (ny + nh > 1.f) nh = 1.f - ny;
            // Final min size check after clamping
            if (nw >= kMinSize && nh >= kMinSize) {
                zone->x = nx; zone->y = ny;
                zone->width = nw; zone->height = nh;
            }
        }
    }

    // Drag zone (move).
    if (mDragging && !mResizing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto* zone = mSelectedZoneId >= 0 ? mSurfaceMap->getZone(mSelectedZoneId) : nullptr;
        if (zone) {
            float dx = normPos.x - mDragOriginX;
            float dy = normPos.y - mDragOriginY;
            zone->x = std::clamp(mDragZoneX + dx, 0.f, 1.f - zone->width);
            zone->y = std::clamp(mDragZoneY + dy, 0.f, 1.f - zone->height);
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        mDragging = false;
        mResizing = false;
        mResizeHandle = -1;
    }
}

// ── Properties panel ─────────────────────────────────────────────────────────

void SurfaceEditorPanel::renderProperties(StudioEngine* engine) {
    auto* zone = mSurfaceMap ? mSurfaceMap->getZone(mSelectedZoneId) : nullptr;
    if (!zone) return;

    ImGui::PushID(mSelectedZoneId);
    ImGui::TextDisabled("Zone Properties");
    ImGui::Separator();

    // Name.
    char nameBuf[128];
    std::strncpy(nameBuf, zone->name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        zone->name = nameBuf;
    }

    // Position.
    ImGui::SetNextItemWidth(100.f);
    ImGui::DragFloat("X##zx", &zone->x, 0.005f, 0.f, 1.f - zone->width, "%.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    ImGui::DragFloat("Y##zy", &zone->y, 0.005f, 0.f, 1.f - zone->height, "%.3f");

    // Size.
    ImGui::SetNextItemWidth(100.f);
    ImGui::DragFloat("W##zw", &zone->width, 0.005f, 0.05f, 1.f, "%.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    ImGui::DragFloat("H##zh", &zone->height, 0.005f, 0.05f, 1.f, "%.3f");
    zone->x = std::clamp(zone->x, 0.f, 1.f - zone->width);
    zone->y = std::clamp(zone->y, 0.f, 1.f - zone->height);

    ImGui::Separator();

    // Output slot assignment.
    ImGui::TextDisabled("Output:");
    int currentOut = zone->outputSlotId;
    if (ImGui::Button("None##out")) {
        zone->outputSlotId = -1;
    }
    if (mOutputManager) {
        for (const auto& slot : mOutputManager->getAllSlots()) {
            ImGui::SameLine();
            char label[32];
            snprintf(label, sizeof(label), "Out %d##outbtn", slot.id);
            bool selected = (zone->outputSlotId == slot.id);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.f));
            if (ImGui::Button(label)) zone->outputSlotId = slot.id;
            if (selected) ImGui::PopStyleColor();
        }
    }

    ImGui::Separator();

    // Camera section.
    ImGui::TextDisabled("Camera:");
    bool customCam = zone->customCamera;
    if (ImGui::Checkbox("Custom Camera", &customCam)) {
        zone->customCamera = customCam;
        if (customCam && zone->cameraName.empty() && engine && engine->getSceneManager()) {
            mSurfaceMap->createCameraForZone(zone->id, engine->getSceneManager());
        } else if (!customCam && !zone->cameraName.empty() && engine && engine->getSceneManager()) {
            mSurfaceMap->destroyCameraForZone(zone->id, engine->getSceneManager());
        }
    }

    if (zone->customCamera && !zone->cameraName.empty() && engine) {
        auto* sm = engine->getSceneManager();
        if (sm && sm->hasCamera(zone->cameraName)) {
            auto* cam = sm->getCamera(zone->cameraName);
            auto* node = cam->getParentSceneNode();
            // Position via parent scene node.
            if (node) {
                auto pos = node->getPosition();
                float posArr[3] = {pos.x, pos.y, pos.z};
                ImGui::SetNextItemWidth(200.f);
                if (ImGui::DragFloat3("Pos##campos", posArr, 0.05f)) {
                    node->setPosition(posArr[0], posArr[1], posArr[2]);
                }
            }
            // FOV via camera.
            float fovDeg = cam->getFOVy().valueDegrees();
            ImGui::SetNextItemWidth(100.f);
            if (ImGui::DragFloat("FOV##camfov", &fovDeg, 0.5f, 5.f, 170.f, "%.1f°")) {
                cam->setFOVy(Ogre::Degree(fovDeg));
            }
        }
    }

    ImGui::Separator();

    // Warp/Blend overrides.
    ImGui::TextDisabled("Per-Zone Warp:");
    bool warpOn = zone->warpEnabled;
    if (ImGui::Checkbox("Enable##zonewarp", &warpOn)) zone->warpEnabled = warpOn;
    ImGui::SameLine();
    if (ImGui::Button("Reset##zwreset")) zone->warpProfile.reset();

    ImGui::TextDisabled("Per-Zone Blend:");
    bool blendOn = zone->blendEnabled;
    if (ImGui::Checkbox("Enable##zoneblend", &blendOn)) zone->blendEnabled = blendOn;

    ImGui::PopID();
}

} // namespace bbfx

#include "OutputManagerPanel.h"
#include "../StudioApp.h"
#include "../StudioEngine.h"
#include "../OutputManager.h"
#include "../WarpWizard.h"
#include "../TextureShareSender.h"
#include "../GridWarpProfile.h"
#include "../ToastSystem.h"
#include <imgui.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace bbfx {

// Corner colors: TL=red, TR=green, BL=blue, BR=yellow
static const ImVec4 kCornerColors[4] = {
    {1.0f, 0.2f, 0.2f, 1.0f},  // TL — red
    {0.2f, 1.0f, 0.2f, 1.0f},  // TR — green
    {0.2f, 0.4f, 1.0f, 1.0f},  // BL — blue
    {1.0f, 1.0f, 0.2f, 1.0f},  // BR — yellow
};
static const char* kCornerNames[4] = {"TL", "TR", "BL", "BR"};

void OutputManagerPanel::render(StudioEngine* engine, StudioApp* app) {
    ImGui::Begin("Output Manager");

    if (!engine) {
        ImGui::TextDisabled("Engine not available");
        ImGui::End();
        return;
    }

    auto* mgr = engine->getOutputManager();
    if (!mgr) {
        ImGui::TextDisabled("OutputManager not available");
        ImGui::End();
        return;
    }

    const auto& slots = mgr->getAllSlots();

    // ── Header ──────────────────────────────────────────────────────────────
    ImGui::Text("Outputs: %d", static_cast<int>(slots.size()));
    ImGui::SameLine();
    if (ImGui::Button("Add Output")) {
        mgr->addOutput(1920, 1080, engine->getSceneManager());
        ToastSystem::instance().toast("Output added");
    }
    ImGui::SameLine();
    if (ImGui::Button("PANIC Warp")) {
        mgr->resetAllWarps();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reset all warps to identity on all outputs");
    ImGui::SameLine();
    if (ImGui::Button("Auto-Blend")) {
        auto suggestions = mgr->detectAdjacentOutputs();
        for (auto& s : suggestions) {
            auto* sA = mgr->getSlot(s.slotA);
            auto* sB = mgr->getSlot(s.slotB);
            if (s.horizontal) {
                // Determine which is left, which is right
                if (sA && sB) {
                    // Apply blend on the right edge of slotA and left edge of slotB
                    sA->blendProfile.right = s.overlapFraction;
                    sB->blendProfile.left  = s.overlapFraction;
                    if (!sA->blendEnabled) mgr->enableBlend(s.slotA);
                    else                   mgr->updateBlendParams(s.slotA);
                    if (!sB->blendEnabled) mgr->enableBlend(s.slotB);
                    else                   mgr->updateBlendParams(s.slotB);
                }
            } else {
                if (sA && sB) {
                    sA->blendProfile.bottom = s.overlapFraction;
                    sB->blendProfile.top    = s.overlapFraction;
                    if (!sA->blendEnabled) mgr->enableBlend(s.slotA);
                    else                   mgr->updateBlendParams(s.slotA);
                    if (!sB->blendEnabled) mgr->enableBlend(s.slotB);
                    else                   mgr->updateBlendParams(s.slotB);
                }
            }
        }
        if (suggestions.empty()) {
            // No adjacent outputs detected (single monitor setup)
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Auto-detect adjacent outputs and apply edge blend");

    ImGui::Separator();

    // ── Preview strip (horizontal thumbnails) ───────────────────────────────
    if (!slots.empty()) {
        ImGui::TextDisabled("Preview:");
        for (size_t i = 0; i < slots.size(); ++i) {
            const auto& slot = slots[i];
            ImTextureID tex = mgr->getTextureID(slot.id);
            if (tex) {
                float thumbW = 160.0f;
                float thumbH = thumbW * 9.0f / 16.0f;

                // Zone-crop UV: show what the output actually displays
                float uMin = 0.f, vMin = 0.f, uMax = 1.f, vMax = 1.f;
                if (auto* sm = mgr->getSurfaceMap()) {
                    if (slot.zoneId >= 0) {
                        if (const auto* zone = sm->getZone(slot.zoneId)) {
                            uMin = zone->x;
                            uMax = zone->x + zone->width;
                            vMin = 1.f - (zone->y + zone->height);
                            vMax = 1.f - zone->y;
                        }
                    }
                }

                ImVec2 thumbPos = ImGui::GetCursorScreenPos();
                ImGui::Image(tex, {thumbW, thumbH}, {uMin, vMin}, {uMax, vMax});

                // Warp preview overlay on thumbnail
                if (slot.warpEnabled && !slot.warpProfile.isIdentity()) {
                    const float* c = slot.warpProfile.corners;
                    ImVec2 pts[4] = {
                        {thumbPos.x + c[0] * thumbW, thumbPos.y + c[1] * thumbH}, // TL
                        {thumbPos.x + c[2] * thumbW, thumbPos.y + c[3] * thumbH}, // TR
                        {thumbPos.x + c[6] * thumbW, thumbPos.y + c[7] * thumbH}, // BR
                        {thumbPos.x + c[4] * thumbW, thumbPos.y + c[5] * thumbH}, // BL
                    };
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImU32 lineCol = IM_COL32(255, 255, 255, 180);
                    dl->AddLine(pts[0], pts[1], lineCol, 1.5f);
                    dl->AddLine(pts[1], pts[2], lineCol, 1.5f);
                    dl->AddLine(pts[2], pts[3], lineCol, 1.5f);
                    dl->AddLine(pts[3], pts[0], lineCol, 1.5f);
                    for (int k = 0; k < 4; ++k) {
                        ImU32 col = ImGui::ColorConvertFloat4ToU32(kCornerColors[k]);
                        dl->AddCircleFilled(pts[k], 4.0f, col);
                    }
                }

                if (i + 1 < slots.size()) ImGui::SameLine();
            }
        }
        ImGui::Separator();
    }

    // ── Per-output details ──────────────────────────────────────────────────
    std::vector<int> slotIds;
    for (const auto& s : slots) slotIds.push_back(s.id);

    for (int slotId : slotIds) {
        const auto* slotConst = mgr->getSlot(slotId);
        if (!slotConst) continue;

        char header[64];
        snprintf(header, sizeof(header), "Output %d (%ux%u)###out%d",
                 slotConst->id, slotConst->width, slotConst->height, slotConst->id);

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID(slotId);

            // ── Resolution ──────────────────────────────────────────────
            ImGui::TextDisabled("Resolution:");
            if (ImGui::Button("720p"))  mgr->setResolution(slotId, 1280, 720, engine->getSceneManager());
            ImGui::SameLine();
            if (ImGui::Button("1080p")) mgr->setResolution(slotId, 1920, 1080, engine->getSceneManager());
            ImGui::SameLine();
            if (ImGui::Button("4K"))    mgr->setResolution(slotId, 3840, 2160, engine->getSceneManager());

            // ── Monitor ─────────────────────────────────────────────────
            ImGui::TextDisabled("Monitor:");
            int displayCount = 0;
            auto* displays = SDL_GetDisplays(&displayCount);
            if (displays && displayCount > 0) {
                for (int i = 0; i < displayCount; ++i) {
                    const char* name = SDL_GetDisplayName(displays[i]);
                    SDL_Rect bounds;
                    SDL_GetDisplayBounds(displays[i], &bounds);
                    char label[128];
                    snprintf(label, sizeof(label), "%d: %s (%dx%d)###mon%d_%d",
                             i + 1, name ? name : "Display", bounds.w, bounds.h, slotId, i);
                    if (ImGui::Button(label)) {
                        mgr->setMonitor(slotId, i);
                    }
                    if (i + 1 < displayCount) ImGui::SameLine();
                }
                SDL_free(displays);
            }

            // ── Fullscreen ──────────────────────────────────────────────
            if (ImGui::Button("Fullscreen")) {
                mgr->toggleFullscreen(slotId);
            }

            // ── Remove ──────────────────────────────────────────────────
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("Remove")) {
                mgr->removeOutput(slotId);
                ToastSystem::instance().toast("Output removed");
            }
            ImGui::PopStyleColor();

            ImGui::Separator();

            // ── Warp Section (v3.4) ─────────────────────────────────────
            auto* slot = mgr->getSlot(slotId);
            if (!slot) { ImGui::PopID(); continue; }

            if (ImGui::CollapsingHeader("Warp###warp")) {
                // ── Calibration wizard ───────────────────────────────────
                WarpWizard* wizard = app ? &app->getWarpWizard() : nullptr;
                bool wizardActiveForThis = wizard && wizard->isActive() &&
                                           wizard->getOutputSlotId() == slotId;

                if (wizard) {
                    if (wizardActiveForThis) {
                        // Show Cancel button in red.
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                        if (ImGui::Button("Cancel Calibration")) {
                            wizard->cancel();
                        }
                        ImGui::PopStyleColor();

                        // Overlay: instruction text + progress.
                        ImGui::Spacing();
                        ImGui::TextColored({1.0f, 1.0f, 0.2f, 1.0f}, "%s",
                                           wizard->getInstructionText().c_str());
                        ImGui::Text("Clicks: %d / 4", wizard->getNumClicked());

                        // Show clicked points (green dots label).
                        const float* clicks = wizard->getClickedPoints();
                        for (int ci = 0; ci < wizard->getNumClicked(); ++ci) {
                            ImGui::TextColored({0.2f, 1.0f, 0.2f, 1.0f},
                                "  [%d] (%.3f, %.3f)", ci + 1,
                                clicks[ci * 2], clicks[ci * 2 + 1]);
                        }
                        ImGui::Spacing();
                    } else if (!wizard->isActive()) {
                        // Calibrate button — only show if no wizard is running.
                        if (ImGui::Button("Calibrate (4-click wizard)")) {
                            wizard->start(slotId, mgr);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip(
                                "Opens a test pattern on the output window.\n"
                                "Click the 4 corners of your projection surface in order:\n"
                                "  TL → TR → BL → BR");
                        ImGui::SameLine();
                    }
                }

                // Enable / disable toggle
                bool warpOn = slot->warpEnabled;
                if (ImGui::Checkbox("Enable Warp", &warpOn)) {
                    if (warpOn) mgr->enableWarp(slotId);
                    else        mgr->disableWarp(slotId);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Warp")) {
                    slot->warpProfile.reset();
                    if (slot->warpEnabled) mgr->updateWarpParams(slotId);
                }

                if (!slot->warpEnabled) {
                    ImGui::TextDisabled("(Enable warp to edit corners)");
                } else {
                    // Canvas with draggable corner handles
                    ImVec2 canvasSize(320.0f, 180.0f);
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

                    // Background preview — zone-cropped
                    ImTextureID tex = mgr->getTextureID(slotId);
                    if (tex) {
                        float uMin = 0.f, vMin = 0.f, uMax = 1.f, vMax = 1.f;
                        if (auto* sm = mgr->getSurfaceMap()) {
                            if (slot->zoneId >= 0) {
                                if (const auto* zone = sm->getZone(slot->zoneId)) {
                                    uMin = zone->x;
                                    uMax = zone->x + zone->width;
                                    vMin = 1.f - (zone->y + zone->height);
                                    vMax = 1.f - zone->y;
                                }
                            }
                        }
                        ImGui::Image(tex, canvasSize, {uMin, vMin}, {uMax, vMax});
                    } else {
                        ImGui::Dummy(canvasSize);
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddRectFilled(canvasPos,
                            {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                            IM_COL32(40, 40, 40, 255));
                    }

                    float* c = slot->warpProfile.corners;
                    // Corner order: TL(0,1) TR(2,3) BL(4,5) BR(6,7)
                    float* cornerPtrs[4][2] = {
                        {&c[0], &c[1]}, {&c[2], &c[3]},
                        {&c[4], &c[5]}, {&c[6], &c[7]}
                    };

                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    // Draw quad lines
                    auto toCanvas = [&](float nx, float ny) -> ImVec2 {
                        return {canvasPos.x + nx * canvasSize.x,
                                canvasPos.y + ny * canvasSize.y};
                    };
                    ImVec2 ptsTL = toCanvas(c[0], c[1]);
                    ImVec2 ptsTR = toCanvas(c[2], c[3]);
                    ImVec2 ptsBL = toCanvas(c[4], c[5]);
                    ImVec2 ptsBR = toCanvas(c[6], c[7]);
                    ImU32 lineCol = IM_COL32(255, 255, 255, 200);
                    dl->AddLine(ptsTL, ptsTR, lineCol, 1.5f);
                    dl->AddLine(ptsTR, ptsBR, lineCol, 1.5f);
                    dl->AddLine(ptsBR, ptsBL, lineCol, 1.5f);
                    dl->AddLine(ptsBL, ptsTL, lineCol, 1.5f);

                    // Draggable handles for each corner
                    for (int k = 0; k < 4; ++k) {
                        float* cx = cornerPtrs[k][0];
                        float* cy = cornerPtrs[k][1];
                        ImVec2 handlePos = toCanvas(*cx, *cy);
                        ImU32 col = ImGui::ColorConvertFloat4ToU32(kCornerColors[k]);

                        // Draw handle circle
                        dl->AddCircleFilled(handlePos, 8.0f, col);
                        dl->AddCircle(handlePos, 8.0f, IM_COL32(255, 255, 255, 200), 12, 1.5f);

                        // Invisible drag button
                        char btnId[16];
                        snprintf(btnId, sizeof(btnId), "##h%d_%d", slotId, k);
                        ImGui::SetCursorScreenPos({handlePos.x - 10.0f, handlePos.y - 10.0f});
                        ImGui::InvisibleButton(btnId, {20.0f, 20.0f});

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("%s (%.3f, %.3f) — drag to move",
                                              kCornerNames[k], *cx, *cy);
                        }
                        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.5f)) {
                            ImVec2 delta = ImGui::GetIO().MouseDelta;
                            *cx = std::clamp(*cx + delta.x / canvasSize.x, 0.0f, 1.0f);
                            *cy = std::clamp(*cy + delta.y / canvasSize.y, 0.0f, 1.0f);
                            mgr->updateWarpParams(slotId);
                        }
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                            // Double-click: reset this corner to identity
                            static const float kId[8] = {0.f,0.f, 1.f,0.f, 0.f,1.f, 1.f,1.f};
                            *cx = kId[k * 2];
                            *cy = kId[k * 2 + 1];
                            mgr->updateWarpParams(slotId);
                        }
                    }

                    // Reset cursor after canvas
                    ImGui::SetCursorScreenPos({canvasPos.x, canvasPos.y + canvasSize.y + 4.0f});

                    // Numeric precision controls
                    if (ImGui::TreeNode("Advanced (numeric)")) {
                        const char* labels[4] = {"TL", "TR", "BL", "BR"};
                        bool changed = false;
                        for (int k = 0; k < 4; ++k) {
                            ImGui::PushStyleColor(ImGuiCol_Text,
                                ImGui::ColorConvertFloat4ToU32(kCornerColors[k]));
                            ImGui::Text("%s", labels[k]);
                            ImGui::PopStyleColor();
                            ImGui::SameLine();
                            char xid[32], yid[32];
                            snprintf(xid, sizeof(xid), "X##c%d%d", slotId, k);
                            snprintf(yid, sizeof(yid), "Y##c%d%d", slotId, k);
                            ImGui::SetNextItemWidth(80.0f);
                            if (ImGui::DragFloat(xid, cornerPtrs[k][0], 0.001f, 0.0f, 1.0f, "%.3f"))
                                changed = true;
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(80.0f);
                            if (ImGui::DragFloat(yid, cornerPtrs[k][1], 0.001f, 0.0f, 1.0f, "%.3f"))
                                changed = true;
                        }
                        if (changed) mgr->updateWarpParams(slotId);
                        ImGui::TreePop();
                    }
                }
            }

            // ── Edge Blend Section (v3.4) ───────────────────────────────
            if (ImGui::CollapsingHeader("Edge Blend###blend")) {
                bool blendOn = slot->blendEnabled;
                if (ImGui::Checkbox("Enable Blend", &blendOn)) {
                    if (blendOn) mgr->enableBlend(slotId);
                    else         mgr->disableBlend(slotId);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Blend")) {
                    slot->blendProfile.reset();
                    if (slot->blendEnabled) mgr->updateBlendParams(slotId);
                }

                if (slot->blendEnabled || slot->blendProfile.isActive()) {
                    bool changed = false;
                    ImGui::SetNextItemWidth(200.0f);
                    if (ImGui::SliderFloat("Left##bl",   &slot->blendProfile.left,   0.0f, 0.5f, "%.3f")) changed = true;
                    ImGui::SetNextItemWidth(200.0f);
                    if (ImGui::SliderFloat("Right##br",  &slot->blendProfile.right,  0.0f, 0.5f, "%.3f")) changed = true;
                    ImGui::SetNextItemWidth(200.0f);
                    if (ImGui::SliderFloat("Top##bt",    &slot->blendProfile.top,    0.0f, 0.5f, "%.3f")) changed = true;
                    ImGui::SetNextItemWidth(200.0f);
                    if (ImGui::SliderFloat("Bottom##bb", &slot->blendProfile.bottom, 0.0f, 0.5f, "%.3f")) changed = true;
                    ImGui::SetNextItemWidth(200.0f);
                    if (ImGui::SliderFloat("Gamma##bg",  &slot->blendProfile.gamma,  1.0f, 3.0f, "%.2f")) changed = true;
                    if (changed && slot->blendEnabled) mgr->updateBlendParams(slotId);
                } else {
                    ImGui::TextDisabled("(Enable blend to configure edges)");
                }
            }

            // ── Grid Warp Section (v3.4 Lot K) ─────────────────────────────
            if (ImGui::CollapsingHeader("Grid Warp (4x4)###gridwarp")) {
                bool gwOn = slot->gridWarpEnabled;
                if (ImGui::Checkbox("Enable Grid Warp##gwen", &gwOn)) {
                    if (gwOn) mgr->enableGridWarp(slotId);
                    else      mgr->disableGridWarp(slotId);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Grid##gwreset")) {
                    slot->gridWarpProfile.reset();
                    if (slot->gridWarpEnabled) mgr->updateGridWarpParams(slotId);
                }

                if (!slot->gridWarpEnabled) {
                    ImGui::TextDisabled("(Enable grid warp to edit control points)");
                } else {
                    // Canvas: show 4x4 grid of draggable handles on top of the preview
                    ImVec2 canvasSize(320.0f, 180.0f);
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

                    // Background preview — zone-cropped
                    ImTextureID tex = mgr->getTextureID(slotId);
                    if (tex) {
                        float uMin = 0.f, vMin = 0.f, uMax = 1.f, vMax = 1.f;
                        if (auto* sm = mgr->getSurfaceMap()) {
                            if (slot->zoneId >= 0) {
                                if (const auto* zone = sm->getZone(slot->zoneId)) {
                                    uMin = zone->x;
                                    uMax = zone->x + zone->width;
                                    vMin = 1.f - (zone->y + zone->height);
                                    vMax = 1.f - zone->y;
                                }
                            }
                        }
                        ImGui::Image(tex, canvasSize, {uMin, vMin}, {uMax, vMax});
                    } else {
                        ImGui::Dummy(canvasSize);
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddRectFilled(canvasPos,
                            {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                            IM_COL32(30, 30, 30, 255));
                    }

                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    auto& gw = slot->gridWarpProfile;
                    constexpr int N = GridWarpProfile::N;

                    auto toCanvas = [&](float nx, float ny) -> ImVec2 {
                        return {canvasPos.x + nx * canvasSize.x,
                                canvasPos.y + ny * canvasSize.y};
                    };

                    // Draw grid lines (horizontal and vertical)
                    ImU32 lineCol = IM_COL32(180, 180, 180, 120);
                    for (int r = 0; r < N; ++r) {
                        for (int c = 0; c < N - 1; ++c) {
                            dl->AddLine(
                                toCanvas(gw.getX(r, c),   gw.getY(r, c)),
                                toCanvas(gw.getX(r, c+1), gw.getY(r, c+1)),
                                lineCol, 1.0f);
                        }
                    }
                    for (int c = 0; c < N; ++c) {
                        for (int r = 0; r < N - 1; ++r) {
                            dl->AddLine(
                                toCanvas(gw.getX(r,   c), gw.getY(r,   c)),
                                toCanvas(gw.getX(r+1, c), gw.getY(r+1, c)),
                                lineCol, 1.0f);
                        }
                    }

                    // Draggable handles for each control point
                    bool gwChanged = false;
                    for (int r = 0; r < N; ++r) {
                        for (int c = 0; c < N; ++c) {
                            float px = gw.getX(r, c);
                            float py = gw.getY(r, c);
                            ImVec2 hPos = toCanvas(px, py);

                            // Color: corners=yellow, edges=cyan, interior=white
                            bool isCorner = (r == 0 || r == N-1) && (c == 0 || c == N-1);
                            bool isEdge   = !isCorner && (r == 0 || r == N-1 || c == 0 || c == N-1);
                            ImU32 hCol = isCorner ? IM_COL32(255,200,0,255)
                                       : isEdge   ? IM_COL32(0,200,255,255)
                                                  : IM_COL32(255,255,255,200);
                            dl->AddCircleFilled(hPos, 5.0f, hCol);
                            dl->AddCircle(hPos, 5.0f, IM_COL32(255,255,255,180), 8, 1.0f);

                            char btnId[24];
                            snprintf(btnId, sizeof(btnId), "##gw%d_%d_%d", slotId, r, c);
                            ImGui::SetCursorScreenPos({hPos.x - 7.0f, hPos.y - 7.0f});
                            ImGui::InvisibleButton(btnId, {14.0f, 14.0f});

                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("(%d,%d) src=(%.3f,%.3f)", r, c, px, py);
                            }
                            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.5f)) {
                                ImVec2 delta = ImGui::GetIO().MouseDelta;
                                float nx = std::clamp(px + delta.x / canvasSize.x, 0.0f, 1.0f);
                                float ny = std::clamp(py + delta.y / canvasSize.y, 0.0f, 1.0f);
                                gw.setPoint(r, c, nx, ny);
                                gwChanged = true;
                            }
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                                // Reset to identity
                                gw.setPoint(r, c,
                                    static_cast<float>(c) / static_cast<float>(N - 1),
                                    static_cast<float>(r) / static_cast<float>(N - 1));
                                gwChanged = true;
                            }
                        }
                    }
                    if (gwChanged) mgr->updateGridWarpParams(slotId);

                    ImGui::SetCursorScreenPos({canvasPos.x, canvasPos.y + canvasSize.y + 4.0f});
                    ImGui::TextDisabled("Drag handles to warp | Double-click to reset point");
                }
            }

            // ── Texture Sharing Output (v3.4 Lot H + Lot N cross-platform) ──
            {
                char sectionLabel[64];
                snprintf(sectionLabel, sizeof(sectionLabel), "%s Output###texshare", getTextureShareLabel());
                if (ImGui::CollapsingHeader(sectionLabel)) {
                    if (!isTextureShareAvailable()) {
                        ImGui::TextDisabled("%s not available.", getTextureShareLabel());
                        ImGui::TextDisabled("Enable output will be a no-op.");
                    }
                    bool texShareOn = slot->textureShareEnabled;
                    if (ImGui::Checkbox("Enable##txs", &texShareOn)) {
                        if (texShareOn) {
                            mgr->enableTextureShare(slotId, slot->textureShareSourceName);
                            ToastSystem::instance().toast(std::string(getTextureShareLabel()) + " enabled on output " + std::to_string(slotId));
                        } else {
                            mgr->disableTextureShare(slotId);
                            ToastSystem::instance().toast(std::string(getTextureShareLabel()) + " disabled on output " + std::to_string(slotId));
                        }
                    }
                    if (slot->textureShareEnabled) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.2f, 1.f, 0.2f, 1.f), "Active");
                    }
                    char nameBuf[128] = {};
                    std::strncpy(nameBuf, slot->textureShareSourceName.c_str(), sizeof(nameBuf) - 1);
                    ImGui::SetNextItemWidth(220.f);
                    if (ImGui::InputText("Source Name##txsn", nameBuf, sizeof(nameBuf))) {
                        slot->textureShareSourceName = nameBuf;
                        if (slot->textureShareEnabled && slot->textureSender) {
                            slot->textureSender->setName(slot->textureShareSourceName);
                        }
                    }
                }
            }

            ImGui::PopID();
            ImGui::Separator();
        }
    }

    ImGui::End();
}

} // namespace bbfx

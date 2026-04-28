#include "MasterViewPanel.h"
#include "../StudioEngine.h"
#include "../OutputManager.h"
#include "../SurfaceMap.h"
#include "../../network/SyncManager.h"
#include "../../network/SyncProtocol.h"
#include <imgui.h>
#include <numeric>

namespace bbfx {

// ── Public API ────────────────────────────────────────────────────────────────

void MasterViewPanel::render(StudioEngine* engine, SyncManager* sync) {
    ImGui::SetNextWindowSize(ImVec2(420, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Master View")) {
        ImGui::End();
        return;
    }

    OutputManager* outMgr = engine ? engine->getOutputManager() : nullptr;
    renderOutputTiles(outMgr);
    renderNetworkSection(sync);
    renderSceneSection();

    ImGui::End();
}

void MasterViewPanel::setActiveScene(const std::string& chordName, int zoneCount,
                                     float camX, float camY, float camZ, float camFov) {
    mActiveSceneChord     = chordName;
    mActiveSceneZoneCount = zoneCount;
    mActiveSceneCamX      = camX;
    mActiveSceneCamY      = camY;
    mActiveSceneCamZ      = camZ;
    mActiveSceneCamFov    = camFov;
}

void MasterViewPanel::clearActiveScene() {
    mActiveSceneChord.clear();
    mActiveSceneZoneCount = 0;
    mActiveSceneCamX = mActiveSceneCamY = mActiveSceneCamZ = 0.f;
    mActiveSceneCamFov = 60.f;
}

// ── Output tiles ──────────────────────────────────────────────────────────────

void MasterViewPanel::renderOutputTiles(OutputManager* mgr) {
    ImGui::SeparatorText("Outputs");

    if (!mgr || mgr->count() == 0) {
        ImGui::TextDisabled("No outputs — use Output Manager to add");
        return;
    }

    const auto& slots = mgr->getAllSlots();
    ImGui::Text("%d output(s)", static_cast<int>(slots.size()));

    // Responsive tile layout
    const float tileW = 192.f;
    const float tileH = 108.f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int tilesPerRow = std::max(1, static_cast<int>((panelWidth + spacing) / (tileW + spacing)));

    int col = 0;
    for (const auto& slot : slots) {
        if (col > 0) ImGui::SameLine();

        ImGui::BeginGroup();

        // Dim the thumbnail when the output is hidden.
        const bool hidden = !slot.visible;
        if (hidden) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);
        }

        // Thumbnail — zone-cropped to show what the output actually displays
        ImTextureID texId = mgr->getTextureID(slot.id);
        if (texId && !hidden) {
            float uMin = 0.f, vMin = 0.f, uMax = 1.f, vMax = 1.f;
            if (auto* sm = mgr->getSurfaceMap()) {
                if (slot.zoneId >= 0) {
                    if (const auto* zone = sm->getZone(slot.zoneId)) {
                        uMin = zone->x;
                        uMax = zone->x + zone->width;
                        // Flip Y: OGRE top-left origin → GL bottom-left origin
                        vMin = 1.f - (zone->y + zone->height);
                        vMax = 1.f - zone->y;
                    }
                }
            }
            ImGui::Image(texId, ImVec2(tileW, tileH), {uMin, vMin}, {uMax, vMax});
        } else {
            // Hidden or no texture: dark placeholder
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(tileW, tileH));
            if (hidden) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    cursor, ImVec2(cursor.x + tileW, cursor.y + tileH),
                    IM_COL32(30, 30, 30, 200));
            }
        }

        // Overlay: Out N + resolution
        ImVec2 overlayPos = ImGui::GetItemRectMin();
        char label[64];
        snprintf(label, sizeof(label), "Out %d", slot.id);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(overlayPos.x + 4, overlayPos.y + 2),
            IM_COL32(255, 255, 255, hidden ? 100 : 220), label);

        char resLabel[32];
        snprintf(resLabel, sizeof(resLabel), "%ux%u", slot.width, slot.height);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(overlayPos.x + 4, overlayPos.y + 16),
            IM_COL32(200, 200, 200, hidden ? 80 : 200), resLabel);

        // Warp/Blend/Grid badges
        float badgeX = overlayPos.x + tileW - 44;
        float badgeY = overlayPos.y + 2;
        auto drawBadge = [&](const char* ch, bool enabled) {
            ImU32 color = enabled ? IM_COL32(0, 220, 0, hidden ? 80 : 220)
                                  : IM_COL32(120, 120, 120, hidden ? 60 : 150);
            ImGui::GetWindowDrawList()->AddText(ImVec2(badgeX, badgeY), color, ch);
            badgeX += 14;
        };
        drawBadge("W", slot.warpEnabled);
        drawBadge("B", slot.blendEnabled);
        drawBadge("G", slot.gridWarpEnabled);

        if (hidden) {
            ImGui::PopStyleVar(); // Alpha
        }

        // Visibility toggle button
        ImGui::PushID(slot.id);
        const char* toggleLabel = slot.visible ? "Hide" : "Show";
        if (ImGui::SmallButton(toggleLabel)) {
            mgr->setOutputVisible(slot.id, !slot.visible);
        }
        ImGui::PopID();

        // Tooltip on hover (over the thumbnail image/dummy)
        // Check the image rect, not the button
        ImVec2 rectMin = overlayPos;
        ImVec2 rectMax = ImVec2(overlayPos.x + tileW, overlayPos.y + tileH);
        if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
            ImGui::BeginTooltip();
            ImGui::Text("Output %d — %ux%u", slot.id, slot.width, slot.height);
            ImGui::Text("Visible: %s", slot.visible ? "Yes" : "No");
            ImGui::Text("Warp: %s", slot.warpEnabled ? "ON" : "OFF");
            if (slot.warpEnabled) {
                const auto& c = slot.warpProfile.corners;
                ImGui::Text("  TL(%.2f,%.2f) TR(%.2f,%.2f)", c[0], c[1], c[2], c[3]);
                ImGui::Text("  BL(%.2f,%.2f) BR(%.2f,%.2f)", c[4], c[5], c[6], c[7]);
            }
            ImGui::Text("Blend: %s", slot.blendEnabled ? "ON" : "OFF");
            if (slot.blendEnabled) {
                const auto& b = slot.blendProfile;
                ImGui::Text("  L=%.2f R=%.2f T=%.2f B=%.2f G=%.1f",
                    b.left, b.right, b.top, b.bottom, b.gamma);
            }
            ImGui::Text("Grid Warp: %s", slot.gridWarpEnabled ? "ON" : "OFF");
            ImGui::EndTooltip();
        }

        ImGui::EndGroup();

        col++;
        if (col >= tilesPerRow) col = 0;
    }
}

// ── Network section ───────────────────────────────────────────────────────────

void MasterViewPanel::renderNetworkSection(SyncManager* sync) {
    ImGui::SeparatorText("Network");

    if (!sync || !sync->isRunning() || sync->getRole() == SyncRole::STANDALONE) {
        ImGui::TextDisabled("Network: Offline");
        return;
    }

    // Role with colored dot
    SyncRole role = sync->getRole();
    ImU32 roleDotColor;
    const char* roleLabel;
    switch (role) {
        case SyncRole::MASTER: roleDotColor = IM_COL32(0, 220, 0, 255); roleLabel = "Master"; break;
        case SyncRole::SLAVE:  roleDotColor = IM_COL32(60, 120, 255, 255); roleLabel = "Slave"; break;
        default:               roleDotColor = IM_COL32(150, 150, 150, 255); roleLabel = "Standalone"; break;
    }
    ImVec2 dotPos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(dotPos.x + 6, dotPos.y + 8), 5.f, roleDotColor);
    ImGui::Dummy(ImVec2(16, 0));
    ImGui::SameLine();
    ImGui::Text("Role: %s", roleLabel);

    // Peer list
    const auto& peers = sync->getPeers();
    if (peers.empty()) {
        ImGui::TextDisabled("No peers connected");
        return;
    }

    for (int i = 0; i < static_cast<int>(peers.size()); ++i) {
        const auto& peer = peers[i];
        if (!peer.connected) continue;

        // Latency color
        ImU32 latColor;
        if (peer.latencyMs < 50.f)       latColor = IM_COL32(0, 220, 0, 255);
        else if (peer.latencyMs < 100.f)  latColor = IM_COL32(220, 200, 0, 255);
        else                              latColor = IM_COL32(220, 0, 0, 255);

        ImGui::PushStyleColor(ImGuiCol_Text, latColor);
        bool expanded = (mSelectedPeerIdx == i);
        char peerLabel[128];
        snprintf(peerLabel, sizeof(peerLabel), "%s (%.0fms)###peer%d",
            peer.hostname.c_str(), peer.latencyMs, i);

        if (ImGui::Selectable(peerLabel, expanded)) {
            mSelectedPeerIdx = expanded ? -1 : i; // toggle
        }
        ImGui::PopStyleColor();

        // Expanded detail
        if (mSelectedPeerIdx == i) {
            ImGui::Indent();
            ImGui::Text("IP: %s", peer.ip.c_str());
            ImGui::Text("Role: %s", syncRoleName(peer.role));
            ImGui::Text("Latency: %.1f ms", peer.latencyMs);

            // Uptime
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - peer.lastSeen);
            ImGui::Text("Last seen: %lld s ago", static_cast<long long>(uptime.count()));

            // Record latency for graph
            auto* hist = findOrCreateHistory(peer.ip);
            hist->samples.push_back(peer.latencyMs);
            if (static_cast<int>(hist->samples.size()) > LatencyHistory::kMaxSamples)
                hist->samples.pop_front();

            // Mini latency graph
            if (!hist->samples.empty()) {
                std::vector<float> buf(hist->samples.begin(), hist->samples.end());
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.0f ms", peer.latencyMs);
                ImGui::PlotLines("##latency", buf.data(),
                    static_cast<int>(buf.size()), 0, overlay, 0.f, 200.f, ImVec2(200, 40));
            }

            // Disconnect button (master only)
            if (role == SyncRole::MASTER) {
                if (ImGui::SmallButton("Disconnect")) {
                    sync->removePeer(peer.ip);
                    mSelectedPeerIdx = -1;
                }
            }

            ImGui::Unindent();
        }
    }
}

// ── Scene section ─────────────────────────────────────────────────────────────

void MasterViewPanel::renderSceneSection() {
    ImGui::SeparatorText("Active Scene");

    if (mActiveSceneChord.empty()) {
        ImGui::TextDisabled("No scene");
    } else {
        ImGui::Text("Chord: %s", mActiveSceneChord.c_str());
        ImGui::Text("Zones: %d", mActiveSceneZoneCount);
        ImGui::Text("Camera: (%.1f, %.1f, %.1f)  FOV %.0f",
                     mActiveSceneCamX, mActiveSceneCamY, mActiveSceneCamZ, mActiveSceneCamFov);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

MasterViewPanel::LatencyHistory* MasterViewPanel::findOrCreateHistory(const std::string& ip) {
    for (auto& h : mLatencyHistories)
        if (h.ip == ip) return &h;
    mLatencyHistories.push_back({ip, {}});
    return &mLatencyHistories.back();
}

} // namespace bbfx

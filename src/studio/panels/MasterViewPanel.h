#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <deque>

namespace bbfx {

class StudioEngine;
class OutputManager;
class SyncManager;
struct PeerInfo;

/// Unified dashboard panel showing all output thumbnails, network peer
/// status & latency, and active scene indicator.  Read-only — never
/// modifies engine or network state.  (v3.4 Lot N — EPIC-158)
class MasterViewPanel {
public:
    MasterViewPanel() = default;

    void render(StudioEngine* engine, SyncManager* sync);

    /// Set the active scene name (called from StudioApp render loop / Lot P).
    void setActiveScene(const std::string& chordName, int zoneCount,
                        float camX = 0.f, float camY = 0.f, float camZ = 0.f, float camFov = 60.f);
    void clearActiveScene();

private:
    void renderOutputTiles(OutputManager* mgr);
    void renderNetworkSection(SyncManager* sync);
    void renderSceneSection();

    // ── State ────────────────────────────────────────────────────────────────
    int mSelectedPeerIdx = -1;

    // Latency history per peer (mini graph)
    struct LatencyHistory {
        std::string ip;
        std::deque<float> samples;
        static constexpr int kMaxSamples = 60;
    };
    std::vector<LatencyHistory> mLatencyHistories;
    LatencyHistory* findOrCreateHistory(const std::string& ip);

    // Active scene info (set by Lot P integration)
    std::string mActiveSceneChord;
    int         mActiveSceneZoneCount = 0;
    float       mActiveSceneCamX = 0.f, mActiveSceneCamY = 0.f, mActiveSceneCamZ = 0.f;
    float       mActiveSceneCamFov = 60.f;
};

} // namespace bbfx

#pragma once

#include "../../network/SyncManager.h"
#include "../../network/SyncProtocol.h"
#include <string>
#include <vector>
#include <deque>
#include <chrono>

namespace bbfx {

/// Network synchronisation panel (v3.4 Lot G).
///
/// Displays:
///   - Role selector (Standalone / Master / Slave)
///   - Auto-discovery toggle
///   - Connected peers list (hostname, IP, latency, status)
///   - Peer detail on click (extended info + latency graph)
///   - Connection log (last 100 events with timestamps)
///   - Master controls: Sync Chord, Panic All, Sync All
class NetworkPanel {
public:
    NetworkPanel() = default;

    /// Render the panel. Opens a window if *pOpen is true.
    void render(SyncManager* sync, bool* pOpen);

    /// Push an event to the log (called from SyncManager toast callback).
    void pushLog(const std::string& msg);

    /// Record a latency sample for a peer (by IP).
    void recordLatency(const std::string& ip, float latencyMs);

private:
    void renderToolbar(SyncManager* sync);
    void renderPeerTable(SyncManager* sync);
    void renderPeerDetail(SyncManager* sync);
    void renderLog();

    // ── State ────────────────────────────────────────────────────────────────
    char mChordBuf[128]   = {};
    char mManualIpBuf[64] = {};
    bool mAutoDiscovery   = true;
    int  mSelectedPeerIdx = -1;

    // Connection log
    struct LogEntry {
        std::string text;
        std::chrono::steady_clock::time_point ts;
    };
    std::deque<LogEntry> mLog;
    static constexpr int kMaxLog = 100;

    // Latency history per peer-ip
    struct LatencyHistory {
        std::string ip;
        std::deque<float> samples; // ms
        static constexpr int kMaxSamples = 60;
    };
    std::vector<LatencyHistory> mLatencyHistories;

    LatencyHistory* findOrCreateHistory(const std::string& ip);
};

} // namespace bbfx

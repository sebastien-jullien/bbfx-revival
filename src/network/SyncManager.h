#pragma once

#include "SyncProtocol.h"
#include "UdpServer.h"
#include "TcpServer.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include <functional>

namespace bbfx {

/// Orchestrates multi-machine BBFx synchronisation.
///
/// Roles:
///   STANDALONE — passive discovery listener only.
///   MASTER     — sends UDP discovery beacons + beat sync; hosts TCP command server.
///   SLAVE      — listens for beacons; connects TCP to master; applies received commands.
///
/// All network I/O runs in background threads (via UdpServer/TcpServer).
/// Call poll() once per frame from the main thread to process messages.
class SyncManager {
public:
    SyncManager();
    ~SyncManager();

    // ── Lifecycle ────────────────────────────────────────────────────────────

    void setRole(SyncRole role);
    void start();
    void stop();

    // ── Queries ──────────────────────────────────────────────────────────────

    SyncRole                     getRole()        const { return mRole; }
    const std::vector<PeerInfo>& getPeers()       const { return mPeers; }
    float                        getClockOffset() const { return mClockOffsetMs; }
    bool                         isRunning()      const { return mRunning; }

    // ── Per-frame poll ───────────────────────────────────────────────────────

    /// Drain all pending network messages, send beacons/pings if timers fired.
    void poll();

    // ── Manual peer management ─────────────────────────────────────────────

    /// Add a peer by IP address (for manual connection when auto-discovery is off).
    void addManualPeer(const std::string& ip);
    /// Remove a peer by IP address (disconnect).
    void removePeer(const std::string& ip);

    // ── Master commands (no-op if not MASTER) ────────────────────────────────

    void sendChord(const std::string& chordName);
    void sendSet(const std::string& nodeName, const std::string& portName, float value);
    void sendSnapshot(const std::string& snapshotJson);
    void sendPanic();

    // ── Callbacks (set by StudioApp) ─────────────────────────────────────────

    /// Called when a CHORD command is received (slave only).
    void setOnChord(std::function<void(const std::string&)> fn) { mOnChord = std::move(fn); }
    /// Called when a SET command is received.
    void setOnSet(std::function<void(const std::string&, const std::string&, float)> fn)
        { mOnSet = std::move(fn); }
    /// Called when a SNAPSHOT command is received.
    void setOnSnapshot(std::function<void(const std::string&)> fn) { mOnSnapshot = std::move(fn); }
    /// Called when a PANIC command is received.
    void setOnPanic(std::function<void()> fn) { mOnPanic = std::move(fn); }
    /// Called when beat sync is received (slave only).
    void setOnBeat(std::function<void(float bpm, int beatCount)> fn) { mOnBeat = std::move(fn); }
    /// Called for UI toasts (peer join, master conflict, etc.).
    void setOnToast(std::function<void(const std::string&)> fn) { mOnToast = std::move(fn); }

    // ── Beat push (call from master's main loop) ─────────────────────────────

    void pushBeat(float bpm, int beatCount);

    // ── JSON config ─────────────────────────────────────────────────────────

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j); ///< Restores role only; does not start.

private:
    // ── Network helpers ──────────────────────────────────────────────────────

    void sendBeacon();
    void sendPings();
    void processDiscovery();
    void processBeat();
    void processCommands();
    void updatePeerTimeouts();
    void connectToMaster(const PeerInfo& master);
    void handleCommand(const std::string& text);
    void broadcastCommand(const std::string& cmd);

    PeerInfo* findPeerByIp(const std::string& ip);
    std::string localHostname() const;
    std::string broadcastAddress() const { return "255.255.255.255"; }
    int64_t nowMs() const;

    // ── State ────────────────────────────────────────────────────────────────

    SyncRole mRole = SyncRole::STANDALONE;
    bool     mRunning = false;
    std::vector<PeerInfo> mPeers;

    // Servers
    std::unique_ptr<UdpServer> mDiscoveryServer; ///< Listens for beacons
    std::unique_ptr<UdpServer> mBeatSyncServer;  ///< Listens/sends beat sync
    std::unique_ptr<TcpServer> mCommandServer;   ///< Master: accepts slave connections

    // Timers
    std::chrono::steady_clock::time_point mLastBeaconSent;
    std::chrono::steady_clock::time_point mLastPing;
    std::chrono::steady_clock::time_point mStartTime;

    // Clock sync
    float mClockOffsetMs    = 0.f;
    static constexpr float kAlpha = 0.1f; // EMA alpha for clock offset smoothing

    // Slave state
    bool mConnectedToMaster = false;

    // Callbacks
    std::function<void(const std::string&)>                  mOnChord;
    std::function<void(const std::string&, const std::string&, float)> mOnSet;
    std::function<void(const std::string&)>                  mOnSnapshot;
    std::function<void()>                                    mOnPanic;
    std::function<void(float, int)>                          mOnBeat;
    std::function<void(const std::string&)>                  mOnToast;
};

} // namespace bbfx

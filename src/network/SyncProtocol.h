#pragma once

#include <string>
#include <chrono>

namespace bbfx {

// ── Ports ────────────────────────────────────────────────────────────────────

constexpr int SYNC_DISCOVERY_PORT = 33196; ///< UDP broadcast for peer discovery
constexpr int SYNC_BEAT_PORT      = 33197; ///< UDP unicast for beat sync
constexpr int SYNC_COMMAND_PORT   = 33198; ///< TCP for reliable commands (CHORD/SET/SNAPSHOT/PANIC)

// ── Timing ───────────────────────────────────────────────────────────────────

constexpr int BEACON_INTERVAL_MS = 2000;  ///< Master → broadcast beacon every 2 s
constexpr int PEER_TIMEOUT_MS    = 10000; ///< Mark peer disconnected after 10 s silence
constexpr int PING_INTERVAL_MS   = 2000;  ///< Master → TCP PING every 2 s for latency

// ── Roles ────────────────────────────────────────────────────────────────────

enum class SyncRole {
    STANDALONE, ///< No sync, passive discovery only
    MASTER,     ///< Sends beacons + beat sync + commands
    SLAVE       ///< Receives beacons + beat sync + commands from master
};

inline const char* syncRoleName(SyncRole r) {
    switch (r) {
        case SyncRole::STANDALONE: return "standalone";
        case SyncRole::MASTER:     return "master";
        case SyncRole::SLAVE:      return "slave";
        default:                   return "standalone";
    }
}

inline SyncRole syncRoleFromString(const std::string& s) {
    if (s == "master") return SyncRole::MASTER;
    if (s == "slave")  return SyncRole::SLAVE;
    return SyncRole::STANDALONE;
}

// ── Peer information ──────────────────────────────────────────────────────────

struct PeerInfo {
    std::string hostname;
    std::string ip;
    int         port      = SYNC_DISCOVERY_PORT;
    SyncRole    role      = SyncRole::STANDALONE;
    float       latencyMs = 0.f;
    std::chrono::steady_clock::time_point lastSeen;
    bool        connected = false;
    int64_t     startTimeMs = 0; ///< Peer startup time (ms since epoch, for master conflict)
    int         tcpClientId = -1; ///< TCP client id when slave is connected (-1 = not connected)
};

// ── OSC addresses for sync protocol ─────────────────────────────────────────

constexpr const char* OSC_BEACON  = "/bbfx/sync/beacon";
constexpr const char* OSC_BEAT    = "/bbfx/sync/beat";

// ── TCP command prefixes ─────────────────────────────────────────────────────

constexpr const char* CMD_CHORD    = "CHORD ";
constexpr const char* CMD_SET      = "SET ";
constexpr const char* CMD_SNAPSHOT = "SNAPSHOT ";
constexpr const char* CMD_PANIC    = "PANIC";
constexpr const char* CMD_PING     = "PING";
constexpr const char* CMD_PONG     = "PONG";

} // namespace bbfx

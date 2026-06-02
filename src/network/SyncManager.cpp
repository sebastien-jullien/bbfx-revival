#include "SyncManager.h"
#include "../core/Version.h"   // v3.5.2 Sprint S8 Lot AU — BBFX_VERSION_STRING
#include "../osc/OscMessage.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace bbfx {

// ── Construction ─────────────────────────────────────────────────────────────

SyncManager::SyncManager() {
    mStartTime = std::chrono::steady_clock::now();
}

SyncManager::~SyncManager() {
    stop();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void SyncManager::setRole(SyncRole role) {
    if (role == mRole) return;
    bool wasRunning = mRunning;
    if (wasRunning) stop();
    mRole = role;
    if (wasRunning) start();
}

void SyncManager::start() {
    if (mRunning) return;

    std::cout << "[SyncManager] Starting as " << syncRoleName(mRole) << std::endl;
    mRunning = true;
    mPeers.clear();
    mClockOffsetMs = 0.f;
    mConnectedToMaster = false;

    // Discovery listener — all roles listen for beacons.
    mDiscoveryServer = std::make_unique<UdpServer>();
    if (!mDiscoveryServer->start(SYNC_DISCOVERY_PORT)) {
        std::cerr << "[SyncManager] Failed to bind discovery port " << SYNC_DISCOVERY_PORT << std::endl;
        mDiscoveryServer.reset();
    }

    // Beat sync server — all roles listen.
    mBeatSyncServer = std::make_unique<UdpServer>();
    if (!mBeatSyncServer->start(SYNC_BEAT_PORT)) {
        std::cerr << "[SyncManager] Failed to bind beat port " << SYNC_BEAT_PORT << std::endl;
        mBeatSyncServer.reset();
    }

    // TCP command server — master only.
    if (mRole == SyncRole::MASTER) {
        mCommandServer = std::make_unique<TcpServer>(SYNC_COMMAND_PORT, 16);
        mCommandServer->start();
        std::cout << "[SyncManager] TCP command server on port " << SYNC_COMMAND_PORT << std::endl;
    }

    mLastBeaconSent = std::chrono::steady_clock::now() - std::chrono::milliseconds(BEACON_INTERVAL_MS);
    mLastPing       = std::chrono::steady_clock::now();
}

void SyncManager::stop() {
    if (!mRunning) return;
    std::cout << "[SyncManager] Stopping." << std::endl;
    mRunning = false;

    if (mDiscoveryServer) { mDiscoveryServer->stop(); mDiscoveryServer.reset(); }
    if (mBeatSyncServer)  { mBeatSyncServer->stop();  mBeatSyncServer.reset(); }
    if (mCommandServer)   { mCommandServer->stop();   mCommandServer.reset(); }

    mPeers.clear();
    mConnectedToMaster = false;
}

// ── Per-frame poll ────────────────────────────────────────────────────────────

void SyncManager::poll() {
    if (!mRunning) return;

    processDiscovery();
    processBeat();
    processCommands();
    updatePeerTimeouts();

    auto now = std::chrono::steady_clock::now();

    // Send beacons (master only).
    if (mRole == SyncRole::MASTER) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - mLastBeaconSent).count();
        if (elapsed >= BEACON_INTERVAL_MS) {
            sendBeacon();
            mLastBeaconSent = now;
        }

        // Send pings for latency.
        auto pingElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - mLastPing).count();
        if (pingElapsed >= PING_INTERVAL_MS) {
            sendPings();
            mLastPing = now;
        }
    }
}

// ── Discovery ─────────────────────────────────────────────────────────────────

void SyncManager::sendBeacon() {
    OscMessage msg;
    msg.address = OSC_BEACON;
    msg.args.push_back(std::string(localHostname()));
    msg.args.push_back(static_cast<int32_t>(mRole));
    msg.args.push_back(std::string(BBFX_VERSION_STRING)); // v3.5.2 Sprint S8 Lot AU — was "3.4" hardcoded
    msg.args.push_back(static_cast<int32_t>(nowMs() & 0x7FFFFFFF)); // 31-bit mask: OSC int32 is signed, keep positive

    UdpServer::send(broadcastAddress(), SYNC_DISCOVERY_PORT, msg);
}

void SyncManager::processDiscovery() {
    if (!mDiscoveryServer) return;
    auto msgs = mDiscoveryServer->poll();
    for (auto& msg : msgs) {
        if (msg.address != OSC_BEACON) continue;
        if (msg.args.size() < 3) continue;

        std::string hostname  = msg.getString(0);
        int         roleInt   = msg.getInt(1);
        SyncRole    peerRole  = static_cast<SyncRole>(roleInt);
        int64_t     peerStart = (msg.args.size() >= 4) ? static_cast<int64_t>(msg.getInt(3)) : 0;
        std::string peerIp    = msg.senderIp;

        // Skip our own beacons.
        if (hostname == localHostname() && peerIp == "127.0.0.1") continue;

        // Master-master conflict: older one wins.
        if (mRole == SyncRole::MASTER && peerRole == SyncRole::MASTER) {
            int64_t myStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  mStartTime.time_since_epoch()).count() & 0x7FFFFFFF;
            if (peerStart < myStart) {
                // Remote master is older — we lose.
                std::cout << "[SyncManager] Master conflict: switching to SLAVE." << std::endl;
                if (mOnToast) mOnToast("Another master detected — switching to Slave");
                mRole = SyncRole::SLAVE;
                // Restart with new role.
                stop();
                start();
                return;
            }
        }

        // Update or create peer.
        PeerInfo* peer = findPeerByIp(peerIp);
        if (!peer) {
            PeerInfo p;
            p.hostname = hostname;
            p.ip       = peerIp;
            p.role     = peerRole;
            p.lastSeen = std::chrono::steady_clock::now();
            p.connected = true;
            p.startTimeMs = peerStart;
            mPeers.push_back(p);
            std::cout << "[SyncManager] New peer: " << hostname << " (" << peerIp
                      << ") role=" << syncRoleName(peerRole) << std::endl;
            if (mOnToast) mOnToast("Peer joined: " + hostname + " (" + syncRoleName(peerRole) + ")");

            // Slave: connect TCP to master.
            if (mRole == SyncRole::SLAVE && peerRole == SyncRole::MASTER && !mConnectedToMaster) {
                connectToMaster(mPeers.back());
            }
        } else {
            peer->lastSeen  = std::chrono::steady_clock::now();
            peer->connected = true;
            peer->role      = peerRole;
            peer->hostname  = hostname;
        }
    }
}

// ── Beat sync ─────────────────────────────────────────────────────────────────

void SyncManager::pushBeat(float bpm, int beatCount) {
    if (mRole != SyncRole::MASTER || !mRunning) return;

    OscMessage msg;
    msg.address = OSC_BEAT;
    msg.args.push_back(bpm);
    msg.args.push_back(static_cast<int32_t>(beatCount));
    msg.args.push_back(static_cast<int32_t>(nowMs() & 0x7FFFFFFF)); // 31-bit mask: OSC int32 is signed, keep positive

    // Unicast to each connected peer.
    for (auto& peer : mPeers) {
        if (peer.connected) {
            UdpServer::send(peer.ip, SYNC_BEAT_PORT, msg);
        }
    }
}

void SyncManager::processBeat() {
    if (!mBeatSyncServer || mRole != SyncRole::SLAVE) return;
    auto msgs = mBeatSyncServer->poll();
    for (auto& msg : msgs) {
        if (msg.address != OSC_BEAT) continue;
        if (msg.args.size() < 3) continue;

        float   bpm       = msg.getFloat(0);
        int     beatCount = msg.getInt(1);
        int64_t remoteMs  = static_cast<int64_t>(msg.getInt(2));
        int64_t localMs   = nowMs() & 0x7FFFFFFF; // 31-bit mask: OSC int32 is signed, keep positive

        // EMA clock offset.
        float rawOffset   = static_cast<float>(localMs - remoteMs);
        mClockOffsetMs    = (1.f - kAlpha) * mClockOffsetMs + kAlpha * rawOffset;

        if (mOnBeat) mOnBeat(bpm, beatCount);
    }
}

// ── TCP commands ──────────────────────────────────────────────────────────────

void SyncManager::processCommands() {
    if (!mCommandServer) return;
    auto msgs = mCommandServer->poll();
    for (auto& m : msgs) {
        // Update latency from PONG — extract echoed timestamp and compute RTT.
        if (m.text.rfind(CMD_PONG, 0) == 0) {
            PeerInfo* peer = nullptr;
            for (auto& p : mPeers) {
                if (p.tcpClientId == m.clientId) { peer = &p; break; }
            }
            std::string tsStr = m.text.substr(std::strlen(CMD_PONG));
            if (!tsStr.empty() && peer) {
                try {
                    int64_t sentMs   = std::stoll(tsStr);
                    int64_t localNow = nowMs() & 0x7FFFFFFF; // 31-bit positive range for OSC int32 compat
                    float rtt = static_cast<float>(localNow - sentMs);
                    if (rtt >= 0.f) peer->latencyMs = rtt * 0.5f; // one-way estimate
                } catch (const std::exception& e) {
                    std::cerr << "[SyncManager] Pong timestamp parse error: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[SyncManager] Pong timestamp parse error (unknown)" << std::endl;
                }
            }
            continue;
        }
        handleCommand(m.text);
        // Track which slave is connected.
        bool found = false;
        for (auto& p : mPeers) { if (p.tcpClientId == m.clientId) { found = true; break; } }
        if (!found && !mPeers.empty()) {
            // Associate latest peer with this client id.
            mPeers.back().tcpClientId = m.clientId;
        }
    }
}

void SyncManager::handleCommand(const std::string& text) {
    if (text == CMD_PANIC) {
        std::cout << "[SyncManager] PANIC received" << std::endl;
        if (mOnPanic) mOnPanic();
    } else if (text.rfind(CMD_CHORD, 0) == 0) {
        std::string name = text.substr(std::strlen(CMD_CHORD));
        std::cout << "[SyncManager] CHORD \"" << name << "\" received" << std::endl;
        if (mOnChord) mOnChord(name);
    } else if (text.rfind(CMD_SET, 0) == 0) {
        // "SET nodeName.portName value"
        std::string rest = text.substr(std::strlen(CMD_SET));
        std::istringstream iss(rest);
        std::string dotted; float value = 0.f;
        if (iss >> dotted >> value) {
            auto dot = dotted.rfind('.');
            if (dot != std::string::npos) {
                std::string node = dotted.substr(0, dot);
                std::string port = dotted.substr(dot + 1);
                std::cout << "[SyncManager] SET " << node << "." << port << " = " << value << std::endl;
                if (mOnSet) mOnSet(node, port, value);
            }
        }
    } else if (text.rfind(CMD_SNAPSHOT, 0) == 0) {
        std::string json = text.substr(std::strlen(CMD_SNAPSHOT));
        std::cout << "[SyncManager] SNAPSHOT received (" << json.size() << " bytes)" << std::endl;
        if (mOnSnapshot) mOnSnapshot(json);
    } else if (text.rfind(CMD_PING, 0) == 0) {
        // Slave replies PONG with the echoed master timestamp for RTT calculation.
        std::string ts = text.substr(std::strlen(CMD_PING));
        if (mCommandServer) {
            // Reply to all connected clients (slave has exactly 1 = the master).
            for (const auto& peer : mPeers) {
                if (peer.tcpClientId >= 1)
                    mCommandServer->send(peer.tcpClientId, std::string(CMD_PONG) + " " + ts + "\n");
            }
        }
    }
}

void SyncManager::broadcastCommand(const std::string& cmd) {
    if (!mCommandServer || mRole != SyncRole::MASTER) return;
    for (const auto& peer : mPeers) {
        if (peer.tcpClientId >= 1) {
            mCommandServer->send(peer.tcpClientId, cmd + "\n");
        }
    }
    std::cout << "[SyncManager] Broadcast: " << cmd << std::endl;
}

void SyncManager::sendPings() {
    if (!mCommandServer) return;
    // PING includes master timestamp so PONG can echo it for RTT calculation.
    auto ts = std::to_string(nowMs() & 0x7FFFFFFF); // 31-bit positive range for OSC int32 compat
    for (const auto& peer : mPeers) {
        if (peer.tcpClientId >= 1) {
            mCommandServer->send(peer.tcpClientId, std::string(CMD_PING) + " " + ts + "\n");
        }
    }
}

// ── Master command API ────────────────────────────────────────────────────────

void SyncManager::sendChord(const std::string& name) {
    broadcastCommand(std::string(CMD_CHORD) + name);
}

void SyncManager::sendSet(const std::string& node, const std::string& port, float value) {
    std::ostringstream oss;
    oss << CMD_SET << node << "." << port << " " << value;
    broadcastCommand(oss.str());
}

void SyncManager::sendSnapshot(const std::string& json) {
    broadcastCommand(std::string(CMD_SNAPSHOT) + json);
}

void SyncManager::sendPanic() {
    broadcastCommand(CMD_PANIC);
}

// ── Slave → master TCP connection ────────────────────────────────────────────

void SyncManager::connectToMaster(const PeerInfo& master) {
    // Slave starts a TcpServer so master can connect back via TCP.
    if (!mCommandServer) {
        mCommandServer = std::make_unique<TcpServer>(SYNC_COMMAND_PORT, 1);
        mCommandServer->start();
        if (mCommandServer->isRunning()) {
            mConnectedToMaster = true;
            std::cout << "[SyncManager] Slave: TCP server started on port "
                      << SYNC_COMMAND_PORT << " for master " << master.ip << std::endl;
        } else {
            std::cerr << "[SyncManager] Slave: Failed to start TCP server on port "
                      << SYNC_COMMAND_PORT << std::endl;
            mCommandServer.reset();
        }
    }
}

// ── Manual peer management ───────────────────────────────────────────────────

void SyncManager::addManualPeer(const std::string& ip) {
    if (findPeerByIp(ip)) {
        std::cout << "[SyncManager] Peer " << ip << " already known" << std::endl;
        return;
    }
    PeerInfo p;
    p.ip        = ip;
    p.hostname  = ip; // Use IP as hostname for manual peers
    p.role      = (mRole == SyncRole::SLAVE) ? SyncRole::MASTER : SyncRole::SLAVE;
    p.lastSeen  = std::chrono::steady_clock::now();
    p.connected = true;
    mPeers.push_back(p);
    std::cout << "[SyncManager] Manual peer added: " << ip << std::endl;
    if (mOnToast) mOnToast("Manual peer added: " + ip);

    if (mRole == SyncRole::SLAVE) {
        connectToMaster(p);
    }
}

void SyncManager::removePeer(const std::string& ip) {
    auto it = std::find_if(mPeers.begin(), mPeers.end(),
                           [&](const PeerInfo& p) { return p.ip == ip; });
    if (it == mPeers.end()) return;
    std::string hostname = it->hostname;
    if (mRole == SyncRole::SLAVE && it->role == SyncRole::MASTER) {
        mConnectedToMaster = false;
    }
    mPeers.erase(it);
    std::cout << "[SyncManager] Peer removed: " << hostname << " (" << ip << ")" << std::endl;
    if (mOnToast) mOnToast("Peer disconnected: " + hostname);
}

// ── Peer management ──────────────────────────────────────────────────────────

PeerInfo* SyncManager::findPeerByIp(const std::string& ip) {
    for (auto& p : mPeers) if (p.ip == ip) return &p;
    return nullptr;
}

void SyncManager::updatePeerTimeouts() {
    auto now = std::chrono::steady_clock::now();
    for (auto& peer : mPeers) {
        if (!peer.connected) continue;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - peer.lastSeen).count();
        if (elapsed > PEER_TIMEOUT_MS) {
            peer.connected = false;
            std::cout << "[SyncManager] Peer " << peer.hostname << " timed out." << std::endl;
            if (mOnToast) mOnToast("Lost connection to " + peer.hostname);
            if (mRole == SyncRole::SLAVE && peer.role == SyncRole::MASTER) {
                mConnectedToMaster = false;
            }
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

int64_t SyncManager::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string SyncManager::localHostname() const {
    char buf[256] = {};
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    gethostname(buf, sizeof(buf));
#else
    gethostname(buf, sizeof(buf));
#endif
    return buf[0] ? buf : "bbfx-host";
}

// ── JSON config ───────────────────────────────────────────────────────────────

nlohmann::json SyncManager::toJson() const {
    nlohmann::json j;
    j["role"] = syncRoleName(mRole);
    auto peers = nlohmann::json::array();
    for (const auto& p : mPeers) {
        if (p.connected) {
            peers.push_back({{"hostname", p.hostname}, {"ip", p.ip}});
        }
    }
    j["knownPeers"] = peers;
    return j;
}

void SyncManager::fromJson(const nlohmann::json& j) {
    if (j.contains("role")) {
        mRole = syncRoleFromString(j["role"].get<std::string>());
    }
    // knownPeers are rediscovered at runtime; no need to restore them.
}

} // namespace bbfx

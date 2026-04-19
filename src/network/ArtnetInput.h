#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <sol/sol.hpp>

namespace bbfx {

/// v3.5 Lot M — singleton Art-Net listener on UDP 6454.
///
/// Background thread receives raw Art-Net packets (header "Art-Net\0",
/// OpCode 0x5000 ArtDMX), maintains one 512-value channel state per
/// universe, and dispatches onReceive callbacks on the main thread.
///
/// `tick()` is the per-frame pump that fires Lua callbacks. Call from
/// StudioApp::capture().
class ArtnetInput {
public:
    static ArtnetInput& instance();

    static constexpr int ARTNET_PORT = 6454;

    /// Start the listener. Idempotent.
    bool start();
    void stop();
    bool isRunning() const { return mRunning; }

    /// Per-frame dispatch of queued packets to Lua callbacks.
    void tick();

    /// Snapshot of 512 channel values for `universe` (0..15). All zeros
    /// if no packet has arrived yet.
    std::array<uint8_t, 512> channels(int universe) const;

    /// Register a Lua callback for a given universe. Returns a handle
    /// usable with off(handle) for removal.
    int  on(int universe, sol::function cb);
    void off(int handle);

    /// Send a raw UDP datagram to (ip, port). Used by bbfx.artnet.send
    /// with a packet built by buildDmxPacket() below.
    static bool sendRaw(const std::string& ip, int port,
                         const std::vector<uint8_t>& data);

    /// Construct an Art-Net ArtDMX packet for `universe` with `data`
    /// (0..512 bytes). Symmetric with ArtnetOutputNode::buildPacket and
    /// shared here so bbfx-core can build packets without depending on
    /// the studio target.
    static std::vector<uint8_t> buildDmxPacket(int universe,
                                                 const std::vector<uint8_t>& data,
                                                 uint8_t sequence);

private:
    ArtnetInput();
    ~ArtnetInput();
    ArtnetInput(const ArtnetInput&) = delete;
    ArtnetInput& operator=(const ArtnetInput&) = delete;

    void listenLoop();
    void parsePacket(const uint8_t* data, size_t len);

    struct Callback {
        int handle;
        int universe;
        sol::function cb;
    };
    struct QueuedPacket {
        int universe;
        std::array<uint8_t, 512> data;
    };

    std::atomic<bool> mRunning{false};
    std::thread mThread;

    mutable std::mutex mMutex;
    std::unordered_map<int, std::array<uint8_t, 512>> mLastChannels;
    std::vector<QueuedPacket> mPendingQueue;  // drained in tick()
    std::vector<Callback> mCallbacks;
    int mNextHandle = 1;

#ifdef _WIN32
    uintptr_t mSocket = ~uintptr_t(0);
#else
    int mSocket = -1;
#endif
};

} // namespace bbfx

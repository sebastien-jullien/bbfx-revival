#pragma once
#include "../osc/OscMessage.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace bbfx {

/// Thread-safe UDP server for receiving OSC messages.
/// Follows the same pattern as TcpServer: separate listener thread + mutex queue.
class UdpServer {
public:
    UdpServer();
    ~UdpServer();

    bool start(int port);
    void stop();
    bool isRunning() const { return mRunning; }
    int getPort() const { return mPort; }

    /// Drain all pending messages (call from main thread each frame).
    std::vector<OscMessage> poll();

    /// Send an OSC message to a target IP:port.
    static bool send(const std::string& ip, int port, const OscMessage& msg);

    /// Get all unique addresses seen (for browser).
    std::vector<std::string> getDiscoveredAddresses() const;

private:
    void listenLoop();

    int mPort = 0;
    std::atomic<bool> mRunning{false};
    std::thread mListenThread;

    std::queue<OscMessage> mMessageQueue;
    mutable std::mutex mQueueMutex;

    // Discovered addresses
    std::vector<std::string> mDiscoveredAddresses;
    mutable std::mutex mAddressMutex;

#ifdef _WIN32
    uintptr_t mSocket = ~0ULL; // SOCKET = UINT_PTR on Windows
#else
    int mSocket = -1;
#endif
};

} // namespace bbfx

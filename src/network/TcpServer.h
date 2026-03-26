#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
constexpr SocketType INVALID_SOCK = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType INVALID_SOCK = -1;
#endif

namespace bbfx {

/// Thread-safe TCP server for the BBFx shell.
/// Accepts connections on a port, reads lines from clients,
/// and queues them for the main thread to process.
class TcpServer {
public:
    struct Message {
        int clientId;
        std::string text;
    };

    TcpServer(int port, int maxClients = 2);
    ~TcpServer();

    void start();
    void stop();

    /// Poll for received messages (call from main thread).
    std::vector<Message> poll();

    /// Send a response to a specific client.
    void send(int clientId, const std::string& msg);

    bool isRunning() const { return mRunning.load(); }

private:
    void listenLoop();
    void closeSocket(SocketType& sock);

    int mPort;
    int mMaxClients;
    SocketType mListenSocket = INVALID_SOCK;
    std::atomic<bool> mRunning{false};
    std::thread mThread;

    // Client tracking
    struct Client {
        SocketType socket = INVALID_SOCK;
        int id = 0;
        std::string lineBuffer;
    };
    std::vector<Client> mClients;
    int mNextClientId = 1;

    // Thread-safe message queues
    std::mutex mInMutex;
    std::queue<Message> mInQueue;  // client → main thread

    std::mutex mOutMutex;
    std::queue<Message> mOutQueue; // main thread → client

#ifdef _WIN32
    static bool sWsaInitialized;
    static int sWsaRefCount;
    static void wsaInit();
    static void wsaCleanup();
#endif
};

} // namespace bbfx

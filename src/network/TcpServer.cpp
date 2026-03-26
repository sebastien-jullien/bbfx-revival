#include "TcpServer.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#define closesocket close
#endif

namespace bbfx {

#ifdef _WIN32
bool TcpServer::sWsaInitialized = false;
int TcpServer::sWsaRefCount = 0;

void TcpServer::wsaInit() {
    if (!sWsaInitialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        sWsaInitialized = true;
    }
    sWsaRefCount++;
}

void TcpServer::wsaCleanup() {
    sWsaRefCount--;
    if (sWsaRefCount <= 0 && sWsaInitialized) {
        WSACleanup();
        sWsaInitialized = false;
        sWsaRefCount = 0;
    }
}
#endif

TcpServer::TcpServer(int port, int maxClients)
    : mPort(port), mMaxClients(maxClients)
{
#ifdef _WIN32
    wsaInit();
#endif
}

TcpServer::~TcpServer() {
    stop();
#ifdef _WIN32
    wsaCleanup();
#endif
}

void TcpServer::closeSocket(SocketType& sock) {
    if (sock != INVALID_SOCK) {
        closesocket(sock);
        sock = INVALID_SOCK;
    }
}

void TcpServer::start() {
    if (mRunning.load()) return;

    // Create listening socket
    mListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListenSocket == INVALID_SOCK) {
        std::cerr << "[TcpServer] Failed to create socket" << std::endl;
        return;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<unsigned short>(mPort));

    if (bind(mListenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[TcpServer] Failed to bind on port " << mPort << std::endl;
        closeSocket(mListenSocket);
        return;
    }

    if (listen(mListenSocket, 4) < 0) {
        std::cerr << "[TcpServer] Failed to listen" << std::endl;
        closeSocket(mListenSocket);
        return;
    }

    mRunning.store(true);
    mThread = std::thread(&TcpServer::listenLoop, this);
    std::cout << "[TcpServer] Listening on port " << mPort << std::endl;
}

void TcpServer::stop() {
    if (!mRunning.load()) return;
    mRunning.store(false);

    // Close the listen socket to unblock accept()
    closeSocket(mListenSocket);

    if (mThread.joinable()) {
        mThread.join();
    }

    // Close all client sockets
    for (auto& client : mClients) {
        closeSocket(client.socket);
    }
    mClients.clear();
}

void TcpServer::listenLoop() {
    // Set listen socket to non-blocking for accept
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(mListenSocket, FIONBIO, &mode);
#else
    int flags = fcntl(mListenSocket, F_GETFL, 0);
    fcntl(mListenSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    while (mRunning.load()) {
        // Try to accept new connections
        if (static_cast<int>(mClients.size()) < mMaxClients) {
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SocketType clientSock = accept(mListenSocket,
                reinterpret_cast<sockaddr*>(&clientAddr),
#ifdef _WIN32
                &addrLen
#else
                reinterpret_cast<socklen_t*>(&addrLen)
#endif
            );
            if (clientSock != INVALID_SOCK) {
                // Set client socket non-blocking
#ifdef _WIN32
                u_long cmode = 1;
                ioctlsocket(clientSock, FIONBIO, &cmode);
#else
                int cflags = fcntl(clientSock, F_GETFL, 0);
                fcntl(clientSock, F_SETFL, cflags | O_NONBLOCK);
#endif
                Client c;
                c.socket = clientSock;
                c.id = mNextClientId++;
                mClients.push_back(c);
                std::cout << "[TcpServer] Client " << c.id << " connected" << std::endl;
            }
        }

        // Read from each client
        for (auto it = mClients.begin(); it != mClients.end(); ) {
            char buf[1024];
            int n = recv(it->socket, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                it->lineBuffer.append(buf, n);
                // Process complete lines
                size_t pos;
                while ((pos = it->lineBuffer.find('\n')) != std::string::npos) {
                    std::string line = it->lineBuffer.substr(0, pos);
                    // Remove \r if present
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    it->lineBuffer.erase(0, pos + 1);

                    std::lock_guard<std::mutex> lock(mInMutex);
                    mInQueue.push({it->id, line});
                }
                ++it;
            }
            else if (n == 0) {
                // Client disconnected
                std::cout << "[TcpServer] Client " << it->id << " disconnected" << std::endl;
                closeSocket(it->socket);
                it = mClients.erase(it);
            }
            else {
                // EAGAIN/EWOULDBLOCK = no data, continue
#ifdef _WIN32
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
                    std::cout << "[TcpServer] Client " << it->id << " error, disconnecting" << std::endl;
                    closeSocket(it->socket);
                    it = mClients.erase(it);
                    continue;
                }
                ++it;
            }
        }

        // Send queued outgoing messages
        {
            std::lock_guard<std::mutex> lock(mOutMutex);
            while (!mOutQueue.empty()) {
                auto& msg = mOutQueue.front();
                for (auto& client : mClients) {
                    if (client.id == msg.clientId) {
                        std::string data = msg.text + "\n";
                        ::send(client.socket, data.c_str(), static_cast<int>(data.size()), 0);
                        break;
                    }
                }
                mOutQueue.pop();
            }
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::vector<TcpServer::Message> TcpServer::poll() {
    std::vector<Message> messages;
    std::lock_guard<std::mutex> lock(mInMutex);
    while (!mInQueue.empty()) {
        messages.push_back(std::move(mInQueue.front()));
        mInQueue.pop();
    }
    return messages;
}

void TcpServer::send(int clientId, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mOutMutex);
    mOutQueue.push({clientId, msg});
}

} // namespace bbfx

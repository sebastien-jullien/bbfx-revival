#include "UdpServer.h"
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace bbfx {

UdpServer::UdpServer() {}

UdpServer::~UdpServer() {
    stop();
}

bool UdpServer::start(int port) {
    if (mRunning) return true;
    mPort = port;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mSocket == INVALID_SOCKET) {
        std::cerr << "[OSC] Failed to create UDP socket" << std::endl;
        return false;
    }
#else
    mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mSocket < 0) {
        std::cerr << "[OSC] Failed to create UDP socket" << std::endl;
        return false;
    }
#endif

    // Set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(static_cast<SOCKET>(mSocket), FIONBIO, &mode);
#else
    int flags = fcntl(mSocket, F_GETFL, 0);
    fcntl(mSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(static_cast<int>(mSocket), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[OSC] Failed to bind UDP port " << port << std::endl;
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(mSocket));
#else
        close(mSocket);
#endif
        return false;
    }

    mRunning = true;
    mListenThread = std::thread(&UdpServer::listenLoop, this);
    std::cout << "[OSC] Listening on UDP port " << port << std::endl;
    return true;
}

void UdpServer::stop() {
    if (!mRunning) return;
    mRunning = false;

#ifdef _WIN32
    closesocket(static_cast<SOCKET>(mSocket));
#else
    close(mSocket);
#endif

    if (mListenThread.joinable()) mListenThread.join();
    std::cout << "[OSC] Stopped UDP listener" << std::endl;
}

std::vector<OscMessage> UdpServer::poll() {
    std::lock_guard<std::mutex> lock(mQueueMutex);
    std::vector<OscMessage> result;
    while (!mMessageQueue.empty()) {
        result.push_back(std::move(mMessageQueue.front()));
        mMessageQueue.pop();
    }
    return result;
}

bool UdpServer::send(const std::string& ip, int port, const OscMessage& msg) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return false;
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    auto data = OscMessage::serialize(msg);
    int sent = sendto(static_cast<int>(sock), reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()), 0,
                      reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    return sent > 0;
}

std::vector<std::string> UdpServer::getDiscoveredAddresses() const {
    std::lock_guard<std::mutex> lock(mAddressMutex);
    return mDiscoveredAddresses;
}

void UdpServer::listenLoop() {
    uint8_t buf[4096];
    while (mRunning) {
        sockaddr_in from{};
#ifdef _WIN32
        int fromLen = sizeof(from);
        int n = recvfrom(static_cast<SOCKET>(mSocket), reinterpret_cast<char*>(buf), sizeof(buf), 0,
                         reinterpret_cast<sockaddr*>(&from), &fromLen);
#else
        socklen_t fromLen = sizeof(from);
        int n = recvfrom(mSocket, buf, sizeof(buf), 0,
                         reinterpret_cast<sockaddr*>(&from), &fromLen);
#endif

        if (n > 0) {
            OscMessage msg;
            if (OscMessage::parse(buf, n, msg)) {
                // Capture sender IP
                char ipStr[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));
                msg.senderIp = ipStr;

                // Track discovered addresses
                {
                    std::lock_guard<std::mutex> lock(mAddressMutex);
                    if (std::find(mDiscoveredAddresses.begin(), mDiscoveredAddresses.end(), msg.address)
                        == mDiscoveredAddresses.end()) {
                        mDiscoveredAddresses.push_back(msg.address);
                    }
                }
                // Queue for main thread
                {
                    std::lock_guard<std::mutex> lock(mQueueMutex);
                    mMessageQueue.push(std::move(msg));
                }
            }
        } else {
            // No data available, sleep briefly to avoid busy wait
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

} // namespace bbfx

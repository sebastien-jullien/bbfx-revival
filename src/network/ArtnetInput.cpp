#include "ArtnetInput.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
using socklen_t = int;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR   (-1)
#  define closesocket close
using SOCKET = int;
#endif

namespace bbfx {

namespace {
bool gWsaInit = false;

void ensureWsa() {
#ifdef _WIN32
    if (gWsaInit) return;
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) == 0) gWsaInit = true;
#else
    gWsaInit = true;
#endif
}
} // anonymous

ArtnetInput& ArtnetInput::instance() {
    static ArtnetInput sInst;
    return sInst;
}

ArtnetInput::ArtnetInput() = default;

ArtnetInput::~ArtnetInput() {
    stop();
}

bool ArtnetInput::start() {
    if (mRunning) return true;
    ensureWsa();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        std::cerr << "[ArtnetInput] socket() failed" << std::endl;
        return false;
    }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>(&one), sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(ARTNET_PORT);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[ArtnetInput] bind(6454) failed — another Art-Net tool holding port?" << std::endl;
        closesocket(s);
        return false;
    }
    mSocket = static_cast<uintptr_t>(s);
    mRunning = true;
    mThread = std::thread([this]() { listenLoop(); });
    std::cout << "[ArtnetInput] listening on UDP " << ARTNET_PORT << std::endl;
    return true;
}

void ArtnetInput::stop() {
    if (!mRunning) return;
    mRunning = false;
    if (mSocket != static_cast<uintptr_t>(INVALID_SOCKET)) {
        closesocket(static_cast<SOCKET>(mSocket));
        mSocket = static_cast<uintptr_t>(INVALID_SOCKET);
    }
    if (mThread.joinable()) mThread.join();
}

void ArtnetInput::listenLoop() {
    uint8_t buf[1024];
    while (mRunning) {
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        int n = recvfrom(static_cast<SOCKET>(mSocket),
                         reinterpret_cast<char*>(buf), sizeof(buf), 0,
                         reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) {
            if (!mRunning) break;
            continue;
        }
        parsePacket(buf, static_cast<size_t>(n));
    }
}

void ArtnetInput::parsePacket(const uint8_t* data, size_t len) {
    // Minimum: 8B header + 2B opcode + 2B version + 1B seq + 1B physical
    // + 2B universe + 2B length = 18B before DMX data.
    if (len < 18) return;
    static const uint8_t kArtNet[8] = { 'A','r','t','-','N','e','t',0 };
    if (std::memcmp(data, kArtNet, 8) != 0) return;
    uint16_t opcode = static_cast<uint16_t>(data[8] | (data[9] << 8));
    if (opcode != 0x5000) return;  // ArtDMX
    int universe = static_cast<int>(data[14] | (data[15] << 8));  // sub-net + universe
    int dataLen  = static_cast<int>((data[16] << 8) | data[17]);  // big-endian
    if (dataLen < 2 || dataLen > 512) return;
    if (18 + dataLen > (int)len) return;

    QueuedPacket q;
    q.universe = universe & 0x0F;  // clamp to 0-15 for the public API
    q.data.fill(0);
    int n = std::min(dataLen, 512);
    std::memcpy(q.data.data(), data + 18, n);

    std::lock_guard<std::mutex> lock(mMutex);
    mLastChannels[q.universe] = q.data;
    mPendingQueue.push_back(std::move(q));
}

void ArtnetInput::tick() {
    std::vector<QueuedPacket> pending;
    std::vector<Callback> cbsCopy;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        pending.swap(mPendingQueue);
        cbsCopy = mCallbacks;
    }
    for (auto& p : pending) {
        for (auto& c : cbsCopy) {
            if (c.universe == p.universe) {
                try {
                    // Build a Lua table of 512 floats (0..1) from the
                    // packet. sol2 accepts a std::array via its wrapper,
                    // but for clarity we let the binding layer wrap it.
                    sol::state_view lua(c.cb.lua_state());
                    sol::table t = lua.create_table();
                    for (int i = 0; i < 512; ++i)
                        t[i + 1] = p.data[i] / 255.0f;
                    c.cb(p.universe, t);
                } catch (const std::exception& e) {
                    std::cerr << "[ArtnetInput] cb error: " << e.what() << std::endl;
                }
            }
        }
    }
}

std::array<uint8_t, 512> ArtnetInput::channels(int universe) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mLastChannels.find(universe);
    if (it == mLastChannels.end()) {
        std::array<uint8_t, 512> empty{};
        empty.fill(0);
        return empty;
    }
    return it->second;
}

int ArtnetInput::on(int universe, sol::function cb) {
    std::lock_guard<std::mutex> lock(mMutex);
    int h = mNextHandle++;
    mCallbacks.push_back(Callback{ h, universe, std::move(cb) });
    return h;
}

void ArtnetInput::off(int handle) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCallbacks.erase(std::remove_if(mCallbacks.begin(), mCallbacks.end(),
        [handle](const Callback& c) { return c.handle == handle; }),
        mCallbacks.end());
}

std::vector<uint8_t> ArtnetInput::buildDmxPacket(int universe,
                                                    const std::vector<uint8_t>& data,
                                                    uint8_t sequence) {
    int len = static_cast<int>(data.size());
    if (len % 2 != 0) ++len;
    std::vector<uint8_t> pkt;
    pkt.reserve(18 + len);
    pkt.push_back('A'); pkt.push_back('r'); pkt.push_back('t');
    pkt.push_back('-'); pkt.push_back('N'); pkt.push_back('e');
    pkt.push_back('t'); pkt.push_back(0);
    pkt.push_back(0x00); pkt.push_back(0x50);        // OpDmx
    pkt.push_back(0x00); pkt.push_back(0x0E);        // Protocol 14
    pkt.push_back(sequence);
    pkt.push_back(0x00);                              // Physical
    pkt.push_back(static_cast<uint8_t>(universe & 0xFF));
    pkt.push_back(static_cast<uint8_t>((universe >> 8) & 0x7F));
    pkt.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>(len & 0xFF));
    for (int i = 0; i < len; ++i) {
        pkt.push_back(i < static_cast<int>(data.size()) ? data[i] : 0);
    }
    return pkt;
}

bool ArtnetInput::sendRaw(const std::string& ip, int port,
                            const std::vector<uint8_t>& data) {
    ensureWsa();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;
    int bcast = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST,
                reinterpret_cast<const char*>(&bcast), sizeof(bcast));
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &dst.sin_addr) != 1) {
        closesocket(s);
        return false;
    }
    int sent = sendto(s, reinterpret_cast<const char*>(data.data()),
                       static_cast<int>(data.size()), 0,
                       reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    closesocket(s);
    return sent > 0;
}

} // namespace bbfx

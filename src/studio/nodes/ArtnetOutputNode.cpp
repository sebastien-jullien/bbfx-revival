#include "ArtnetOutputNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/Animator.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace bbfx {

#ifdef _WIN32
bool ArtnetOutputNode::sWsaInit = false;
#endif

// ── Construction ──────────────────────────────────────────────────────────────

ArtnetOutputNode::ArtnetOutputNode(const std::string& name)
    : AnimationNode(name) {
    // ParamSpec
    { ParamDef d; d.name = "dest_ip"; d.label = "Destination IP"; d.type = ParamType::STRING;
      d.stringVal = "2.0.0.1"; mSpec.addParam(d); }
    { ParamDef d; d.name = "universe"; d.label = "Universe"; d.type = ParamType::INT;
      d.intVal = 0; d.minVal = 0.f; d.maxVal = 32767.f; mSpec.addParam(d); }
    { ParamDef d; d.name = "channel_count"; d.label = "Channel Count"; d.type = ParamType::INT;
      d.intVal = 3; d.minVal = 1.f; d.maxVal = 512.f; mSpec.addParam(d); }
    setParamSpec(&mSpec);

    rebuildPorts(3);

#ifdef _WIN32
    if (!sWsaInit) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        sWsaInit = true;
    }
#endif

    mLastSend = std::chrono::steady_clock::now();
    std::cout << "[Artnet] Node created: " << name << std::endl;
}

ArtnetOutputNode::~ArtnetOutputNode() {
#ifdef _WIN32
    closeSocket();
#endif
}

// ── Port management ───────────────────────────────────────────────────────────

void ArtnetOutputNode::rebuildPorts(int count) {
    // Add new ports as needed (we never remove to keep existing links valid)
    for (int i = mLastChannelCount + 1; i <= count; ++i) {
        std::string portName = "ch" + std::to_string(i);
        auto& inputs = getInputs();
        if (inputs.find(portName) == inputs.end()) {
            addInput(new AnimationPort(portName, 0.0f));
        }
    }
    mLastChannelCount = std::max(mLastChannelCount, count);
    mLastData.resize(static_cast<size_t>(mLastChannelCount), 0);
}

// ── Update ────────────────────────────────────────────────────────────────────

void ArtnetOutputNode::update() {
    // Read params
    auto* pIp  = mSpec.getParam("dest_ip");
    auto* pUni = mSpec.getParam("universe");
    auto* pCnt = mSpec.getParam("channel_count");

    std::string ip  = pIp  ? pIp->stringVal : "2.0.0.1";
    int universe    = pUni ? pUni->intVal : 0;
    int count       = pCnt ? std::clamp(pCnt->intVal, 1, 512) : 3;

    // Rebuild ports if count changed
    if (count != mLastChannelCount) rebuildPorts(count);

    // Rate limiting: 44 Hz max
    auto now = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(now - mLastSend).count();
    bool timeFired = mFirstSend || elapsedMs >= ARTNET_INTERVAL_MS;

    // Read inputs
    auto& ins = getInputs();
    std::vector<uint8_t> dmxData(static_cast<size_t>(count), 0);
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        std::string portName = "ch" + std::to_string(i + 1);
        auto it = ins.find(portName);
        float v = it != ins.end() ? static_cast<float>(it->second->getValue()) : 0.f;
        uint8_t dmxVal = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(v * 255.f)), 0, 255));
        dmxData[i] = dmxVal;
        if (dmxVal != mLastData[i]) changed = true;
    }

    if ((changed || timeFired) && timeFired) {
        sendPacket(ip, universe, dmxData);
        mLastData   = dmxData;
        mLastSend   = now;
        mFirstSend  = false;
    }

    fireUpdate();
}

// ── Artnet packet ─────────────────────────────────────────────────────────────

std::vector<uint8_t> ArtnetOutputNode::buildPacket(int universe,
                                                     const std::vector<uint8_t>& data,
                                                     uint8_t sequence) {
    // Ensure even length (Art-Net requirement)
    int len = static_cast<int>(data.size());
    if (len % 2 != 0) ++len;

    std::vector<uint8_t> pkt;
    pkt.reserve(18 + len);

    // Header: "Art-Net\0"
    pkt.push_back('A'); pkt.push_back('r'); pkt.push_back('t');
    pkt.push_back('-'); pkt.push_back('N'); pkt.push_back('e');
    pkt.push_back('t'); pkt.push_back(0);

    // OpCode: 0x5000 little-endian = OpDmx
    pkt.push_back(0x00); pkt.push_back(0x50);

    // Protocol version: 14 big-endian
    pkt.push_back(0x00); pkt.push_back(0x0E);

    // Sequence
    pkt.push_back(sequence);

    // Physical
    pkt.push_back(0x00);

    // Universe: 15-bit little-endian
    pkt.push_back(static_cast<uint8_t>(universe & 0xFF));
    pkt.push_back(static_cast<uint8_t>((universe >> 8) & 0x7F));

    // Length: big-endian
    pkt.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>(len & 0xFF));

    // DMX data (padded to even length with 0)
    for (int i = 0; i < len; ++i) {
        pkt.push_back(i < static_cast<int>(data.size()) ? data[i] : 0);
    }

    return pkt;
}

void ArtnetOutputNode::sendPacket(const std::string& ip, int universe,
                                   const std::vector<uint8_t>& data) {
    // Increment sequence (wrap 1-255, 0 = no sequence tracking)
    ++mSequence;
    if (mSequence == 0) mSequence = 1;

    auto pkt = buildPacket(universe, data, mSequence);

#ifdef _WIN32
    ensureSocket();
    if (mSocket == INVALID_SOCKET) return;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ARTNET_PORT);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "[Artnet] Invalid destination IP: " << ip << std::endl;
        return;
    }

    sendto(mSocket, reinterpret_cast<const char*>(pkt.data()),
           static_cast<int>(pkt.size()), 0,
           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#endif
}

// ── Quick Assign audio ────────────────────────────────────────────────────────

void ArtnetOutputNode::quickAssignAudio() {
    auto* animator = Animator::instance();
    if (!animator) return;

    // Find first AudioAnalyzerNode
    AnimationNode* analyzer = nullptr;
    for (const auto& name : animator->getRegisteredNodeNames()) {
        auto* n = animator->getRegisteredNode(name);
        if (n && n->getTypeName() == "AudioAnalyzerNode") { analyzer = n; break; }
    }
    if (!analyzer) {
        std::cout << "[Artnet] quickAssignAudio: no AudioAnalyzerNode in DAG" << std::endl;
        return;
    }

    // Ensure at least 3 channels
    auto* pCnt = mSpec.getParam("channel_count");
    if (pCnt && pCnt->intVal < 3) pCnt->intVal = 3;
    rebuildPorts(3);

    // Link: low→ch1, mid→ch2, high→ch3
    auto& srcOuts = analyzer->getOutputs();
    auto& dstIns  = getInputs();
    const std::vector<std::pair<std::string,std::string>> mapping = {
        {"low", "ch1"}, {"mid", "ch2"}, {"high", "ch3"}
    };
    for (auto& [src, dst] : mapping) {
        auto srcIt = srcOuts.find(src);
        auto dstIt = dstIns.find(dst);
        if (srcIt != srcOuts.end() && dstIt != dstIns.end()) {
            animator->link(srcIt->second, dstIt->second);
        }
    }
    std::cout << "[Artnet] quickAssignAudio: audio bands linked to DMX ch1-ch3" << std::endl;
}

// ── Socket helpers (Windows) ──────────────────────────────────────────────────

#ifdef _WIN32
void ArtnetOutputNode::ensureSocket() {
    if (mSocket != INVALID_SOCKET) return;
    mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mSocket == INVALID_SOCKET) {
        std::cerr << "[Artnet] Failed to create UDP socket" << std::endl;
        return;
    }
    // Enable broadcast
    BOOL bcast = TRUE;
    if (setsockopt(mSocket, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&bcast), sizeof(bcast)) == SOCKET_ERROR) {
        std::cerr << "[Artnet] Failed to enable SO_BROADCAST" << std::endl;
    }
}

void ArtnetOutputNode::closeSocket() {
    if (mSocket != INVALID_SOCKET) {
        closesocket(mSocket);
        mSocket = INVALID_SOCKET;
    }
}
#endif

} // namespace bbfx

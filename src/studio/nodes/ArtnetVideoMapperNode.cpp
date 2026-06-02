#include "ArtnetVideoMapperNode.h"
#include "ArtnetOutputNode.h"
#include "../../core/Animator.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <algorithm>
#include <cmath>
#include <iostream>

#ifdef _WIN32
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace bbfx {

#ifdef _WIN32
bool ArtnetVideoMapperNode::sWsaInit = false;

void ArtnetVideoMapperNode::ensureSocket() {
    if (!sWsaInit) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) sWsaInit = true;
        else {
            std::cerr << "[ArtnetVideoMapper] WSAStartup failed" << std::endl;
            return;
        }
    }
    if (mSocket != INVALID_SOCKET) return;
    mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mSocket == INVALID_SOCKET) {
        std::cerr << "[ArtnetVideoMapper] socket() failed" << std::endl;
        return;
    }
    BOOL bcast = TRUE;
    setsockopt(mSocket, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&bcast), sizeof(bcast));
}

void ArtnetVideoMapperNode::closeSocket() {
    if (mSocket != INVALID_SOCKET) {
        closesocket(mSocket);
        mSocket = INVALID_SOCKET;
    }
}

void ArtnetVideoMapperNode::sendDmxPacket(int universe, const std::vector<uint8_t>& chunk) {
    ensureSocket();
    if (mSocket == INVALID_SOCKET) return;

    ++mSequence;
    if (mSequence == 0) mSequence = 1;
    auto pkt = ArtnetOutputNode::buildPacket(universe, chunk, mSequence);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ArtnetOutputNode::ARTNET_PORT);
    if (inet_pton(AF_INET, mTargetIp.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "[ArtnetVideoMapper] Invalid target_ip: " << mTargetIp << std::endl;
        return;
    }
    sendto(mSocket, reinterpret_cast<const char*>(pkt.data()),
           static_cast<int>(pkt.size()), 0,
           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}
#endif

ArtnetVideoMapperNode::~ArtnetVideoMapperNode() {
    cleanup();
}

void ArtnetVideoMapperNode::cleanup() {
#ifdef _WIN32
    closeSocket();
#endif
}

ArtnetVideoMapperNode::ArtnetVideoMapperNode(const std::string& name)
    : AnimationNode(name)
{
    ParamDef ipDef;
    ipDef.name = "target_ip"; ipDef.label = "Target IP";
    ipDef.type = ParamType::STRING; ipDef.stringVal = "255.255.255.255";
    mSpec.addParam(ipDef);

    ParamDef uDef;
    uDef.name = "start_universe"; uDef.label = "Start Universe";
    uDef.type = ParamType::INT;
    uDef.intVal = 0; uDef.minVal = 0; uDef.maxVal = 32767;
    mSpec.addParam(uDef);

    ParamDef layDef;
    layDef.name = "pixel_layout"; layDef.label = "Pixel Layout";
    layDef.type = ParamType::ENUM;
    layDef.stringVal = "linear_horizontal";
    layDef.choices = {"linear_horizontal", "linear_vertical",
                      "serpentine_horizontal", "serpentine_vertical", "matrix"};
    mSpec.addParam(layDef);

    ParamDef pxDef;
    pxDef.name = "pixel_count_x"; pxDef.label = "Pixels X";
    pxDef.type = ParamType::INT;
    pxDef.intVal = 16; pxDef.minVal = 1; pxDef.maxVal = 512;
    mSpec.addParam(pxDef);

    ParamDef pyDef;
    pyDef.name = "pixel_count_y"; pyDef.label = "Pixels Y";
    pyDef.type = ParamType::INT;
    pyDef.intVal = 1; pyDef.minVal = 1; pyDef.maxVal = 512;
    mSpec.addParam(pyDef);

    ParamDef fmtDef;
    fmtDef.name = "pixel_format"; fmtDef.label = "Pixel Format";
    fmtDef.type = ParamType::ENUM;
    fmtDef.stringVal = "RGB";
    fmtDef.choices = {"RGB", "RGBW", "BGR"};
    mSpec.addParam(fmtDef);

    ParamDef gDef;
    gDef.name = "gamma"; gDef.label = "Gamma";
    gDef.type = ParamType::FLOAT;
    gDef.floatVal = 2.2f; gDef.minVal = 0.5f; gDef.maxVal = 3.0f;
    mSpec.addParam(gDef);

    // C2 — readback_rate_hz (spec S4 §500) : cadence de lecture de la texture
    // source → Art-Net. Auparavant absent du ParamSpec (mReadbackRateHz hardcodé 30).
    ParamDef rrDef;
    rrDef.name = "readback_rate_hz"; rrDef.label = "Readback Rate (Hz)";
    rrDef.type = ParamType::INT;
    rrDef.intVal = 30; rrDef.minVal = 1; rrDef.maxVal = 60;
    mSpec.addParam(rrDef);

    setParamSpec(&mSpec);

    // C2 — port consommateur (Pattern 3) : pulle un nom de texture d'un producteur
    // upstream (NoiseTexture, Spectrogram, Theora, Grayscale, viewport RTT share…).
    // C'est cette texture qui est lue chaque tick et mappée en pixels Art-Net.
    addInput(new AnimationPort("source_texture", 0.0f, true));

    // v3.5.2 Sprint S8 Lot AT — `enabled` port provided by AnimationNode base
    // (when 0, the whole node is frozen → UDP send suppressed too).

    addOutput(new AnimationPort("packet_count", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("packet_count", "Cumulative count of Artnet packets sent since node creation.");
}

void ArtnetVideoMapperNode::_setSourcePixels(const std::vector<uint8_t>& rgb) {
    mSource = rgb;
}

void ArtnetVideoMapperNode::pullSourceFromUpstream() {
    // Résout le producteur de texture lié au port `source_texture` et lit ses
    // pixels (downsamplés à mPixelCountX × mPixelCountY) dans mSource (RGB).
    auto* animator = Animator::instance();
    if (!animator) return;
    auto& in = getInputs();
    auto itSrc = in.find("source_texture");
    if (itSrc == in.end()) return;

    std::string texName;
    for (auto* src : animator->getSourceNodes(itSrc->second)) {
        if (!src) continue;
        auto* spec = src->getParamSpec();
        if (!spec) continue;
        for (const char* mirror : {"texture_out", "current_texture", "texture"}) {
            if (auto* p = spec->getParam(mirror); p && !p->stringVal.empty()) {
                if (!Ogre::TextureManager::getSingleton().getByName(p->stringVal).isNull()) {
                    texName = p->stringVal; break;
                }
            }
        }
        if (!texName.empty()) break;
    }
    if (texName.empty()) return;

    auto tex = Ogre::TextureManager::getSingleton().getByName(texName);
    if (tex.isNull()) return;

    const int W = std::clamp(mPixelCountX, 1, 512);
    const int H = std::clamp(mPixelCountY, 1, 512);
    std::vector<uint8_t> rgb(static_cast<size_t>(W) * H * 3, 0);
    try {
        // blitToMemory gère le scaling (downsample) source → grille cible.
        Ogre::PixelBox dst(W, H, 1, Ogre::PF_BYTE_RGB, rgb.data());
        tex->getBuffer()->blitToMemory(dst);
        mSource = std::move(rgb);
    } catch (const Ogre::Exception& e) {
        std::cerr << "[ArtnetVideoMapper] readback échoué (" << e.getDescription()
                  << ")" << std::endl;
    }
}

std::vector<uint8_t> ArtnetVideoMapperNode::applyLayout(const std::vector<uint8_t>& src) const {
    int w = mPixelCountX;
    int h = mPixelCountY;
    int total = w * h;
    int srcCount = static_cast<int>(src.size() / 3);
    if (srcCount < total) total = srcCount;

    std::vector<uint8_t> out(total * 3, 0);
    // src est une grille W×H (w = mPixelCountX, h = mPixelCountY) en row-major,
    // stride de ligne = w. Pour les layouts VERTICAUX, l'index LED i parcourt
    // d'abord une colonne (h LEDs) : il faut donc décoder i avec la hauteur h
    // (col = i/h, row = i%h) et indexer la source avec le stride w — sinon le
    // mapping est faux dès que w != h (ancien bug : décodage sur w + stride w).
    for (int i = 0; i < total; ++i) {
        int srcX = 0, srcY = 0;
        switch (mLayout) {
        case PixelLayout::LinearHorizontal:
        case PixelLayout::Matrix:
            srcX = i % w;
            srcY = i / w;
            break;
        case PixelLayout::SerpentineHorizontal:
            srcX = i % w;
            srcY = i / w;
            if (srcY % 2 == 1) srcX = (w - 1) - srcX;   // lignes impaires inversées
            break;
        case PixelLayout::LinearVertical: {
            int c = i / h;          // colonne 0..w-1
            int r = i % h;          // ligne dans la colonne 0..h-1
            srcX = c; srcY = r;
            break;
        }
        case PixelLayout::SerpentineVertical: {
            int c = i / h;
            int r = i % h;
            if (c % 2 == 1) r = (h - 1) - r;            // colonnes impaires inversées
            srcX = c; srcY = r;
            break;
        }
        }
        int srcIdx = (srcY * w + srcX) * 3;
        if (srcIdx >= 0 && srcIdx + 2 < (int)src.size()) {
            out[i * 3 + 0] = src[srcIdx + 0];
            out[i * 3 + 1] = src[srcIdx + 1];
            out[i * 3 + 2] = src[srcIdx + 2];
        }
    }
    return out;
}

std::vector<uint8_t> ArtnetVideoMapperNode::applyFormat(const std::vector<uint8_t>& src) const {
    if (mFormat == PixelFormat::RGB) return src;
    int n = static_cast<int>(src.size() / 3);
    if (mFormat == PixelFormat::BGR) {
        std::vector<uint8_t> out(src.size());
        for (int i = 0; i < n; ++i) {
            out[i * 3 + 0] = src[i * 3 + 2];  // B
            out[i * 3 + 1] = src[i * 3 + 1];  // G
            out[i * 3 + 2] = src[i * 3 + 0];  // R
        }
        return out;
    }
    // RGBW: 4 bytes per pixel; W = max(R,G,B)
    std::vector<uint8_t> out(n * 4, 0);
    for (int i = 0; i < n; ++i) {
        uint8_t r = src[i * 3 + 0];
        uint8_t g = src[i * 3 + 1];
        uint8_t b = src[i * 3 + 2];
        uint8_t w = std::max({r, g, b});
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = w;
    }
    return out;
}

void ArtnetVideoMapperNode::applyGamma(std::vector<uint8_t>& data) const {
    if (std::abs(mGamma - 1.0f) < 1e-3f) return;
    for (auto& byte : data) {
        float v = byte / 255.0f;
        v = std::pow(v, mGamma);
        byte = static_cast<uint8_t>(std::clamp(v * 255.0f, 0.0f, 255.0f));
    }
}

std::vector<std::vector<uint8_t>> ArtnetVideoMapperNode::processToUniverses() {
    auto reordered = applyLayout(mSource);
    auto formatted = applyFormat(reordered);
    applyGamma(formatted);
    mLastReordered = formatted;

    // Split into universes (max 510 bytes per Art-Net DMX universe — even-only;
    // 170 RGB LEDs * 3 bytes = 510, or 127 RGBW * 4 bytes = 508).
    constexpr int kMaxBytesPerUniverse = 510;
    std::vector<std::vector<uint8_t>> universes;
    int total = static_cast<int>(formatted.size());
    for (int off = 0; off < total; off += kMaxBytesPerUniverse) {
        int end = std::min(total, off + kMaxBytesPerUniverse);
        std::vector<uint8_t> chunk(formatted.begin() + off, formatted.begin() + end);
        // Pad to even length (Art-Net requirement)
        if (chunk.size() % 2) chunk.push_back(0);
        universes.push_back(std::move(chunk));
    }
    mLastPacketCount = static_cast<int>(universes.size());
    return universes;
}

void ArtnetVideoMapperNode::update() {
    if (!mEnabled) return;

    auto* ipP    = mSpec.getParam("target_ip");
    auto* uP     = mSpec.getParam("start_universe");
    auto* layP   = mSpec.getParam("pixel_layout");
    auto* pxP    = mSpec.getParam("pixel_count_x");
    auto* pyP    = mSpec.getParam("pixel_count_y");
    auto* fmtP   = mSpec.getParam("pixel_format");
    auto* gP     = mSpec.getParam("gamma");
    if (ipP)  mTargetIp = ipP->stringVal;
    if (uP)   mStartUniverse = uP->intVal;
    if (pxP)  mPixelCountX = std::clamp(pxP->intVal, 1, 512);
    if (pyP)  mPixelCountY = std::clamp(pyP->intVal, 1, 512);
    if (gP)   mGamma = std::clamp(gP->floatVal, 0.5f, 3.0f);
    if (auto* rrP = mSpec.getParam("readback_rate_hz"))
        mReadbackRateHz = std::clamp(rrP->intVal, 1, 60);

    if (layP) {
        if      (layP->stringVal == "linear_vertical")        mLayout = PixelLayout::LinearVertical;
        else if (layP->stringVal == "serpentine_horizontal")  mLayout = PixelLayout::SerpentineHorizontal;
        else if (layP->stringVal == "serpentine_vertical")    mLayout = PixelLayout::SerpentineVertical;
        else if (layP->stringVal == "matrix")                  mLayout = PixelLayout::Matrix;
        else                                                    mLayout = PixelLayout::LinearHorizontal;
    }
    if (fmtP) {
        if      (fmtP->stringVal == "RGBW") mFormat = PixelFormat::RGBW;
        else if (fmtP->stringVal == "BGR")  mFormat = PixelFormat::BGR;
        else                                  mFormat = PixelFormat::RGB;
    }

    // C2 — readback de la texture upstream (port source_texture) → mSource, en
    // production. Rate-limité à readback_rate_hz (séparé de l'envoi). En test,
    // _setSourcePixels() alimente directement mSource sans port lié.
    {
        auto now = std::chrono::steady_clock::now();
        bool firstRb = (mLastReadbackTime.time_since_epoch().count() == 0);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastReadbackTime).count();
        int periodMs = (mReadbackRateHz > 0) ? (1000 / mReadbackRateHz) : 33;
        if (firstRb || elapsed >= periodMs) {
            pullSourceFromUpstream();
            mLastReadbackTime = now;
        }
    }

    // Process source pixels and dispatch Art-Net DMX packets over UDP.
    // Rate-limited via readback_rate_hz to avoid saturating the network
    // even when the source RTT is updated every frame.
    if (!mSource.empty()) {
        auto universes = processToUniverses();
        auto& out = getOutputs();
        if (out.count("packet_count")) out.at("packet_count")->setValue((float)mLastPacketCount);

        // Rate-limit: at most readback_rate_hz packets per second.
        auto now = std::chrono::steady_clock::now();
        bool firstSend = (mLastSendTime.time_since_epoch().count() == 0);
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - mLastSendTime).count();
        int periodMs = (mReadbackRateHz > 0) ? (1000 / mReadbackRateHz) : 33;
        bool shouldSend = firstSend || (elapsedMs >= periodMs);

        // The `enabled` input port (default 1.0) gates UDP sending.
        auto& in = getInputs();
        bool gated = true;
        if (auto it = in.find("enabled"); it != in.end()) {
            gated = (it->second->getValue() > 0.5f);
        }

        if (gated && shouldSend) {
            for (size_t i = 0; i < universes.size(); ++i) {
                int u = mStartUniverse + static_cast<int>(i);
                sendDmxPacket(u, universes[i]);
            }
            mLastSendTime = now;
        }
    }

    fireUpdate();
}

} // namespace bbfx

#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

namespace bbfx {

/// ArtnetVideoMapperNode — pixel-mapping output. Takes a buffer of RGB pixels
/// (downsampled viewport in production; provided directly via a setter in
/// tests), applies pixel_layout reordering, pixel_format conversion (RGB →
/// RGBW / BGR), gamma correction, then encodes Art-Net DMX packets across
/// however many universes are needed (max 170 RGB LEDs / universe = 510 bytes).
///
/// MadMapper / Resolume Wire compatible.
class ArtnetVideoMapperNode : public AnimationNode {
public:
    enum class PixelLayout {
        LinearHorizontal,     // row-major
        LinearVertical,       // column-major
        SerpentineHorizontal, // alternating row direction
        SerpentineVertical,   // alternating column direction
        Matrix                // alias of LinearHorizontal for now
    };
    enum class PixelFormat { RGB, RGBW, BGR };

    explicit ArtnetVideoMapperNode(const std::string& name);
    ~ArtnetVideoMapperNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "ArtnetVideoMapperNode"; }

    PixelLayout getPixelLayout() const { return mLayout; }
    PixelFormat getPixelFormat() const { return mFormat; }
    int  getPixelCountX() const { return mPixelCountX; }
    int  getPixelCountY() const { return mPixelCountY; }
    int  getStartUniverse() const { return mStartUniverse; }
    int  getLastPacketCount() const { return mLastPacketCount; }
    const std::vector<uint8_t>& getLastReorderedBuffer() const { return mLastReordered; }

    /// Test-only: set the source pixel buffer (RGB triplets, row-major) so
    /// dbg.test() can validate layout/format/gamma without a real RTT.
    void _setSourcePixels(const std::vector<uint8_t>& rgb);

    /// Process the current source pixels through layout/format/gamma and
    /// return the resulting per-universe DMX byte arrays (also stored
    /// internally). Does NOT send over UDP.
    std::vector<std::vector<uint8_t>> processToUniverses();

private:
    ParamSpec mSpec;
    std::string mTargetIp = "255.255.255.255";
    int  mStartUniverse = 0;
    PixelLayout mLayout = PixelLayout::LinearHorizontal;
    PixelFormat mFormat = PixelFormat::RGB;
    int  mPixelCountX = 16;
    int  mPixelCountY = 1;
    float mGamma = 2.2f;

    std::vector<uint8_t> mSource;       // RGB source buffer
    std::vector<uint8_t> mLastReordered;// after layout reorder + format conversion
    int mLastPacketCount = 0;

    // ── v3.5.2 final: UDP transmission ───────────────────────────────────
    int  mReadbackRateHz = 30;
    std::chrono::steady_clock::time_point mLastSendTime{};
    std::chrono::steady_clock::time_point mLastReadbackTime{}; // C2 — rate-limit du readback RTT
    uint8_t mSequence = 0;

    // C2 — readback de la texture upstream (port `source_texture`) vers mSource,
    // downsamplé à mPixelCountX × mPixelCountY. Remplace l'alimentation test-only.
    void pullSourceFromUpstream();
#ifdef _WIN32
    SOCKET mSocket = INVALID_SOCKET;
    static bool sWsaInit;
    void ensureSocket();
    void closeSocket();
    void sendDmxPacket(int universe, const std::vector<uint8_t>& chunk);
#else
    void sendDmxPacket(int /*universe*/, const std::vector<uint8_t>& /*chunk*/) {}
#endif

    std::vector<uint8_t> applyLayout(const std::vector<uint8_t>& src) const;
    std::vector<uint8_t> applyFormat (const std::vector<uint8_t>& src) const;
    void                  applyGamma  (std::vector<uint8_t>& data) const;
};

} // namespace bbfx

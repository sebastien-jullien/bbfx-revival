#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

namespace bbfx {

/// DAG node that sends Art-Net DMX packets over UDP (port 6454).
///
/// ParamSpec parameters:
///   dest_ip       (string)  — destination IP (e.g. "2.0.0.1" or "255.255.255.255" for broadcast)
///   universe      (int 0-15)— Art-Net universe number
///   channel_count (int 1-512) — number of DMX channels to send
///
/// Input ports "ch1", "ch2", ... "chN" (float 0-1 per channel).
/// Sends at max 44 Hz (standard DMX refresh rate).
class ArtnetOutputNode : public AnimationNode {
public:
    explicit ArtnetOutputNode(const std::string& name = "");
    ~ArtnetOutputNode() override;
    void update() override;
    std::string getTypeName() const override { return "ArtnetOutputNode"; }

    /// Quick Assign: auto-link audio analyzer bands to ch1/ch2/ch3.
    void quickAssignAudio();

    static constexpr int ARTNET_PORT = 6454;
    static constexpr double ARTNET_INTERVAL_MS  = 22.7;   // ~44 Hz : cadence MAX d'envoi sur changement
    static constexpr double ARTNET_KEEPALIVE_MS = 1000.0; // refresh périodique même sans changement (spec Art-Net)

    /// Build a raw Art-Net DMX packet (public for testing/debugging).
    static std::vector<uint8_t> buildPacket(int universe, const std::vector<uint8_t>& data,
                                             uint8_t sequence);

private:
    void rebuildPorts(int count);
    void sendPacket(const std::string& ip, int universe, const std::vector<uint8_t>& data);

    ParamSpec mSpec;
    int mLastChannelCount = 0;

    std::vector<uint8_t> mLastData; ///< previous DMX values for delta detection
    uint8_t mSequence = 0;

    std::chrono::steady_clock::time_point mLastSend;
    bool mFirstSend = true;

#ifdef _WIN32
    SOCKET mSocket = INVALID_SOCKET;
    static bool sWsaInit;
#else
    int mSocket = -1;   // M14 — socket UDP POSIX (Linux/macOS)
#endif
    void ensureSocket();
    void closeSocket();
};

} // namespace bbfx

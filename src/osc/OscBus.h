#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sol/sol.hpp>

#include "OscMessage.h"

namespace bbfx {

class UdpServer;

/// v3.5 Lot M — singleton OSC bus used by the Lua bindings.
///
/// Owns a single UdpServer on a user-chosen port, caches the last value
/// seen at every address so `bbfx.osc.get(addr)` is O(1), and dispatches
/// incoming messages to pattern-matched Lua callbacks registered via
/// `bbfx.osc.on(pattern, cb)`.
///
/// `tick()` is the per-frame pump: it drains UdpServer::poll() and
/// invokes everything. Call from StudioApp::capture() / Engine::capture().
class OscBus {
public:
    static OscBus& instance();

    /// Start (or re-start) the UDP listener on `port`. Safe to call
    /// multiple times — subsequent calls rebind if the port changed.
    bool listen(int port);

    /// Pump — drain packets + run callbacks. One call per frame.
    void tick();

    /// Register a callback for messages whose address globs `pattern`.
    /// Returns an opaque handle usable with `off(handle)` to remove it.
    /// Pattern supports `*` (any non-slash chars) and exact matches.
    int  on(const std::string& pattern, sol::function cb);
    void off(int handle);

    /// Last value seen at `address`. Empty optional if never observed.
    std::optional<OscArg> last(const std::string& address) const;

    /// All discovered addresses (passed through UdpServer dedup).
    std::vector<std::string> discoveredAddresses() const;

    // ── MIDI learn callback bridge (v3.5 Lot M I-1412) ──────────────────
    // MidiLearnManager captures the next CC/note and populates a binding
    // but does not notify Lua. OscBus holds one pending Lua callback per
    // paramName; pumpMidiLearn() is called from StudioApp's MIDI poll loop
    // once per frame with fresh messages.
    void registerMidiLearnCallback(const std::string& paramName, sol::function cb);
    void pumpMidiLearn(const class MidiMessage* msgs, size_t n);

    // Close and release all resources (shutdown).
    void shutdown();

private:
    OscBus();
    ~OscBus();
    OscBus(const OscBus&) = delete;
    OscBus& operator=(const OscBus&) = delete;

    static bool match(const std::string& pattern, const std::string& address);

    struct Subscription {
        int handle;
        std::string pattern;
        sol::function cb;
    };

    int mCurrentPort = 0;
    std::unique_ptr<UdpServer> mServer;

    mutable std::mutex mMutex;
    std::unordered_map<std::string, OscArg> mLastValues;
    std::vector<Subscription> mSubs;
    int mNextHandle = 1;

    // MIDI learn bridge — one callback per param, fires once on next event.
    std::unordered_map<std::string, sol::function> mMidiLearnCbs;
};

} // namespace bbfx

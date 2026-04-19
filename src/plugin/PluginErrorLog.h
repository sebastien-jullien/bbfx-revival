#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string>

namespace bbfx {

/// v3.5 Lot G — Ring buffer of plugin runtime errors.
///
/// Populated by:
///   - PluginManager::onSandboxViolation (sandbox deny)
///   - PluginManager::load (init.lua error)
///   - PluginManager::enable/disable/unload (hook errors)
///
/// Consumed by PluginErrorsPanel (UI listing) and, later in Lot W, by
/// crash-recovery when a plugin has repeatedly failed.
///
/// The log is bounded: once `kMaxEntries` is reached the oldest entry is
/// dropped. Thread-safe via a mutex.
class PluginErrorLog {
public:
    enum class Severity { Info, Warning, Error };

    struct Entry {
        std::chrono::system_clock::time_point timestamp;
        std::string pluginId;
        Severity    severity;
        std::string message;
        std::string context;   // optional: e.g. "load", "enable", "sandbox"
        bool        acknowledged = false;
    };

    static PluginErrorLog& instance();

    void push(const std::string& pluginId,
              Severity severity,
              std::string message,
              std::string context = {});

    // Copy out the entries matching the filter. `pluginIdFilter` empty =
    // all plugins. `minSeverity` = Info to get everything.
    std::deque<Entry> snapshot(const std::string& pluginIdFilter = {},
                               Severity minSeverity = Severity::Info) const;

    void acknowledgeAll();
    void clear();
    size_t unacknowledgedCount() const;
    size_t totalCount() const;

    static constexpr size_t kMaxEntries = 256;

    static const char* severityName(Severity s);

private:
    PluginErrorLog() = default;
    mutable std::mutex mMtx;
    std::deque<Entry> mEntries;
};

} // namespace bbfx

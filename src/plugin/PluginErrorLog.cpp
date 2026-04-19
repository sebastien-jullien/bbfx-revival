#include "plugin/PluginErrorLog.h"

namespace bbfx {

PluginErrorLog& PluginErrorLog::instance() {
    static PluginErrorLog inst;
    return inst;
}

void PluginErrorLog::push(const std::string& pluginId, Severity severity,
                          std::string message, std::string context) {
    std::lock_guard<std::mutex> lk(mMtx);
    if (mEntries.size() >= kMaxEntries) mEntries.pop_front();
    Entry e;
    e.timestamp = std::chrono::system_clock::now();
    e.pluginId  = pluginId;
    e.severity  = severity;
    e.message   = std::move(message);
    e.context   = std::move(context);
    mEntries.push_back(std::move(e));
}

std::deque<PluginErrorLog::Entry> PluginErrorLog::snapshot(const std::string& pluginIdFilter,
                                                            Severity minSeverity) const {
    std::lock_guard<std::mutex> lk(mMtx);
    if (pluginIdFilter.empty() && minSeverity == Severity::Info) {
        return mEntries;
    }
    std::deque<Entry> out;
    for (const auto& e : mEntries) {
        if (!pluginIdFilter.empty() && e.pluginId != pluginIdFilter) continue;
        if (static_cast<int>(e.severity) < static_cast<int>(minSeverity)) continue;
        out.push_back(e);
    }
    return out;
}

void PluginErrorLog::acknowledgeAll() {
    std::lock_guard<std::mutex> lk(mMtx);
    for (auto& e : mEntries) e.acknowledged = true;
}

void PluginErrorLog::clear() {
    std::lock_guard<std::mutex> lk(mMtx);
    mEntries.clear();
}

size_t PluginErrorLog::unacknowledgedCount() const {
    std::lock_guard<std::mutex> lk(mMtx);
    size_t n = 0;
    for (const auto& e : mEntries) if (!e.acknowledged) ++n;
    return n;
}

size_t PluginErrorLog::totalCount() const {
    std::lock_guard<std::mutex> lk(mMtx);
    return mEntries.size();
}

const char* PluginErrorLog::severityName(Severity s) {
    switch (s) {
        case Severity::Info:    return "Info";
        case Severity::Warning: return "Warning";
        case Severity::Error:   return "Error";
    }
    return "?";
}

} // namespace bbfx

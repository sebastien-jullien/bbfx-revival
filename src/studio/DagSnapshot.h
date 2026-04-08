#pragma once
#include <string>
#include <map>
#include <functional>

namespace bbfx {

class Animator;

/// Captures and interpolates the complete state of the DAG for crossfader A/B.
/// Format: map<"nodeName.portName", float> — compatible with ChordBlock::snapshot.
class DagSnapshot {
public:
    /// Capture ALL float port values from all registered nodes.
    void capture(Animator& animator);

    /// Interpolate between two snapshots and apply to DAG.
    /// t = 0.0 means 100% A, t = 1.0 means 100% B.
    /// Ports with active automation (if automationPortCheck provided) are skipped.
    static void apply(const DagSnapshot& a, const DagSnapshot& b, float t,
                      Animator& animator,
                      std::function<bool(const std::string&, const std::string&)> isAutomated = nullptr);

    const std::map<std::string, float>& getData() const { return mData; }
    void setData(const std::map<std::string, float>& data) { mData = data; }
    bool empty() const { return mData.empty(); }
    void clear() { mData.clear(); }

private:
    std::map<std::string, float> mData; // "nodeName.portName" → value
};

} // namespace bbfx

#include "DagSnapshot.h"
#include "../core/Animator.h"
#include "../core/AnimationNode.h"
#include "../core/AnimationPort.h"

#include <functional>

namespace bbfx {

void DagSnapshot::capture(Animator& animator) {
    mData.clear();
    auto names = animator.getRegisteredNodeNames();
    for (auto& nodeName : names) {
        auto* node = animator.getRegisteredNode(nodeName);
        if (!node) continue;
        for (auto& [portName, port] : node->getInputs()) {
            std::string key = nodeName + "." + portName;
            mData[key] = port->getValue();
        }
    }
}

void DagSnapshot::apply(const DagSnapshot& a, const DagSnapshot& b, float t,
                         Animator& animator,
                         std::function<bool(const std::string&, const std::string&)> isAutomated) {
    // Collect all keys from both snapshots
    std::map<std::string, float> result;

    for (auto& [key, valA] : a.mData) {
        auto itB = b.mData.find(key);
        if (itB != b.mData.end()) {
            // Key in both: lerp
            result[key] = valA * (1.0f - t) + itB->second * t;
        } else {
            // Key only in A: keep A's value (no snap to 0)
            result[key] = valA;
        }
    }

    for (auto& [key, valB] : b.mData) {
        if (a.mData.find(key) == a.mData.end()) {
            // Key only in B: keep B's value (no snap from 0)
            result[key] = valB;
        }
    }

    // Apply to DAG
    for (auto& [key, value] : result) {
        // Parse "nodeName.portName"
        auto dotPos = key.find('.');
        if (dotPos == std::string::npos) continue;
        std::string nodeName = key.substr(0, dotPos);
        std::string portName = key.substr(dotPos + 1);

        // Skip automated ports
        if (isAutomated && isAutomated(nodeName, portName)) continue;

        auto* node = animator.getRegisteredNode(nodeName);
        if (!node) continue;
        auto& inputs = node->getInputs();
        auto it = inputs.find(portName);
        if (it != inputs.end()) {
            it->second->setValue(value);
        }
    }
}

} // namespace bbfx

#include "AutomationEngine.h"
#include "AutomationData.h"
#include "Animator.h"
#include "AnimationNode.h"
#include "AnimationPort.h"

namespace bbfx {

AutomationEngine::AutomationEngine(AutomationData* data)
    : mData(data) {}

void AutomationEngine::evaluate(float currentBeat, Animator* animator) {
    if (!mEnabled || !mData || !animator) return;

    for (const auto& lane : mData->lanes) {
        if (lane.muted) continue;
        if (lane.nodeName.empty() || lane.portName.empty()) continue;
        if (lane.keyframes.empty()) continue;

        auto* node = animator->getRegisteredNode(lane.nodeName);
        if (!node) continue;
        // Node figé (port `enabled` < 0.5) : ne pas piloter ses autres ports (sans
        // effet tant qu'il est off) — mais on autorise l'automation du port `enabled`
        // lui-même pour pouvoir le réactiver depuis la timeline.
        if (!node->isEnabled() && lane.portName != "enabled") continue;

        auto& inputs = node->getInputs();
        auto it = inputs.find(lane.portName);
        if (it == inputs.end()) continue;

        float value = mData->evaluate(lane, currentBeat);
        it->second->setValue(value);
    }
}

} // namespace bbfx

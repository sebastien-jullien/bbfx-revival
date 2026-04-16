#include "BlendNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/Engine.h"
#include "../StudioEngine.h"
#include <algorithm>

namespace bbfx {

BlendNode::BlendNode(const std::string& name) : AnimationNode(name) {
    { ParamDef d; d.name = "output_id"; d.label = "Output ID"; d.type = ParamType::INT;
      d.intVal = 0; d.minVal = 0.f; d.maxVal = 7.f; mSpec.addParam(d); }
    setParamSpec(&mSpec);

    addInput(new AnimationPort("output_id", 0.0f));
    addInput(new AnimationPort("left",   0.0f));
    addInput(new AnimationPort("right",  0.0f));
    addInput(new AnimationPort("top",    0.0f));
    addInput(new AnimationPort("bottom", 0.0f));
    addInput(new AnimationPort("gamma",  2.2f));
}

void BlendNode::update() {
    auto& in = getInputs();
    auto safeGet = [&](const char* name, float def) -> float {
        auto it = in.find(name);
        return it != in.end() ? it->second->getValue() : def;
    };
    int outputId = static_cast<int>(safeGet("output_id", 0.0f));

    auto* engine = dynamic_cast<StudioEngine*>(Engine::instance());
    if (!engine) return;
    auto* mgr = engine->getOutputManager();
    if (!mgr) return;
    auto* slot = mgr->getSlot(outputId);
    if (!slot) return;

    slot->blendProfile.left   = std::clamp(safeGet("left",   0.0f), 0.0f, 0.5f);
    slot->blendProfile.right  = std::clamp(safeGet("right",  0.0f), 0.0f, 0.5f);
    slot->blendProfile.top    = std::clamp(safeGet("top",    0.0f), 0.0f, 0.5f);
    slot->blendProfile.bottom = std::clamp(safeGet("bottom", 0.0f), 0.0f, 0.5f);
    slot->blendProfile.gamma  = std::clamp(safeGet("gamma",  2.2f), 1.0f, 3.0f);

    if (slot->blendEnabled) {
        mgr->updateBlendParams(outputId);
    }

    fireUpdate();
}

} // namespace bbfx

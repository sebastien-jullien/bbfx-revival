#include "WarpNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/Engine.h"
#include "../StudioEngine.h"
#include <algorithm>

namespace bbfx {

WarpNode::WarpNode(const std::string& name) : AnimationNode(name) {
    { ParamDef d; d.name = "output_id"; d.label = "Output ID"; d.type = ParamType::INT;
      d.intVal = 0; d.minVal = 0.f; d.maxVal = 7.f; mSpec.addParam(d); }
    setParamSpec(&mSpec);

    addInput(new AnimationPort("output_id", 0.0f));
    addInput(new AnimationPort("tl_x", 0.0f));
    addInput(new AnimationPort("tl_y", 0.0f));
    addInput(new AnimationPort("tr_x", 1.0f));
    addInput(new AnimationPort("tr_y", 0.0f));
    addInput(new AnimationPort("bl_x", 0.0f));
    addInput(new AnimationPort("bl_y", 1.0f));
    addInput(new AnimationPort("br_x", 1.0f));
    addInput(new AnimationPort("br_y", 1.0f));
}

void WarpNode::update() {
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

    float* c = slot->warpProfile.corners;
    c[0] = std::clamp(safeGet("tl_x", 0.0f), 0.0f, 1.0f);
    c[1] = std::clamp(safeGet("tl_y", 0.0f), 0.0f, 1.0f);
    c[2] = std::clamp(safeGet("tr_x", 1.0f), 0.0f, 1.0f);
    c[3] = std::clamp(safeGet("tr_y", 0.0f), 0.0f, 1.0f);
    c[4] = std::clamp(safeGet("bl_x", 0.0f), 0.0f, 1.0f);
    c[5] = std::clamp(safeGet("bl_y", 1.0f), 0.0f, 1.0f);
    c[6] = std::clamp(safeGet("br_x", 1.0f), 0.0f, 1.0f);
    c[7] = std::clamp(safeGet("br_y", 1.0f), 0.0f, 1.0f);

    if (slot->warpEnabled) {
        mgr->updateWarpParams(outputId);
    }

    fireUpdate();
}

} // namespace bbfx

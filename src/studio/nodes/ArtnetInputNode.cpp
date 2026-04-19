#include "ArtnetInputNode.h"

#include <algorithm>
#include <iostream>

#include "../../core/AnimationPort.h"
#include "../../network/ArtnetInput.h"

namespace bbfx {

ArtnetInputNode::ArtnetInputNode(const std::string& name)
    : AnimationNode(name) {
    { ParamDef d; d.name = "universe"; d.label = "Universe"; d.type = ParamType::INT;
      d.intVal = 0; d.minVal = 0.f; d.maxVal = 15.f; mSpec.addParam(d); }
    { ParamDef d; d.name = "channel_count"; d.label = "Channel Count"; d.type = ParamType::INT;
      d.intVal = 8; d.minVal = 1.f; d.maxVal = 512.f; mSpec.addParam(d); }
    setParamSpec(&mSpec);

    rebuildPorts(8);

    // Auto-start the listener on first node instantiation.
    ArtnetInput::instance().start();
    std::cout << "[ArtnetInput] Node created: " << name << std::endl;
}

void ArtnetInputNode::rebuildPorts(int count) {
    for (int i = mLastChannelCount + 1; i <= count; ++i) {
        std::string portName = "ch" + std::to_string(i);
        auto& outputs = getOutputs();
        if (outputs.find(portName) == outputs.end()) {
            addOutput(new AnimationPort(portName, 0.0f));
        }
    }
    mLastChannelCount = std::max(mLastChannelCount, count);
}

void ArtnetInputNode::update() {
    auto* pUni = mSpec.getParam("universe");
    auto* pCnt = mSpec.getParam("channel_count");
    int universe = pUni ? std::clamp(pUni->intVal, 0, 15) : 0;
    int count    = pCnt ? std::clamp(pCnt->intVal, 1, 512) : 8;

    if (count != mLastChannelCount) rebuildPorts(count);

    auto data = ArtnetInput::instance().channels(universe);
    auto& outputs = getOutputs();
    for (int i = 1; i <= count; ++i) {
        auto it = outputs.find("ch" + std::to_string(i));
        if (it != outputs.end()) {
            it->second->setValue(data[i - 1] / 255.0f);
        }
    }
}

} // namespace bbfx

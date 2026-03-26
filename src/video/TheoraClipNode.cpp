#include "TheoraClipNode.h"

namespace bbfx {

TheoraClipNode::TheoraClipNode(const std::string& filename)
    : AnimationNode("TheoraClipNode")
    , mClip(std::make_unique<TheoraClip>(filename))
{
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("time_control", 0.0f));
    addOutput(new AnimationPort("playing", 0.0f));
    addOutput(new AnimationPort("frame_ready", 0.0f));
}

TheoraClipNode::~TheoraClipNode() = default;

void TheoraClipNode::update() {
    auto& inputs = getInputs();
    auto& outputs = getOutputs();

    float dt = inputs.at("dt")->getValue();
    mClip->frameUpdate(dt);

    outputs.at("playing")->setValue(mClip->isPlaying() ? 1.0f : 0.0f);
    outputs.at("frame_ready")->setValue(mClip->isFrameReady() ? 1.0f : 0.0f);

    fireUpdate();
}

} // namespace bbfx

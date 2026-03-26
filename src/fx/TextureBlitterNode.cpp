#include "TextureBlitterNode.h"

namespace bbfx {

TextureBlitterNode::TextureBlitterNode(const string& textureName)
    : AnimationNode("TextureBlitterNode")
    , mBlitter(std::make_unique<TextureBlitter>(textureName))
{
    addInput(new AnimationPort("r", 1.0f));
    addInput(new AnimationPort("g", 1.0f));
    addInput(new AnimationPort("b", 1.0f));
    addInput(new AnimationPort("a", 1.0f));
    addInput(new AnimationPort("pattern", 0.0f)); // 0=solid (default)
    addOutput(new AnimationPort("texture_dirty", 0.0f));
}

TextureBlitterNode::~TextureBlitterNode() = default;

void TextureBlitterNode::update() {
    mBlitter->blit();
    getOutputs().at("texture_dirty")->setValue(1.0f);
    fireUpdate();
}

} // namespace bbfx

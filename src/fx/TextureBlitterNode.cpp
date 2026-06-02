#include "TextureBlitterNode.h"

namespace bbfx {

TextureBlitterNode::TextureBlitterNode(const string& textureName, const std::string& nodeName)
    : AnimationNode(nodeName)
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
    // M5 — les ports r/g/b/a/pattern pilotent désormais le remplissage
    // (avant : blit() peignait du bleu codé en dur, ports ignorés).
    auto& in = getInputs();
    float r = in.at("r")->getValue();
    float g = in.at("g")->getValue();
    float b = in.at("b")->getValue();
    float a = in.at("a")->getValue();
    int pattern = static_cast<int>(in.at("pattern")->getValue() + 0.5f);
    mBlitter->blit(r, g, b, a, pattern);
    getOutputs().at("texture_dirty")->setValue(1.0f);
    fireUpdate();
}

} // namespace bbfx

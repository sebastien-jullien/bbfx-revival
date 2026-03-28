#pragma once

#include "../core/AnimationNode.h"
#include "TextureBlitter.h"
#include <memory>

namespace bbfx {

class TextureBlitterNode : public AnimationNode {
public:
    explicit TextureBlitterNode(const string& textureName);
    virtual ~TextureBlitterNode();
    void update() override;
    std::string getTypeName() const override { return "TextureBlitterNode"; }

private:
    std::unique_ptr<TextureBlitter> mBlitter;
};

} // namespace bbfx

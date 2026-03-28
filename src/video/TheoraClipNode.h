#pragma once

#include "../core/AnimationNode.h"
#include "TheoraClip.h"
#include <memory>

namespace bbfx {

class TheoraClipNode : public AnimationNode {
public:
    explicit TheoraClipNode(const std::string& filename);
    ~TheoraClipNode() override;
    void update() override;
    std::string getTypeName() const override { return "TheoraClipNode"; }

    void play() { mClip->play(); }
    void pause() { mClip->pause(); }
    void stop() { mClip->stop(); }

    TheoraClip* getClip() { return mClip.get(); }

private:
    std::unique_ptr<TheoraClip> mClip;
};

} // namespace bbfx

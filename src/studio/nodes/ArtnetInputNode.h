#pragma once

#include <string>

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// v3.5 Lot M — DAG node that exposes the state of an Art-Net universe
/// received on the shared ArtnetInput singleton as output ports ch1..chN
/// (float 0..1 each).
///
/// Params :
///   universe      (int 0-15) — which universe to sample
///   channel_count (int 1-512)
class ArtnetInputNode : public AnimationNode {
public:
    explicit ArtnetInputNode(const std::string& name = "");
    ~ArtnetInputNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "ArtnetInputNode"; }

private:
    void rebuildPorts(int count);

    ParamSpec mSpec;
    int mLastChannelCount = 0;
};

} // namespace bbfx

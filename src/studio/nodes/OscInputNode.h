#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <memory>

namespace bbfx { class UdpServer; }

namespace bbfx {

/// DAG node that receives OSC messages and exposes float values as output ports.
/// Listens on a configurable UDP port, filters by address pattern.
class OscInputNode : public AnimationNode {
public:
    explicit OscInputNode(const std::string& name);
    ~OscInputNode();
    void update() override;
    std::string getTypeName() const override { return "OscInputNode"; }

private:
    ParamSpec mSpec;
    std::shared_ptr<UdpServer> mServer;
    int mCurrentPort = 0;
};

} // namespace bbfx

#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node that sends video frames via NDI (NewTek Network Device Interface).
/// Requires the NDI SDK to be installed. When BBFX_HAS_NDI is not defined,
/// the node registers but does nothing (graceful no-op).
class NdiOutputNode : public AnimationNode {
public:
    explicit NdiOutputNode(const std::string& name);
    ~NdiOutputNode() override;
    void update() override;
    std::string getTypeName() const override { return "NdiOutputNode"; }

private:
    ParamSpec mSpec;
    bool mInitialized = false;
    void* mNdiSender = nullptr;  // NDIlib_send_instance_t (opaque when SDK absent)
    int mFrameCount = 0;
};

} // namespace bbfx

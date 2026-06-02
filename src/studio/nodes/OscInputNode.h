#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <memory>
#include <string>
#include <vector>

namespace bbfx { class UdpServer; }

namespace bbfx {

/// N4 — file des presets demandés via OSC (`/bbfx/preset/load/<name>`). Drainée
/// par la boucle principale du Studio (qui appelle dbg.preset). Avant, le handler
/// OSC ne faisait que pulser un trigger sans charger le preset nommé.
extern std::vector<std::string> gPendingOscPresetLoads;

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

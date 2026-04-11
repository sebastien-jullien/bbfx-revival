#include "OscOutputNode.h"
#include "../../network/UdpServer.h"
#include "../../osc/OscMessage.h"
#include "../../core/AnimationPort.h"
#include <cmath>

namespace bbfx {

OscOutputNode::OscOutputNode(const std::string& name) : AnimationNode(name) {
    for (int i = 1; i <= 8; ++i)
        addInput(new AnimationPort("value" + std::to_string(i), 0.0f));
    addInput(new AnimationPort("send", 0.0f)); // trigger send
    addInput(new AnimationPort("dt", 0.016f));

    { ParamDef d; d.name = "target_ip"; d.type = ParamType::STRING; d.stringVal = "127.0.0.1"; d.label = "Target IP"; mSpec.addParam(d); }
    { ParamDef d; d.name = "target_port"; d.type = ParamType::INT; d.intVal = 9001; d.minVal = 1024; d.maxVal = 65535; d.label = "Target Port"; mSpec.addParam(d); }
    { ParamDef d; d.name = "address"; d.type = ParamType::STRING; d.stringVal = "/bbfx/out"; d.label = "OSC Address"; mSpec.addParam(d); }
    { ParamDef d; d.name = "max_rate"; d.type = ParamType::INT; d.intVal = 60; d.minVal = 1; d.maxVal = 120; d.label = "Max Send Rate"; mSpec.addParam(d); }

    setParamSpec(&mSpec);
}

void OscOutputNode::update() {
    auto& ins = getInputs();

    // Rate limiting
    auto* rateParam = mSpec.getParam("max_rate");
    int maxRate = rateParam ? rateParam->intVal : 60;
    if (++mSendCounter < (60 / maxRate)) return;
    mSendCounter = 0;

    // Check if any value changed
    bool changed = false;
    for (int i = 0; i < 8; ++i) {
        float val = ins["value" + std::to_string(i + 1)]->getValue();
        if (std::abs(val - mLastValues[i]) > 0.001f) {
            mLastValues[i] = val;
            changed = true;
        }
    }

    // Also check explicit send trigger
    float sendTrig = ins["send"]->getValue();

    if (!changed && sendTrig < 0.5f) return;

    // Build and send OSC message
    auto* ipParam = mSpec.getParam("target_ip");
    auto* portParam = mSpec.getParam("target_port");
    auto* addrParam = mSpec.getParam("address");

    std::string ip = ipParam ? ipParam->stringVal : "127.0.0.1";
    int port = portParam ? portParam->intVal : 9001;
    std::string addr = addrParam ? addrParam->stringVal : "/bbfx/out";

    OscMessage msg;
    msg.address = addr;
    for (int i = 0; i < 8; ++i) {
        msg.args.push_back(mLastValues[i]);
    }

    UdpServer::send(ip, port, msg);
    fireUpdate();
}

} // namespace bbfx

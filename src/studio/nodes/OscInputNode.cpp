#include "OscInputNode.h"
#include "../../network/UdpServer.h"
#include "../../osc/OscMessage.h"
#include "../../core/AnimationPort.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include <iostream>
#include <algorithm>

namespace bbfx {

// N4 — file des presets demandés via OSC, drainée par la boucle principale.
std::vector<std::string> gPendingOscPresetLoads;

OscInputNode::OscInputNode(const std::string& name) : AnimationNode(name) {
    // 8 float value outputs
    for (int i = 1; i <= 8; ++i)
        addOutput(new AnimationPort("value" + std::to_string(i), 0.0f));
    addOutput(new AnimationPort("trigger", 0.0f));
    addInput(new AnimationPort("dt", 0.016f));

    // ParamSpec
    { ParamDef d; d.name = "port"; d.type = ParamType::INT; d.intVal = 9000; d.minVal = 1024; d.maxVal = 65535; d.label = "UDP Port"; mSpec.addParam(d); }
    { ParamDef d; d.name = "address"; d.type = ParamType::STRING; d.stringVal = "/bbfx/*"; d.label = "Address Filter"; mSpec.addParam(d); }

    setParamSpec(&mSpec);
}

OscInputNode::~OscInputNode() {
    if (mServer) mServer->stop();
}

// Simple glob match: * matches any substring, ? matches one char
static bool globMatch(const std::string& pattern, const std::string& str) {
    size_t pi = 0, si = 0, starP = std::string::npos, starS = 0;
    while (si < str.size()) {
        if (pi < pattern.size() && (pattern[pi] == str[si] || pattern[pi] == '?')) { ++pi; ++si; }
        else if (pi < pattern.size() && pattern[pi] == '*') { starP = pi++; starS = si; }
        else if (starP != std::string::npos) { pi = starP + 1; si = ++starS; }
        else return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

void OscInputNode::update() {
    auto* portParam = mSpec.getParam("port");
    int port = portParam ? portParam->intVal : 9000;

    // Start/restart server if port changed
    if (port != mCurrentPort) {
        if (mServer) mServer->stop();
        mServer = std::make_shared<UdpServer>();
        mServer->start(port);
        mCurrentPort = port;
    }

    if (!mServer) return;

    auto* addrParam = mSpec.getParam("address");
    std::string addrFilter = addrParam ? addrParam->stringVal : "/bbfx/*";

    auto messages = mServer->poll();
    auto& outs = getOutputs();

    // Reset trigger (it's a one-frame pulse)
    outs["trigger"]->setValue(0.0f);

    for (auto& msg : messages) {
        // Address filter
        if (!addrFilter.empty() && !globMatch(addrFilter, msg.address)) continue;

        // ── OSC command dispatch (v3.3) ──────────────────────────────────
        // /bbfx/fader/<N> <float> — set fader N value (0.0-1.0)
        // /bbfx/set/<node>/<port> <float> — set DAG port value
        // /bbfx/node/enable/<name> — enable node
        // /bbfx/node/disable/<name> — disable node
        auto* animator = Animator::instance();
        if (animator) {
            // /bbfx/set/<node>/<port> <float>
            if (msg.address.rfind("/bbfx/set/", 0) == 0 && msg.address.size() > 10) {
                std::string rest = msg.address.substr(10);
                auto slash = rest.find('/');
                if (slash != std::string::npos) {
                    std::string nodeName = rest.substr(0, slash);
                    std::string portName = rest.substr(slash + 1);
                    auto* node = animator->getRegisteredNode(nodeName);
                    if (node && !msg.args.empty()) {
                        auto& inputs = node->getInputs();
                        auto it = inputs.find(portName);
                        if (it != inputs.end()) {
                            it->second->setValue(msg.getFloat(0));
                        }
                    }
                }
            }
            // /bbfx/node/enable/<name>
            else if (msg.address.rfind("/bbfx/node/enable/", 0) == 0) {
                std::string name = msg.address.substr(18);
                auto* node = animator->getRegisteredNode(name);
                if (node) node->setEnabled(true);
            }
            // /bbfx/node/disable/<name>
            else if (msg.address.rfind("/bbfx/node/disable/", 0) == 0) {
                std::string name = msg.address.substr(19);
                auto* node = animator->getRegisteredNode(name);
                if (node) node->setEnabled(false);
            }
            // /bbfx/preset/load/<name> — load preset by name
            else if (msg.address.rfind("/bbfx/preset/load/", 0) == 0) {
                std::string presetName = msg.address.substr(18);
                std::cout << "[OSC] Preset recall: " << presetName << std::endl;
                // N4 — empile la demande ; la boucle principale du Studio la draine et
                // appelle dbg.preset(name) (le chargement touche le graphe → main-thread).
                if (!presetName.empty()) gPendingOscPresetLoads.push_back(presetName);
                outs["trigger"]->setValue(1.0f);
            }
            // /bbfx/bpm <float> — set BPM
            else if (msg.address == "/bbfx/bpm" && !msg.args.empty()) {
                float bpm = msg.getFloat(0);
                if (bpm >= 20.0f && bpm <= 300.0f) {
                    auto* rootTime = dynamic_cast<AnimationNode*>(animator->getRegisteredNode("__root_time"));
                    if (rootTime) {
                        auto& ins = rootTime->getInputs();
                        auto it = ins.find("bpm");
                        if (it != ins.end()) it->second->setValue(bpm);
                    }
                }
            }
        }

        // ── Standard value routing ───────────────────────────────────────
        if (msg.args.empty()) {
            // Bang: set trigger to 1.0 for one frame
            outs["trigger"]->setValue(1.0f);
        } else {
            // Route float args to value1..value8
            for (size_t i = 0; i < msg.args.size() && i < 8; ++i) {
                float val = msg.getFloat(i);
                outs["value" + std::to_string(i + 1)]->setValue(val);
            }
        }
    }

    fireUpdate();
}

} // namespace bbfx

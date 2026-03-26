#include "bbfx_bindings.h"
#include <filesystem>
#include <chrono>
#include "../core/Engine.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../core/StatsOverlay.h"
#include "../input/InputManager.h"
#include "../input/StdinReader.h"
#include "../fx/PerlinVertexShader.h"
#include "../fx/PerlinFxNode.h"
#include "../fx/TextureBlitter.h"
#include "../fx/TextureBlitterNode.h"
#include "../fx/WaveVertexShader.h"
#include "../fx/ColorShiftNode.h"
#include "../video/TheoraClip.h"
#include "../video/ReversableClip.h"
#include "../video/TextureCrossfader.h"
#include "../video/TheoraClipNode.h"
#include "../network/TcpServer.h"

namespace bbfx {

void register_bbfx_bindings(sol::state& lua) {
    auto bbfx = lua.create_named_table("bbfx");

    // ── I-028: Engine bindings ──────────────────────────────────────────
    lua.new_usertype<Engine>("bbfx_Engine",
        sol::no_constructor,
        "instance", []() -> Engine* { return Engine::instance(); },
        "startRendering", &Engine::startRendering,
        "stopRendering", &Engine::stopRendering,
        "getSceneManager", &Engine::getSceneManager,
        "getRenderWindow", &Engine::getRenderWindow,
        "screenshot", &Engine::screenshot,
        "toggleFullscreen", &Engine::toggleFullscreen
    );
    bbfx["Engine"] = lua["bbfx_Engine"];

    // ── StatsOverlay bindings ────────────────────────────────────────────
    lua.new_usertype<StatsOverlay>("bbfx_StatsOverlay",
        sol::no_constructor,
        "instance", []() -> StatsOverlay* { return StatsOverlay::instance(); },
        "toggle", &StatsOverlay::toggle,
        "show", &StatsOverlay::show,
        "hide", &StatsOverlay::hide,
        "isVisible", &StatsOverlay::isVisible
    );
    bbfx["StatsOverlay"] = lua["bbfx_StatsOverlay"];

    // ── I-029: Animator bindings ────────────────────────────────────────
    lua.new_usertype<Animator>("bbfx_Animator",
        sol::no_constructor,
        "instance", []() -> Animator* { return Animator::instance(); },
        "addNode", [](Animator& self, AnimationNode* node) {
            // Register all ports of the node
            for (auto& [name, port] : node->getInputs()) {
                self.add(port);
            }
            for (auto& [name, port] : node->getOutputs()) {
                self.add(port);
            }
            node->setListener(&self);
            self.registerNode(node);
        },
        "addPort", [](Animator& self, AnimationNode* nodeA, const std::string& outputName,
                       AnimationNode* nodeB, const std::string& inputName) {
            auto& outputs = nodeA->getOutputs();
            auto& inputs = nodeB->getInputs();
            auto oi = outputs.find(outputName);
            auto ii = inputs.find(inputName);
            if (oi != outputs.end() && ii != inputs.end()) {
                self.link(oi->second, ii->second);
            }
        },
        "removeNode", &Animator::removeNode,
        "getNodeNames", [](Animator& self) -> std::vector<std::string> {
            // Collect unique node names from all registered ports
            std::set<std::string> names;
            // Iterate through the vertex/port maps via the graph
            // Since we can't access protected members, we collect from known nodes
            // This uses the port map: each port belongs to a node
            // We'll expose a helper that tracks registered nodes
            return self.getRegisteredNodeNames();
        },
        "getNodeByName", [](Animator& self, const std::string& name) -> AnimationNode* {
            return self.getRegisteredNode(name);
        },
        "exportDOT", &Animator::exportDOT,
        "renderOneFrame", &Animator::renderOneFrame,
        "setPreOp", [](Animator& self, bool isLink, AnimationPort* from, AnimationPort* to, float time) {
            self.schedule(Operation(isLink, from, to), time);
        },
        "setPostOp", [](Animator& /*self*/) {
            // Post-ops are handled internally by the Animator
        }
    );
    bbfx["Animator"] = lua["bbfx_Animator"];

    // ── I-055: AnimationPort bindings (must be before derived node types) ─
    lua.new_usertype<AnimationPort>("bbfx_AnimationPort",
        sol::no_constructor,
        "getName", &AnimationPort::getName,
        "getFullName", &AnimationPort::getFullName,
        "getValue", &AnimationPort::getValue,
        "setValue", &AnimationPort::setValue,
        "getNode", &AnimationPort::getNode
    );
    bbfx["AnimationPort"] = lua["bbfx_AnimationPort"];

    // AnimationNode base (must be registered before derived types for sol::bases)
    lua.new_usertype<AnimationNode>("bbfx_AnimationNode",
        sol::no_constructor,
        "getName", &AnimationNode::getName,
        "update", &AnimationNode::update,
        "getOutput", [](AnimationNode& self, const std::string& name) -> AnimationPort* {
            auto& outputs = self.getOutputs();
            auto it = outputs.find(name);
            return (it != outputs.end()) ? it->second : nullptr;
        },
        "getInput", [](AnimationNode& self, const std::string& name) -> AnimationPort* {
            auto& inputs = self.getInputs();
            auto it = inputs.find(name);
            return (it != inputs.end()) ? it->second : nullptr;
        },
        "getInputNames", [](AnimationNode& self) -> std::vector<std::string> {
            std::vector<std::string> names;
            for (auto& [name, _] : self.getInputs()) names.push_back(name);
            return names;
        },
        "getOutputNames", [](AnimationNode& self) -> std::vector<std::string> {
            std::vector<std::string> names;
            for (auto& [name, _] : self.getOutputs()) names.push_back(name);
            return names;
        }
    );
    bbfx["AnimationNode"] = lua["bbfx_AnimationNode"];

    // ── I-030: Animation node bindings ──────────────────────────────────
    lua.new_usertype<RootTimeNode>("bbfx_RootTimeNode",
        sol::call_constructor, sol::constructors<RootTimeNode()>(),
        "instance", []() -> RootTimeNode* { return RootTimeNode::instance(); },
        "reset", &RootTimeNode::reset,
        "getTotalTime", &RootTimeNode::getTotalTime,
        "update", &RootTimeNode::update,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["RootTimeNode"] = lua["bbfx_RootTimeNode"];

    lua.new_usertype<LuaAnimationNode>("bbfx_LuaAnimationNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name, sol::function update) {
                return new LuaAnimationNode(name, std::move(update));
            }
        ),
        "addInput", &LuaAnimationNode::addInput,
        "addOutput", &LuaAnimationNode::addOutput,
        "update", &LuaAnimationNode::update,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["LuaAnimationNode"] = lua["bbfx_LuaAnimationNode"];

    lua.new_usertype<AccumulatorNode>("bbfx_AccumulatorNode",
        sol::call_constructor, sol::constructors<AccumulatorNode()>(),
        "update", &AccumulatorNode::update,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["AccumulatorNode"] = lua["bbfx_AccumulatorNode"];

    lua.new_usertype<AnimationStateNode>("bbfx_AnimationStateNode",
        sol::no_constructor,
        "update", &AnimationStateNode::update,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["AnimationStateNode"] = lua["bbfx_AnimationStateNode"];

    // ── I-033 to I-036: Input bindings ──────────────────────────────────
    lua.new_usertype<KeyboardManager>("bbfx_KeyboardManager",
        sol::no_constructor,
        "isKeyDown", &KeyboardManager::isKeyDown,
        "wasKeyPressed", &KeyboardManager::wasKeyPressed
    );

    lua.new_usertype<MouseManager>("bbfx_MouseManager",
        sol::no_constructor,
        "getX", &MouseManager::getX,
        "getY", &MouseManager::getY,
        "getDX", &MouseManager::getDX,
        "getDY", &MouseManager::getDY,
        "isButtonDown", &MouseManager::isButtonDown
    );

    lua.new_usertype<JoystickManager>("bbfx_JoystickManager",
        sol::no_constructor,
        "getAxisValue", &JoystickManager::getAxisValue,
        "isButtonDown", &JoystickManager::isButtonDown,
        "getCount", &JoystickManager::getCount
    );

    lua.new_usertype<InputManager>("bbfx_InputManager",
        sol::no_constructor,
        "instance", []() -> InputManager* { return InputManager::instance(); },
        "getKeyboard", &InputManager::getKeyboard,
        "getMouse", &InputManager::getMouse,
        "getJoystick", &InputManager::getJoystick,
        "capture", &InputManager::capture
    );
    bbfx["InputManager"] = lua["bbfx_InputManager"];
    bbfx["KeyboardManager"] = lua["bbfx_KeyboardManager"];
    bbfx["MouseManager"] = lua["bbfx_MouseManager"];
    bbfx["JoystickManager"] = lua["bbfx_JoystickManager"];

    // ── I-065: FX Lua bindings ──────────────────────────────────────────
    lua.new_usertype<PerlinVertexShader>("bbfx_PerlinVertexShader",
        sol::call_constructor, sol::factories(
            [](const std::string& meshName, const std::string& cloneName) {
                return new PerlinVertexShader(meshName, cloneName);
            }
        ),
        "enable", &PerlinVertexShader::enable,
        "disable", &PerlinVertexShader::disable,
        "renderOneFrame", &PerlinVertexShader::renderOneFrame
    );
    bbfx["PerlinVertexShader"] = lua["bbfx_PerlinVertexShader"];

    lua.new_usertype<TextureBlitter>("bbfx_TextureBlitter",
        sol::call_constructor, sol::factories(
            [](const std::string& name) { return new TextureBlitter(name); }
        ),
        "blit", &TextureBlitter::blit
    );
    bbfx["TextureBlitter"] = lua["bbfx_TextureBlitter"];

    // ── PerlinFxNode (AnimationNode wrapper) ─────────────────────────────
    lua.new_usertype<PerlinFxNode>("bbfx_PerlinFxNode",
        sol::call_constructor, sol::factories(
            [](const std::string& meshName, const std::string& cloneName) {
                return new PerlinFxNode(meshName, cloneName);
            }
        ),
        "enable", &PerlinFxNode::enable,
        "disable", &PerlinFxNode::disable,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["PerlinFxNode"] = lua["bbfx_PerlinFxNode"];

    // ── TextureBlitterNode (AnimationNode wrapper) ───────────────────────
    lua.new_usertype<TextureBlitterNode>("bbfx_TextureBlitterNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name) { return new TextureBlitterNode(name); }
        ),
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["TextureBlitterNode"] = lua["bbfx_TextureBlitterNode"];

    // ── WaveVertexShader (AnimationNode + SoftwareVertexShader) ──────────
    lua.new_usertype<WaveVertexShader>("bbfx_WaveVertexShader",
        sol::call_constructor, sol::factories(
            [](const std::string& meshName, const std::string& cloneName) {
                return new WaveVertexShader(meshName, cloneName);
            }
        ),
        "enable", &WaveVertexShader::enable,
        "disable", &WaveVertexShader::disable,
        "renderOneFrame", &WaveVertexShader::renderOneFrame,
        "getName", &AnimationNode::getName,
        "getOutput", [](WaveVertexShader& self, const std::string& name) -> AnimationPort* {
            auto& outputs = self.AnimationNode::getOutputs();
            auto it = outputs.find(name);
            return (it != outputs.end()) ? it->second : nullptr;
        },
        "getInput", [](WaveVertexShader& self, const std::string& name) -> AnimationPort* {
            auto& inputs = self.AnimationNode::getInputs();
            auto it = inputs.find(name);
            return (it != inputs.end()) ? it->second : nullptr;
        }
    );
    bbfx["WaveVertexShader"] = lua["bbfx_WaveVertexShader"];

    // ── ColorShiftNode ──────────────────────────────────────────────────
    lua.new_usertype<ColorShiftNode>("bbfx_ColorShiftNode",
        sol::call_constructor, sol::factories(
            [](const std::string& materialName) {
                return new ColorShiftNode(materialName);
            }
        ),
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["ColorShiftNode"] = lua["bbfx_ColorShiftNode"];

    // ── v2.4 Theora Video bindings ──────────────────────────────────────
    lua.new_usertype<TheoraClip>("bbfx_TheoraClip",
        sol::call_constructor, sol::factories(
            [](const std::string& filename) { return new TheoraClip(filename); }
        ),
        "play", &TheoraClip::play,
        "pause", &TheoraClip::pause,
        "stop", &TheoraClip::stop,
        "frameUpdate", &TheoraClip::frameUpdate,
        "setTime", &TheoraClip::setTime,
        "getTime", &TheoraClip::getTime,
        "isPlaying", &TheoraClip::isPlaying,
        "getMaterialName", &TheoraClip::getMaterialName,
        "getWidth", &TheoraClip::getWidth,
        "getHeight", &TheoraClip::getHeight
    );
    bbfx["TheoraClip"] = lua["bbfx_TheoraClip"];

    lua.new_usertype<ReversableClip>("bbfx_ReversableClip",
        sol::call_constructor, sol::factories(
            [](const std::string& fwd, const std::string& rev) {
                return new ReversableClip(fwd, rev);
            }
        ),
        "doReverse", &ReversableClip::doReverse,
        "isReversed", &ReversableClip::isReversed,
        sol::base_classes, sol::bases<TheoraClip>()
    );
    bbfx["ReversableClip"] = lua["bbfx_ReversableClip"];

    lua.new_usertype<TextureCrossfader>("bbfx_TextureCrossfader",
        sol::call_constructor, sol::factories(
            [](const std::string& mat, const std::string& t1, const std::string& t2) {
                return new TextureCrossfader(mat, t1, t2);
            }
        ),
        "crossfade", &TextureCrossfader::crossfade,
        "getBeta", &TextureCrossfader::getBeta
    );
    bbfx["TextureCrossfader"] = lua["bbfx_TextureCrossfader"];

    lua.new_usertype<TheoraClipNode>("bbfx_TheoraClipNode",
        sol::call_constructor, sol::factories(
            [](const std::string& filename) { return new TheoraClipNode(filename); }
        ),
        "play", &TheoraClipNode::play,
        "pause", &TheoraClipNode::pause,
        "stop", &TheoraClipNode::stop,
        "getClip", &TheoraClipNode::getClip,
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["TheoraClipNode"] = lua["bbfx_TheoraClipNode"];

    // ── v2.6: StdinReader bindings ───────────────────────────────────────
    lua.new_usertype<StdinReader>("bbfx_StdinReader",
        sol::call_constructor, sol::factories(
            []() { return new StdinReader(); }
        ),
        "poll", [&lua](StdinReader& self) -> sol::object {
            auto line = self.poll();
            if (line.has_value()) {
                return sol::make_object(lua, line.value());
            }
            return sol::lua_nil;
        }
    );
    bbfx["StdinReader"] = lua["bbfx_StdinReader"];

    // ── v2.6: TcpServer bindings ────────────────────────────────────────
    lua.new_usertype<TcpServer>("bbfx_TcpServer",
        sol::call_constructor, sol::factories(
            [](int port, int maxClients) { return new TcpServer(port, maxClients); },
            [](int port) { return new TcpServer(port); }
        ),
        "start", &TcpServer::start,
        "stop", &TcpServer::stop,
        "isRunning", &TcpServer::isRunning,
        "poll", [](TcpServer& self) -> sol::as_table_t<std::vector<sol::table>> {
            // Not directly convertible — use wrapper in Lua
            return sol::as_table(std::vector<sol::table>{});
        },
        "pollRaw", [&lua](TcpServer& self) -> sol::table {
            auto messages = self.poll();
            sol::table result = lua.create_table();
            for (size_t i = 0; i < messages.size(); ++i) {
                sol::table msg = lua.create_table();
                msg["id"] = messages[i].clientId;
                msg["text"] = messages[i].text;
                result[i + 1] = msg;
            }
            return result;
        },
        "send", &TcpServer::send
    );
    bbfx["TcpServer"] = lua["bbfx_TcpServer"];

    // ── v2.6: fileModTime binding ───────────────────────────────────────
    bbfx["fileModTime"] = [](const std::string& path) -> double {
        try {
            auto ftime = std::filesystem::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                std::chrono::clock_cast<std::chrono::system_clock>(ftime)
            );
            return static_cast<double>(sctp.time_since_epoch().count());
        } catch (...) {
            return 0.0;
        }
    };
}

} // namespace bbfx

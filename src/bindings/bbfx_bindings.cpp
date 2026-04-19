#include "bbfx_bindings.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include "../core/Engine.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../core/StatsOverlay.h"
#include "../input/InputManager.h"
#include "../input/GamepadManager.h"
#include "../input/GamepadMappingProfile.h"
// v3.5 Lot M — MIDI/OSC/Artnet bindings
#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiLearnManager.h"
#include "../midi/MidiMessage.h"
#include "../osc/OscBus.h"
#include "../osc/OscMessage.h"
#include "../network/UdpServer.h"
#include "../network/ArtnetInput.h"
#include "../studio/TextureShareReceiver.h"
// v3.5 Lot N — Noise + Tempo + Timeline bindings
#include "../procedural/NoiseGenerator.h"
#include "../timing/TempoManager.h"
#include "../timing/LuaTimeline.h"
// v3.5 Lot Q — Media / Images / Sequences / Models
#include "../media/FFmpegBridge.h"
#include "../media/ImageLoader.h"
#include "../media/SequencePlayer.h"
#include "../media/MeshImporter.h"
// v3.5 Lot R — procedural geometry/SDF/L-system
#include "../procedural/GeometryGenerator.h"
#include "../procedural/SDFPrimitives.h"
#include "../procedural/LSystem.h"
// v3.5 Lot S — plugin authoring backend
#include "../plugin/PluginAuthoringBackend.h"
#include "../plugin/PluginManager.h"
// v3.5 Lot U — hot reloader
#include "../plugin/PluginHotReloader.h"
#include "../plugin/PluginValidator.h"
// v3.5 Lot V — GitHub publishing
#include "../plugin/GitHubPublisher.h"
#include "../input/StdinReader.h"
#include "../fx/PerlinVertexShader.h"
#include "../fx/PerlinFxNode.h"
#include "../fx/TextureBlitter.h"
#include "../fx/TextureBlitterNode.h"
#include "../fx/WaveVertexShader.h"
#include "../fx/ColorShiftNode.h"
#include "../fx/ShaderFxNode.h"
#include "../video/TheoraClip.h"
#include "../video/ReversableClip.h"
#include "../video/TextureCrossfader.h"
#include "../video/TheoraClipNode.h"
#include "../record/InputRecorder.h"
#include "../record/InputPlayer.h"
#include "../record/VideoExporter.h"
#include "../network/TcpServer.h"
#include "../audio/AudioCapture.h"
#include "../audio/AudioAnalyzer.h"
#include "../audio/BeatDetector.h"
#include "../core/ParamSpec.h"
#include "../plugin/PluginManager.h"
#include "../plugin/PluginManifest.h"
#include "../plugin/PluginValidator.h"
#include "../plugin/CommunityIndex.h"
#include "../plugin/DeepLinkHandler.h"
#include "../plugin/GithubReactionsFetcher.h"
#include "../network/HttpClient.h"
#include "../network/WebSocketClient.h"
#include <OgreOverlayManager.h>
#include <OgreOverlay.h>
#include <OgreOverlayContainer.h>
#include <OgreOverlayElement.h>
#include <OgreTextAreaOverlayElement.h>
#include <OgreFontManager.h>
#include <OgreViewport.h>
#include <OgreRenderWindow.h>
#include <OgreParticleSystem.h>
#include <OgreCompositorManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreImage.h>
#include <OgreRenderTarget.h>
#include <OgreRenderTexture.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

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
        "toggleFullscreen", &Engine::toggleFullscreen,
        "setOfflineMode", &Engine::setOfflineMode,
        "setOnlineMode", &Engine::setOnlineMode,
        "isOfflineMode", &Engine::isOfflineMode,
        "getOfflineDt", &Engine::getOfflineDt,
        "setVideoExporter", &Engine::setVideoExporter,
        "clearVideoExporter", &Engine::clearVideoExporter
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
        "renameNode", &Animator::renameNode,
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
        "setBPM", &RootTimeNode::setBPM,
        "getBPM", &RootTimeNode::getBPM,
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
        "setUpdateFunction", &LuaAnimationNode::setUpdateFunction,
        "getTargetNodeName", &LuaAnimationNode::getTargetNodeName,
        "getTargetSceneNode", &LuaAnimationNode::getTargetSceneNode,
        "getTargetSceneNodes", &LuaAnimationNode::getTargetSceneNodes,
        "getTargetNodeNames", &LuaAnimationNode::getTargetNodeNames,
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

    // v3.5 Lot J: this binding now covers the full GamepadManager surface.
    // Old Lua scripts that type bbfx.JoystickManager still work — it maps
    // to the same C++ class via the alias in JoystickManager.h.
    lua.new_usertype<GamepadManager>("bbfx_GamepadManager",
        sol::no_constructor,
        // legacy methods
        "getAxisValue", &GamepadManager::getAxisValue,
        "isButtonDown", &GamepadManager::isButtonDown,
        "getCount",     &GamepadManager::getCount,
        // v3.5 Lot J additions
        "getName",      &GamepadManager::getName,
        "getType",      [](GamepadManager& g, int i) -> std::string {
                            return GamepadManager::typeName(g.getType(i));
                        },
        "rumble",          &GamepadManager::rumble,
        "rumbleTriggers",  &GamepadManager::rumbleTriggers,
        "stopRumble",      &GamepadManager::stopRumble,
        "hasGyro",         &GamepadManager::hasGyro,
        "hasAccel",        &GamepadManager::hasAccel,
        "getGyro", [](GamepadManager& g, int i) -> std::tuple<float,float,float> {
            float x, y, z;
            g.getGyro(i, x, y, z);
            return {x, y, z};
        },
        "getAccel", [](GamepadManager& g, int i) -> std::tuple<float,float,float> {
            float x, y, z;
            g.getAccel(i, x, y, z);
            return {x, y, z};
        },
        "calibrateGyro", &GamepadManager::calibrateGyro,
        "setGyroFilter", &GamepadManager::setGyroFilter
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
    // v3.5 Lot J: both names point at the same usertype — existing user
    // scripts using bbfx.JoystickManager continue to work.
    bbfx["GamepadManager"]  = lua["bbfx_GamepadManager"];
    bbfx["JoystickManager"] = lua["bbfx_GamepadManager"];

    // ── v3.5 Lot J: bbfx.gamepad.* convenience namespace ─────────────────
    // Wraps the InputManager singleton for scripts that don't care about
    // object lifetimes. The legacy bbfx.joystick alias points to the same
    // table for backward compat with v3.3/v3.4 scripts.
    auto gamepadNs = bbfx.get_or("gamepad", sol::table(lua, sol::create));
    bbfx["gamepad"] = gamepadNs;
    // The InputManager singleton may tear down before the Lua state does.
    // Wrap the lookup in a function so every binding below is safe.
    auto gmgr = []() -> GamepadManager* {
        auto* im = InputManager::instance();
        return im ? &im->getJoystick() : nullptr;
    };

    gamepadNs["count"] = [gmgr]() -> int {
        auto* g = gmgr(); return g ? g->getCount() : 0;
    };
    gamepadNs["getName"] = [gmgr](int idx) -> std::string {
        auto* g = gmgr(); return g ? g->getName(idx) : std::string();
    };
    gamepadNs["getType"] = [gmgr](int idx) -> std::string {
        auto* g = gmgr();
        return g ? GamepadManager::typeName(g->getType(idx)) : std::string("Generic");
    };
    gamepadNs["getAxisValue"] = [gmgr](int idx, int axis) -> float {
        auto* g = gmgr(); return g ? g->getAxisValue(idx, axis) : 0.0f;
    };
    gamepadNs["isButtonDown"] = [gmgr](int idx, int btn) -> bool {
        auto* g = gmgr(); return g && g->isButtonDown(idx, btn);
    };
    // v3.5 Lot L — convenience accessors so Lua plugins don't have to
    // remember the SDL3 axis/button enum numbers.
    gamepadNs["isPressed"] = [gmgr](int idx, int btn) -> bool {
        auto* g = gmgr(); return g && g->isButtonDown(idx, btn);
    };
    gamepadNs["getLeftStick"] = [gmgr](int idx) -> std::tuple<float, float> {
        auto* g = gmgr(); if (!g) return {0.0f, 0.0f};
        return { g->getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFTX),
                 g->getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFTY) };
    };
    gamepadNs["getRightStick"] = [gmgr](int idx) -> std::tuple<float, float> {
        auto* g = gmgr(); if (!g) return {0.0f, 0.0f};
        return { g->getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHTX),
                 g->getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHTY) };
    };
    gamepadNs["getTriggers"] = [gmgr](int idx) -> std::tuple<float, float> {
        auto* g = gmgr(); if (!g) return {0.0f, 0.0f};
        float lt = std::max(0.0f, g->getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        float rt = std::max(0.0f, g->getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        return { lt, rt };
    };
    gamepadNs["rumble"] = [gmgr](int idx, float low, float high, int ms) -> bool {
        auto* g = gmgr(); return g && g->rumble(idx, low, high, ms);
    };
    gamepadNs["rumbleTriggers"] = [gmgr](int idx, float l, float r, int ms) -> bool {
        auto* g = gmgr(); return g && g->rumbleTriggers(idx, l, r, ms);
    };
    gamepadNs["stopRumble"] = [gmgr](int idx) {
        if (auto* g = gmgr()) g->stopRumble(idx);
    };
    gamepadNs["hasGyro"] = [gmgr](int idx) -> bool {
        auto* g = gmgr(); return g && g->hasGyro(idx);
    };
    gamepadNs["hasAccel"] = [gmgr](int idx) -> bool {
        auto* g = gmgr(); return g && g->hasAccel(idx);
    };
    gamepadNs["getGyro"] = [gmgr](int idx) -> std::tuple<float, float, float> {
        auto* g = gmgr();
        float x = 0, y = 0, z = 0;
        if (g) g->getGyro(idx, x, y, z);
        return {x, y, z};
    };
    gamepadNs["getAccel"] = [gmgr](int idx) -> std::tuple<float, float, float> {
        auto* g = gmgr();
        float x = 0, y = 0, z = 0;
        if (g) g->getAccel(idx, x, y, z);
        return {x, y, z};
    };
    gamepadNs["calibrateGyro"] = [gmgr](int idx) {
        if (auto* g = gmgr()) g->calibrateGyro(idx);
    };
    gamepadNs["setGyroFilter"] = [gmgr](float processNoise, float measurementNoise) {
        if (auto* g = gmgr()) g->setGyroFilter(processNoise, measurementNoise);
    };

    // --- Lot K: touchpad / LED / battery -------------------------------
    gamepadNs["hasTouchpad"] = [gmgr](int idx) -> bool {
        auto* g = gmgr(); return g && g->hasTouchpad(idx);
    };
    gamepadNs["getTouchpadFingers"] = [gmgr, &lua](int idx) -> sol::table {
        sol::table t = lua.create_table();
        auto* gm = gmgr();
        if (!gm) { t["count"] = 0; return t; }
        GamepadManager::TouchFinger f[2];
        int n = gm->getTouchpadFingers(idx, f);
        t["count"] = n;
        sol::table arr = lua.create_table();
        for (int i = 0; i < 2; ++i) {
            sol::table ft = lua.create_table();
            ft["down"]     = f[i].down;
            ft["x"]        = f[i].x;
            ft["y"]        = f[i].y;
            ft["pressure"] = f[i].pressure;
            arr[i + 1] = ft;
        }
        t["fingers"] = arr;
        return t;
    };
    gamepadNs["hasLED"] = [gmgr](int idx) -> bool {
        auto* g = gmgr(); return g && g->hasLED(idx);
    };
    gamepadNs["setLED"] = [gmgr](int idx, int r, int g, int b) -> bool {
        auto* gm = gmgr();
        return gm && gm->setLED(idx,
            static_cast<uint8_t>(std::clamp(r, 0, 255)),
            static_cast<uint8_t>(std::clamp(g, 0, 255)),
            static_cast<uint8_t>(std::clamp(b, 0, 255)));
    };
    gamepadNs["getBatteryPercent"] = [gmgr](int idx) -> int {
        auto* g = gmgr(); return g ? g->getBatteryPercent(idx) : -1;
    };
    gamepadNs["getBatteryState"] = [gmgr](int idx) -> std::string {
        auto* g = gmgr();
        return g ? GamepadManager::powerStateName(g->getBatteryState(idx))
                 : std::string("Unknown");
    };

    // Legacy alias — the v3.3/v3.4 `bbfx.joystick` surface is a subset of
    // the new table. Point the whole table at the same Lua value.
    bbfx["joystick"] = gamepadNs;

    // ── v3.5 Lot K: bbfx.gamepadMapping.* -- load/save .bbfx-gamepad-mapping
    auto mapNs = bbfx.get_or("gamepadMapping", sol::table(lua, sol::create));
    bbfx["gamepadMapping"] = mapNs;
    mapNs["loadFile"] = [&lua](const std::string& path) -> sol::object {
        GamepadMappingProfile p;
        std::string err;
        if (!p.loadFromFile(path, err)) {
            sol::table t = lua.create_table();
            t["ok"] = false;
            t["error"] = err;
            return t;
        }
        sol::table t = lua.create_table();
        t["ok"]          = true;
        t["name"]        = p.name;
        t["deviceType"]  = p.deviceType;
        t["description"] = p.description;
        sol::table arr = lua.create_table();
        int i = 1;
        for (const auto& m : p.mappings) {
            sol::table mt = lua.create_table();
            mt["source"] = m.source;
            mt["target"] = m.target;
            mt["scale"]  = m.scale;
            mt["offset"] = m.offset;
            mt["invert"] = m.invert;
            arr[i++] = mt;
        }
        t["mappings"] = arr;
        return t;
    };
    mapNs["saveFile"] = [&lua](const std::string& path, sol::table profileTbl) -> sol::object {
        GamepadMappingProfile p;
        if (auto v = profileTbl.get<sol::optional<std::string>>("name"))        p.name = *v;
        if (auto v = profileTbl.get<sol::optional<std::string>>("deviceType"))  p.deviceType = *v;
        else p.deviceType = "Any";
        if (auto v = profileTbl.get<sol::optional<std::string>>("description")) p.description = *v;

        if (auto arr = profileTbl.get<sol::optional<sol::table>>("mappings")) {
            for (auto& kv : *arr) {
                sol::table mt = kv.second.as<sol::table>();
                GamepadMappingEntry e;
                if (auto v = mt.get<sol::optional<std::string>>("source")) e.source = *v;
                if (auto v = mt.get<sol::optional<std::string>>("target")) e.target = *v;
                if (auto v = mt.get<sol::optional<float>>("scale"))        e.scale  = *v;
                if (auto v = mt.get<sol::optional<float>>("offset"))       e.offset = *v;
                if (auto v = mt.get<sol::optional<bool>>("invert"))        e.invert = *v;
                p.mappings.push_back(std::move(e));
            }
        }
        std::string err;
        return sol::make_object(lua, p.saveToFile(path, err));
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot M — Live protocols Lua API (MIDI / OSC / Artnet / TextureShare)
    // ══════════════════════════════════════════════════════════════════════

    // ── bbfx.midi.* — listPorts/getCC/isNoteOn/sendCC/sendNoteOn/Off/learn ─
    auto midiNs = bbfx.get_or("midi", sol::table(lua, sol::create));
    bbfx["midi"] = midiNs;

    midiNs["listInputPorts"] = [&lua]() -> sol::table {
        sol::table t = lua.create_table();
        auto* m = MidiDeviceManager::instance();
        if (!m) return t;
        int i = 1;
        for (auto& n : m->getInputDeviceNames()) t[i++] = n;
        return t;
    };
    midiNs["listOutputPorts"] = [&lua]() -> sol::table {
        sol::table t = lua.create_table();
        auto* m = MidiDeviceManager::instance();
        if (!m) return t;
        int i = 1;
        for (auto& n : m->getOutputDeviceNames()) t[i++] = n;
        return t;
    };
    midiNs["getCC"] = [](int channel, int cc) -> float {
        auto* m = MidiDeviceManager::instance();
        return m ? m->getLastCCValue(channel, cc) : 0.0f;
    };
    midiNs["isNoteOn"] = [](int channel, int note) -> bool {
        auto* m = MidiDeviceManager::instance();
        return m && m->isNoteDown(channel, note);
    };
    midiNs["getNoteVelocity"] = [](int channel, int note) -> float {
        auto* m = MidiDeviceManager::instance();
        return m ? m->getNoteVelocity(channel, note) : 0.0f;
    };
    midiNs["sendCC"] = [](int outputDeviceId, int channel, int cc, int value) {
        auto* m = MidiDeviceManager::instance();
        if (!m || channel < 1 || channel > 16) return;
        uint8_t status = MidiMessage::ControlChange | ((channel - 1) & 0x0F);
        m->sendMessage(outputDeviceId, status,
                        static_cast<uint8_t>(std::clamp(cc, 0, 127)),
                        static_cast<uint8_t>(std::clamp(value, 0, 127)));
    };
    midiNs["sendNoteOn"] = [](int outputDeviceId, int channel, int note, int velocity) {
        auto* m = MidiDeviceManager::instance();
        if (!m || channel < 1 || channel > 16) return;
        uint8_t status = MidiMessage::NoteOn | ((channel - 1) & 0x0F);
        m->sendMessage(outputDeviceId, status,
                        static_cast<uint8_t>(std::clamp(note, 0, 127)),
                        static_cast<uint8_t>(std::clamp(velocity, 0, 127)));
    };
    midiNs["sendNoteOff"] = [](int outputDeviceId, int channel, int note) {
        auto* m = MidiDeviceManager::instance();
        if (!m || channel < 1 || channel > 16) return;
        uint8_t status = MidiMessage::NoteOff | ((channel - 1) & 0x0F);
        m->sendMessage(outputDeviceId, status,
                        static_cast<uint8_t>(std::clamp(note, 0, 127)),
                        0);
    };
    // bbfx.midi.learn(paramName, callback) — captures the next CC/note and
    // invokes `callback(messageType, channel, number, value)`. Uses the
    // existing MidiLearnManager but with a custom "lua" target so it does
    // not conflict with fader/trigger bindings.
    midiNs["learn"] = [](const std::string& paramName, sol::function cb) {
        auto& mlm = MidiLearnManager::instance();
        MidiLearnTarget t;
        t.type = "lua";
        t.portName = paramName;
        mlm.startLearn(t);
        // We register a one-shot observer in OscBus::luaLearnCallback.
        // Since MidiLearnManager itself only wires fader/trigger/port
        // bindings, we intercept via an extra hook registered here.
        OscBus::instance().registerMidiLearnCallback(paramName, cb);
    };
    midiNs["cancelLearn"] = []() {
        MidiLearnManager::instance().cancelLearn();
    };

    // ── bbfx.osc.* — send / on / get --------------------------------------
    auto oscNs = bbfx.get_or("osc", sol::table(lua, sol::create));
    bbfx["osc"] = oscNs;

    oscNs["send"] = [](const std::string& destination,
                        const std::string& address,
                        sol::variadic_args va) {
        OscMessage m;
        m.address = address;
        for (auto v : va) {
            if (v.is<float>())         m.args.emplace_back(v.get<float>());
            else if (v.is<int32_t>())  m.args.emplace_back(v.get<int32_t>());
            else if (v.is<std::string>()) m.args.emplace_back(v.get<std::string>());
        }
        // destination format : "ip:port". Default port is 8000 if omitted.
        std::string ip = destination; int port = 8000;
        auto colon = destination.find(':');
        if (colon != std::string::npos) {
            ip   = destination.substr(0, colon);
            port = std::atoi(destination.substr(colon + 1).c_str());
        }
        return UdpServer::send(ip, port, m);
    };
    oscNs["on"] = [](const std::string& pattern, sol::function cb) -> int {
        return OscBus::instance().on(pattern, std::move(cb));
    };
    oscNs["off"] = [](int handle) {
        OscBus::instance().off(handle);
    };
    oscNs["get"] = [&lua](const std::string& address) -> sol::object {
        auto v = OscBus::instance().last(address);
        if (!v) return sol::make_object(lua, sol::nil);
        if (std::holds_alternative<float>(*v))
            return sol::make_object(lua, std::get<float>(*v));
        if (std::holds_alternative<int32_t>(*v))
            return sol::make_object(lua, std::get<int32_t>(*v));
        return sol::make_object(lua, std::get<std::string>(*v));
    };
    oscNs["listen"] = [](int port) -> bool {
        return OscBus::instance().listen(port);
    };
    oscNs["discoveredAddresses"] = [&lua]() -> sol::table {
        sol::table t = lua.create_table();
        int i = 1;
        for (auto& a : OscBus::instance().discoveredAddresses()) t[i++] = a;
        return t;
    };

    // ── bbfx.artnet.* — send / sendBulk / onReceive / getChannels --------
    auto artnetNs = bbfx.get_or("artnet", sol::table(lua, sol::create));
    bbfx["artnet"] = artnetNs;

    artnetNs["send"] = [](const std::string& ip, int universe, int channel, int value) -> bool {
        if (universe < 0 || universe > 15)  return false;
        if (channel < 1  || channel > 512)  return false;
        std::vector<uint8_t> data(512, 0);
        data[channel - 1] = static_cast<uint8_t>(std::clamp(value, 0, 255));
        auto pkt = ArtnetInput::buildDmxPacket(universe, data, 0);
        return ArtnetInput::sendRaw(ip, ArtnetInput::ARTNET_PORT, pkt);
    };
    artnetNs["sendBulk"] = [](const std::string& ip, int universe, int start,
                                sol::table values) -> bool {
        if (universe < 0 || universe > 15) return false;
        std::vector<uint8_t> data(512, 0);
        int i = start;
        for (auto& kv : values) {
            if (i < 1 || i > 512) break;
            int v = kv.second.is<int>() ? kv.second.as<int>()
                                          : static_cast<int>(kv.second.as<float>() * 255.0f);
            data[i - 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            ++i;
        }
        auto pkt = ArtnetInput::buildDmxPacket(universe, data, 0);
        return ArtnetInput::sendRaw(ip, ArtnetInput::ARTNET_PORT, pkt);
    };
    artnetNs["onReceive"] = [](int universe, sol::function cb) -> int {
        return ArtnetInput::instance().on(universe, std::move(cb));
    };
    artnetNs["off"] = [](int handle) {
        ArtnetInput::instance().off(handle);
    };
    artnetNs["getChannels"] = [&lua](int universe) -> sol::table {
        sol::table t = lua.create_table();
        auto ch = ArtnetInput::instance().channels(universe);
        for (int i = 0; i < 512; ++i) t[i + 1] = ch[i] / 255.0f;
        return t;
    };
    artnetNs["listen"] = []() -> bool {
        return ArtnetInput::instance().start();
    };
    artnetNs["stop"] = []() {
        ArtnetInput::instance().stop();
    };

    // ── bbfx.textureShare.* — createReceiver / listSources / createSender ─
    auto tsNs = bbfx.get_or("textureShare", sol::table(lua, sol::create));
    bbfx["textureShare"] = tsNs;

    tsNs["createReceiver"] = [&lua](const std::string& sourceName) -> sol::object {
        auto r = createTextureReceiver(sourceName);
        if (!r) return sol::make_object(lua, sol::nil);
        sol::table handle = lua.create_table();
        // Keep the receiver alive via a shared pointer stored as userdata.
        auto shared = std::shared_ptr<TextureShareReceiver>(std::move(r));
        handle["_impl"] = shared;
        handle["getTextureName"] = [shared]() -> std::string {
            return shared ? shared->getTextureName() : std::string();
        };
        handle["update"] = [shared]() -> bool {
            return shared && shared->updateTexture();
        };
        handle["release"] = [shared]() {
            if (shared) shared->release();
        };
        handle["backend"] = [shared]() -> std::string {
            return shared ? shared->backendName() : std::string("Null");
        };
        return handle;
    };
    tsNs["listSources"] = [&lua]() -> sol::table {
        sol::table t = lua.create_table();
        int i = 1;
        for (auto& s : TextureShareReceiver::listAvailableSources()) t[i++] = s;
        return t;
    };
    tsNs["backend"] = []() -> std::string {
        return TextureShareReceiver::platformBackend();
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot N — Noise / Easing / Tempo / Timeline Lua API
    // ══════════════════════════════════════════════════════════════════════

    // ── bbfx.noise.* (no permission required) ──────────────────────────
    auto noiseNs = bbfx.get_or("noise", sol::table(lua, sol::create));
    bbfx["noise"] = noiseNs;

    noiseNs["simplex2D"] = sol::overload(
        [](float x, float y)           { return NoiseGenerator::simplex2D(x, y, 0); },
        [](float x, float y, int seed) { return NoiseGenerator::simplex2D(x, y, seed); }
    );
    noiseNs["simplex3D"] = sol::overload(
        [](float x, float y, float z)           { return NoiseGenerator::simplex3D(x, y, z, 0); },
        [](float x, float y, float z, int seed) { return NoiseGenerator::simplex3D(x, y, z, seed); }
    );
    noiseNs["simplex4D"] = sol::overload(
        [](float x, float y, float z, float w)           { return NoiseGenerator::simplex4D(x, y, z, w, 0); },
        [](float x, float y, float z, float w, int seed) { return NoiseGenerator::simplex4D(x, y, z, w, seed); }
    );
    noiseNs["worley2D"] = sol::overload(
        [](float x, float y)           { return NoiseGenerator::worley2D(x, y, 0); },
        [](float x, float y, int seed) { return NoiseGenerator::worley2D(x, y, seed); }
    );
    noiseNs["worley3D"] = sol::overload(
        [](float x, float y, float z)           { return NoiseGenerator::worley3D(x, y, z, 0); },
        [](float x, float y, float z, int seed) { return NoiseGenerator::worley3D(x, y, z, seed); }
    );
    noiseNs["curl2D"] = [](float x, float y, int seed) -> std::tuple<float, float> {
        float ox, oy; NoiseGenerator::curl2D(x, y, seed, ox, oy);
        return { ox, oy };
    };
    noiseNs["curl3D"] = [](float x, float y, float z, int seed)
                            -> std::tuple<float, float, float> {
        float ox, oy, oz; NoiseGenerator::curl3D(x, y, z, seed, ox, oy, oz);
        return { ox, oy, oz };
    };
    noiseNs["fbm2D"] = [](float x, float y, sol::optional<sol::table> optsTbl) -> float {
        NoiseGenerator::FbmOptions o;
        if (optsTbl) {
            if (auto v = optsTbl->get<sol::optional<int>>("octaves"))   o.octaves = *v;
            if (auto v = optsTbl->get<sol::optional<float>>("lacunarity")) o.lacunarity = *v;
            if (auto v = optsTbl->get<sol::optional<float>>("gain"))    o.gain = *v;
            if (auto v = optsTbl->get<sol::optional<int>>("seed"))      o.seed = *v;
        }
        return NoiseGenerator::fbm2D(x, y, o);
    };
    noiseNs["generateTexture"] = [](int w, int h, sol::optional<sol::table> optsTbl) -> std::string {
        std::string kind = "simplex"; float scale = 8.0f; int octaves = 4; int seed = 0;
        if (optsTbl) {
            if (auto v = optsTbl->get<sol::optional<std::string>>("kind"))  kind = *v;
            if (auto v = optsTbl->get<sol::optional<float>>("scale"))       scale = *v;
            if (auto v = optsTbl->get<sol::optional<int>>("octaves"))       octaves = *v;
            if (auto v = optsTbl->get<sol::optional<int>>("seed"))          seed = *v;
        }
        return NoiseGenerator::generateTexture(w, h, kind, scale, octaves, seed);
    };

    // ── bbfx.easing.* — pure Lua library loaded from lua/plugin/easing.lua
    // Exposing it as a C++ binding would be redundant. We load the file
    // here and assign it to bbfx.easing so plugins see the same global.
    try {
        auto res = lua.safe_script_file("lua/plugin/easing.lua");
        if (res.valid()) {
            sol::table easingLib = res;
            bbfx["easing"] = easingLib;
        }
    } catch (const std::exception& e) {
        std::cerr << "[bbfx.easing] load failed: " << e.what() << std::endl;
    }

    // ── bbfx.tempo.* ──────────────────────────────────────────────────
    auto tempoNs = bbfx.get_or("tempo", sol::table(lua, sol::create));
    bbfx["tempo"] = tempoNs;

    tempoNs["getBPM"]        = []() { return TempoManager::instance().getBPM(); };
    tempoNs["getBeat"]       = []() { return TempoManager::instance().getBeat(); };
    tempoNs["getBeatPhase"]  = []() { return TempoManager::instance().getBeatPhase(); };
    tempoNs["setManualBPM"]  = [](float bpm) { TempoManager::instance().setManualBPM(bpm); };
    tempoNs["setSource"]     = [](const std::string& s) {
        TempoManager::instance().setSource(TempoManager::sourceFromString(s));
    };
    tempoNs["getSource"]     = []() {
        return std::string(TempoManager::sourceName(TempoManager::instance().getSource()));
    };
    tempoNs["onNextBeat"] = [](sol::function cb) -> int {
        return TempoManager::instance().onNextBeat([cb]() {
            try { cb(); } catch (const std::exception& e) {
                std::cerr << "[bbfx.tempo] onNextBeat cb: " << e.what() << std::endl;
            }
        });
    };
    tempoNs["onNextBar"] = [](sol::function cb) -> int {
        return TempoManager::instance().onNextBar([cb]() {
            try { cb(); } catch (const std::exception& e) {
                std::cerr << "[bbfx.tempo] onNextBar cb: " << e.what() << std::endl;
            }
        });
    };
    tempoNs["onSubdivision"] = [](int n, sol::function cb) -> int {
        return TempoManager::instance().onSubdivision(n, [cb]() {
            try { cb(); } catch (const std::exception& e) {
                std::cerr << "[bbfx.tempo] onSubdivision cb: " << e.what() << std::endl;
            }
        });
    };
    tempoNs["off"] = [](int handle) { TempoManager::instance().off(handle); };

    // ── bbfx.timeline.* — keyframe+event timeline container ──────────
    auto timelineNs = bbfx.get_or("timeline", sol::table(lua, sol::create));
    bbfx["timeline"] = timelineNs;

    timelineNs["create"] = [&lua](sol::optional<sol::table> optsTbl) -> sol::table {
        auto tl = std::make_shared<LuaTimeline>();
        if (optsTbl) {
            if (auto v = optsTbl->get<sol::optional<float>>("duration")) tl->setDuration(*v);
            if (auto v = optsTbl->get<sol::optional<bool>>("loop"))       tl->setLoop(*v);
            if (auto v = optsTbl->get<sol::optional<float>>("speed"))     tl->setSpeed(*v);
        }
        sol::table h = lua.create_table();
        h["_impl"]        = tl;
        h["addKey"]       = [tl](float t, float v, sol::optional<std::string> e) {
            tl->addKey(t, v, e ? *e : std::string());
        };
        h["addEvent"]     = [tl](float t, sol::function cb) { tl->addEvent(t, std::move(cb)); };
        h["play"]         = [tl]()            { tl->play(); };
        h["pause"]        = [tl]()            { tl->pause(); };
        h["stop"]         = [tl]()            { tl->stop(); };
        h["seek"]         = [tl](float t)     { tl->seek(t); };
        h["setSpeed"]     = [tl](float s)     { tl->setSpeed(s); };
        h["setLoop"]      = [tl](bool on)     { tl->setLoop(on); };
        h["getValue"]     = [tl]()            { return tl->getValue(); };
        h["getCurrentTime"] = [tl]()          { return tl->getCurrentTime(); };
        h["getDuration"]  = [tl]()            { return tl->getDuration(); };
        h["isPlaying"]    = [tl]()            { return tl->isPlaying(); };
        h["clearKeys"]    = [tl]()            { tl->clearKeys(); };
        h["clearEvents"]  = [tl]()            { tl->clearEvents(); };
        h["update"]       = [tl, &lua](float dt) {
            sol::object easingObj = lua["bbfx"]["easing"];
            sol::table easingLib = easingObj.is<sol::table>()
                                    ? easingObj.as<sol::table>()
                                    : sol::table();
            tl->update(dt, easingLib);
        };
        return h;
    };

    // ── I-065: FX Lua bindings ──────────────────────────────────────────
    lua.new_usertype<PerlinVertexShader>("bbfx_PerlinVertexShader",
        sol::call_constructor, sol::factories(
            [](const std::string& meshName, const std::string& cloneName) {
                return new PerlinVertexShader(meshName, cloneName);
            }
        ),
        "enable", &PerlinVertexShader::enable,
        "disable", &PerlinVertexShader::disable,
        "renderOneFrame", &PerlinVertexShader::renderOneFrame,
        "setDisplacement", &PerlinVertexShader::setDisplacement,
        "getDisplacement", &PerlinVertexShader::getDisplacement
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
        "setLoop", &TheoraClip::setLoop,
        "isLooping", &TheoraClip::isLooping,
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

    // ── v2.7: Audio bindings ───────────────────────────────────────────
    lua.new_usertype<AudioCapture>("bbfx_AudioCapture",
        sol::call_constructor, sol::factories(
            [](int sampleRate, int bufferSize) { return new AudioCapture(sampleRate, bufferSize); },
            [](int sampleRate) { return new AudioCapture(sampleRate); },
            []() { return new AudioCapture(); }
        ),
        "start", &AudioCapture::start,
        "stop", &AudioCapture::stop,
        "isRunning", &AudioCapture::isRunning,
        "getSampleRate", &AudioCapture::getSampleRate,
        "getBufferSize", &AudioCapture::getBufferSize
    );
    bbfx["AudioCapture"] = lua["bbfx_AudioCapture"];

    lua.new_usertype<AudioCaptureNode>("bbfx_AudioCaptureNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name, AudioCapture* capture) {
                return new AudioCaptureNode(name, capture);
            }
        ),
        "hasFreshData", &AudioCaptureNode::hasFreshData,
        "tick", [](AudioCaptureNode* n) { n->update(); },
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["AudioCaptureNode"] = lua["bbfx_AudioCaptureNode"];

    lua.new_usertype<AudioAnalyzerNode>("bbfx_AudioAnalyzerNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name, AudioCaptureNode* captureNode) {
                return new AudioAnalyzerNode(name, captureNode);
            }
        ),
        "getRMS", &AudioAnalyzerNode::getRMS,
        "getPeak", &AudioAnalyzerNode::getPeak,
        "getBand", &AudioAnalyzerNode::getBand,
        "tick", [](AudioAnalyzerNode* n) { n->update(); },
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["AudioAnalyzerNode"] = lua["bbfx_AudioAnalyzerNode"];

    lua.new_usertype<BeatDetectorNode>("bbfx_BeatDetectorNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name, AudioAnalyzerNode* analyzer) {
                return new BeatDetectorNode(name, analyzer);
            }
        ),
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["BeatDetectorNode"] = lua["bbfx_BeatDetectorNode"];

    // ── Ogre::Viewport / RenderWindow bindings ────────────────────────
    auto ogre = lua["Ogre"].get_or_create<sol::table>();

    lua.new_usertype<Ogre::Viewport>("Ogre_Viewport",
        sol::no_constructor,
        "setBackgroundColour", &Ogre::Viewport::setBackgroundColour,
        "getBackgroundColour", &Ogre::Viewport::getBackgroundColour,
        "setClearEveryFrame", [](Ogre::Viewport* vp, bool clear) { vp->setClearEveryFrame(clear); },
        "getActualWidth", &Ogre::Viewport::getActualWidth,
        "getActualHeight", &Ogre::Viewport::getActualHeight
    );

    lua.new_usertype<Ogre::RenderWindow>("Ogre_RenderWindow",
        sol::no_constructor,
        "getViewport", &Ogre::RenderWindow::getViewport,
        "getNumViewports", &Ogre::RenderWindow::getNumViewports,
        "getWidth", &Ogre::RenderWindow::getWidth,
        "getHeight", &Ogre::RenderWindow::getHeight,
        "writeContentsToFile", &Ogre::RenderWindow::writeContentsToFile
    );

    // ── Ogre::ParticleSystem bindings ──────────────────────────────────
    lua.new_usertype<Ogre::ParticleSystem>("Ogre_ParticleSystem",
        sol::no_constructor,
        sol::base_classes, sol::bases<Ogre::MovableObject>(),
        "setVisible", &Ogre::ParticleSystem::setVisible,
        "getName", &Ogre::ParticleSystem::getName,
        "getNumParticles", &Ogre::ParticleSystem::getNumParticles,
        "setEmitting", &Ogre::ParticleSystem::setEmitting,
        "fastForward", &Ogre::ParticleSystem::fastForward
    );

    ogre.set_function("createParticleSystem",
        [](Ogre::SceneManager* sm, const std::string& name, const std::string& templateName) -> Ogre::ParticleSystem* {
            return sm->createParticleSystem(name, templateName);
        }
    );

    // ── Ogre::CompositorManager bindings (lightweight table approach) ──
    // Lua code uses: Ogre.CompositorManager.getSingleton():addCompositor(vp, name)
    // We emulate this by creating a table with methods that delegate to the singleton.
    {
        sol::table cmgr = lua.create_table();
        cmgr["getSingleton"] = [&lua]() {
            sol::table inst = lua.create_table();
            inst["addCompositor"] = [](sol::table, Ogre::Viewport* vp, const std::string& name) {
                Ogre::CompositorManager::getSingleton().addCompositor(vp, name);
            };
            inst["removeCompositor"] = [](sol::table, Ogre::Viewport* vp, const std::string& name) {
                Ogre::CompositorManager::getSingleton().removeCompositor(vp, name);
            };
            inst["setCompositorEnabled"] = [](sol::table, Ogre::Viewport* vp, const std::string& name, bool enabled) {
                Ogre::CompositorManager::getSingleton().setCompositorEnabled(vp, name, enabled);
            };
            return inst;
        };
        ogre["CompositorManager"] = cmgr;
    }

    // ── v2.8: ShaderFxNode bindings ────────────────────────────────────
    lua.new_usertype<ShaderFxNode>("bbfx_ShaderFxNode",
        sol::call_constructor, sol::factories(
            [](const std::string& name, const std::string& vertPath,
               const std::string& fragPath, Ogre::SceneManager* scene) {
                return new ShaderFxNode(name, vertPath, fragPath, scene);
            },
            [](const std::string& name, const std::string& vertPath,
               Ogre::SceneManager* scene) {
                return new ShaderFxNode(name, vertPath, "", scene);
            }
        ),
        sol::base_classes, sol::bases<AnimationNode>()
    );
    bbfx["ShaderFxNode"] = lua["bbfx_ShaderFxNode"];

    // ── v2.7: Overlay bindings ────────────────────────────────────────

    ogre.set_function("OverlayManager_getSingleton", []() -> Ogre::OverlayManager& {
        return Ogre::OverlayManager::getSingleton();
    });
    ogre.set_function("OverlayManager_create", [](const std::string& name) -> Ogre::Overlay* {
        return Ogre::OverlayManager::getSingleton().create(name);
    });
    ogre.set_function("OverlayManager_getByName", [](const std::string& name) -> Ogre::Overlay* {
        return Ogre::OverlayManager::getSingleton().getByName(name);
    });
    ogre.set_function("OverlayManager_createPanel", [](const std::string& name) -> Ogre::OverlayContainer* {
        return static_cast<Ogre::OverlayContainer*>(
            Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", name));
    });
    ogre.set_function("OverlayManager_createTextArea", [](const std::string& name) -> Ogre::TextAreaOverlayElement* {
        return static_cast<Ogre::TextAreaOverlayElement*>(
            Ogre::OverlayManager::getSingleton().createOverlayElement("TextArea", name));
    });

    lua.new_usertype<Ogre::Overlay>("Ogre_Overlay",
        sol::no_constructor,
        "show", &Ogre::Overlay::show,
        "hide", &Ogre::Overlay::hide,
        "isVisible", &Ogre::Overlay::isVisible,
        "add2D", &Ogre::Overlay::add2D,
        "setZOrder", &Ogre::Overlay::setZOrder
    );

    lua.new_usertype<Ogre::OverlayElement>("Ogre_OverlayElement",
        sol::no_constructor,
        "setPosition", &Ogre::OverlayElement::setPosition,
        "setDimensions", &Ogre::OverlayElement::setDimensions,
        "show", &Ogre::OverlayElement::show,
        "hide", &Ogre::OverlayElement::hide
    );

    lua.new_usertype<Ogre::OverlayContainer>("Ogre_OverlayContainer",
        sol::no_constructor,
        "setPosition", &Ogre::OverlayContainer::setPosition,
        "setDimensions", &Ogre::OverlayContainer::setDimensions,
        "addChild", &Ogre::OverlayContainer::addChild,
        "show", &Ogre::OverlayContainer::show,
        "hide", &Ogre::OverlayContainer::hide,
        sol::base_classes, sol::bases<Ogre::OverlayElement>()
    );

    lua.new_usertype<Ogre::TextAreaOverlayElement>("Ogre_TextAreaOverlayElement",
        sol::no_constructor,
        "setCaption", [](Ogre::TextAreaOverlayElement& self, const std::string& text) {
            self.setCaption(text);
        },
        "setCharHeight", &Ogre::TextAreaOverlayElement::setCharHeight,
        "setFontName", [](Ogre::TextAreaOverlayElement& self, const std::string& name) {
            self.setFontName(name);
        },
        "setColour", &Ogre::TextAreaOverlayElement::setColour,
        "setPosition", &Ogre::TextAreaOverlayElement::setPosition,
        "setDimensions", &Ogre::TextAreaOverlayElement::setDimensions,
        "show", &Ogre::TextAreaOverlayElement::show,
        "hide", &Ogre::TextAreaOverlayElement::hide,
        sol::base_classes, sol::bases<Ogre::OverlayElement>()
    );

    // ── v2.9: InputRecorder bindings ───────────────────────────────────
    lua.new_usertype<InputRecorder>("bbfx_InputRecorder",
        sol::call_constructor, sol::factories([]() { return new InputRecorder(); }),
        "start", &InputRecorder::start,
        "stop", &InputRecorder::stop,
        "isRecording", &InputRecorder::isRecording,
        "advanceTime", &InputRecorder::advanceTime,
        "recordKey", &InputRecorder::recordKey,
        "recordAxis", &InputRecorder::recordAxis,
        "recordBeat", &InputRecorder::recordBeat
    );
    bbfx["InputRecorder"] = lua["bbfx_InputRecorder"];

    // ── v2.9: InputPlayer bindings ──────────────────────────────────────
    lua.new_usertype<InputPlayer>("bbfx_InputPlayer",
        sol::call_constructor, sol::factories([]() { return new InputPlayer(); }),
        "play", &InputPlayer::play,
        "stop", &InputPlayer::stop,
        "isPlaying", &InputPlayer::isPlaying,
        "getNextEvents", [&lua](InputPlayer& self, float dt) -> sol::table {
            auto events = self.getNextEvents(dt);
            sol::table result = lua.create_table();
            for (size_t i = 0; i < events.size(); ++i) {
                sol::table e = lua.create_table();
                e["type"] = events[i].type;
                e["code"] = events[i].code;
                e["state"] = events[i].state;
                e["value"] = events[i].value;
                e["time"] = events[i].time;
                result[i + 1] = e;
            }
            return result;
        }
    );
    bbfx["InputPlayer"] = lua["bbfx_InputPlayer"];

    // ── v2.9: VideoExporter bindings ────────────────────────────────────
    lua.new_usertype<VideoExporter>("bbfx_VideoExporter",
        sol::call_constructor, sol::factories([]() { return new VideoExporter(); }),
        "start", &VideoExporter::start,
        "captureFrame", [](VideoExporter& self, Ogre::RenderWindow* rw) { self.captureFrame(rw); },
        "stop", &VideoExporter::stop,
        "isExporting", &VideoExporter::isExporting,
        "getFrameCount", &VideoExporter::getFrameCount
    );
    bbfx["VideoExporter"] = lua["bbfx_VideoExporter"];

    // ── v3.2: ParamSpec bindings ─────────────────────────────────────────
    {
        auto paramSpecType = lua.new_usertype<ParamSpec>("bbfx_ParamSpec",
            sol::constructors<ParamSpec()>());
        paramSpecType["addParam"] = [](ParamSpec& self, const sol::table& t) {
            ParamDef def;
            sol::optional<std::string> nameOpt = t["name"];
            def.name = nameOpt.value_or("");
            sol::optional<std::string> labelOpt = t["label"];
            def.label = labelOpt.value_or(def.name);
            sol::optional<std::string> typeOpt = t["type"];
            std::string typeStr = typeOpt.value_or("float");

            if (typeStr == "float") {
                def.type = ParamType::FLOAT;
                sol::optional<float> v = t["default"]; def.floatVal = v.value_or(0.0f);
            } else if (typeStr == "int") {
                def.type = ParamType::INT;
                sol::optional<int> v = t["default"]; def.intVal = v.value_or(0);
            } else if (typeStr == "bool") {
                def.type = ParamType::BOOL;
                sol::optional<bool> v = t["default"]; def.boolVal = v.value_or(false);
            } else if (typeStr == "enum") {
                def.type = ParamType::ENUM;
                sol::optional<std::string> v = t["default"]; def.stringVal = v.value_or("");
            } else if (typeStr == "color") {
                def.type = ParamType::COLOR;
                sol::optional<sol::table> cv = t["default"];
                if (cv) { def.colorVal[0] = (*cv)[1]; def.colorVal[1] = (*cv)[2]; def.colorVal[2] = (*cv)[3]; }
            } else if (typeStr == "vec3") {
                def.type = ParamType::VEC3;
                sol::optional<sol::table> vv = t["default"];
                if (vv) { def.vec3Val[0] = (*vv)[1]; def.vec3Val[1] = (*vv)[2]; def.vec3Val[2] = (*vv)[3]; }
            } else {
                // String-valued types: string, mesh, texture, material, shader, particle, compositor
                if (typeStr == "mesh")            def.type = ParamType::MESH;
                else if (typeStr == "texture")    def.type = ParamType::TEXTURE;
                else if (typeStr == "material")   def.type = ParamType::MATERIAL;
                else if (typeStr == "shader")     def.type = ParamType::SHADER;
                else if (typeStr == "particle")   def.type = ParamType::PARTICLE;
                else if (typeStr == "compositor") def.type = ParamType::COMPOSITOR;
                else                              def.type = ParamType::STRING;
                sol::optional<std::string> v = t["default"]; def.stringVal = v.value_or("");
            }

            sol::optional<float> minOpt = t["min"]; def.minVal = minOpt.value_or(0.0f);
            sol::optional<float> maxOpt = t["max"]; def.maxVal = maxOpt.value_or(10.0f);
            sol::optional<float> stepOpt = t["step"]; def.stepVal = stepOpt.value_or(0.01f);

            sol::optional<sol::table> choices = t["choices"];
            if (choices) {
                for (auto& kv : *choices) def.choices.push_back(kv.second.as<std::string>());
            }
            self.addParam(def);
        };
        paramSpecType["getFloat"] = [](ParamSpec& self, const std::string& name) -> float {
            auto* p = self.getParam(name);
            return p ? p->floatVal : 0.0f;
        };
        paramSpecType["getInt"] = [](ParamSpec& self, const std::string& name) -> int {
            auto* p = self.getParam(name);
            return p ? p->intVal : 0;
        };
        paramSpecType["getBool"] = [](ParamSpec& self, const std::string& name) -> bool {
            auto* p = self.getParam(name);
            return p ? p->boolVal : false;
        };
        paramSpecType["getString"] = [](ParamSpec& self, const std::string& name) -> std::string {
            auto* p = self.getParam(name);
            return p ? p->stringVal : "";
        };
        paramSpecType["empty"] = &ParamSpec::empty;
        bbfx["ParamSpec"] = lua["bbfx_ParamSpec"];
    }

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

    // ── v3.5 Lot A: user-facing bbfx.plugin.* (introspection only). ──────
    //    The sandbox-facing bbfx.plugin.registerNodeType/etc. (Lot B) lives
    //    in a separate binding file (bbfx_plugin_bindings.cpp) and is only
    //    exposed inside each plugin's restricted sol::environment.
    auto pluginNs = bbfx.get_or("plugin", sol::table(lua, sol::create));
    bbfx["plugin"] = pluginNs;

    pluginNs["scan"] = []() -> size_t {
        PluginManager::instance().scanDirectories();
        return PluginManager::instance().listPlugins().size();
    };
    pluginNs["list"] = [&lua]() -> sol::table {
        sol::table out = lua.create_table();
        const auto ids = PluginManager::instance().listPlugins();
        int i = 1;
        for (const auto& id : ids) out[i++] = id;
        return out;
    };
    pluginNs["info"] = [&lua](const std::string& id) -> sol::object {
        const PluginInfo* p = PluginManager::instance().getPlugin(id);
        if (!p) return sol::nil;
        sol::table t = lua.create_table();
        t["id"]              = p->id;
        t["name"]            = p->manifest.name;
        t["version"]         = p->manifest.version;
        t["bbfx_version"]    = p->manifest.bbfxVersion;
        t["author"]          = p->manifest.author.name;
        t["description"]     = p->manifest.description;
        t["category"]        = p->manifest.category;
        t["license"]         = p->manifest.license;
        t["state"]           = toString(p->state);
        t["directory"]       = p->directoryPath.string();
        t["is_builtin"]      = p->isBuiltin;
        t["resource_group"]  = p->resourceGroupName;
        t["last_error"]      = p->lastError;
        sol::table perms = lua.create_table();
        int i = 1;
        for (auto pm : p->manifest.permissions) perms[i++] = toString(pm);
        t["permissions"] = perms;
        return t;
    };
    pluginNs["validatePath"] = [&lua](const std::string& path) -> sol::table {
        sol::table t = lua.create_table();
        auto r = PluginValidator::validatePath(std::filesystem::path(path));
        t["ok"] = r.ok;
        sol::table errs = lua.create_table();
        int i = 1;
        for (const auto& e : r.errors) errs[i++] = e;
        t["errors"] = errs;
        return t;
    };
    pluginNs["userDir"] = []() -> std::string {
        return PluginManager::instance().getUserPluginsDir().string();
    };
    pluginNs["bundledDir"] = []() -> std::string {
        return PluginManager::instance().getBundledPluginsDir().string();
    };
    pluginNs["currentBBFxVersion"] = []() -> std::string {
        return PluginValidator::currentBBFxVersion();
    };

    // v3.5 Lot B: lifecycle control from outside the sandbox
    pluginNs["load"] = [](const std::string& id) -> bool {
        return PluginManager::instance().load(id);
    };
    pluginNs["enable"] = [](const std::string& id) -> bool {
        return PluginManager::instance().enable(id);
    };
    pluginNs["disable"] = [](const std::string& id) -> bool {
        return PluginManager::instance().disable(id);
    };
    pluginNs["unload"] = [](const std::string& id) -> bool {
        return PluginManager::instance().unload(id);
    };

    // v3.5 Lot D: user-facing install/uninstall. ZIP install lands in Lot F.
    pluginNs["install"] = [&lua](const std::string& path) -> sol::object {
        std::string err;
        std::string id = PluginManager::instance().installFromPath(path, &err);
        if (id.empty()) {
            sol::table t = lua.create_table();
            t["ok"] = false;
            t["error"] = err;
            return t;
        }
        sol::table t = lua.create_table();
        t["ok"] = true;
        t["id"] = id;
        return t;
    };
    pluginNs["uninstall"] = [](const std::string& id) -> bool {
        return PluginManager::instance().uninstall(id);
    };

    // v3.5 Lot F: async install from URL. onComplete runs on the main
    // thread (marshalled by HttpClient::pumpMainThread()).
    pluginNs["installFromUrl"] = [](const std::string& url,
                                     sol::optional<std::string> sha256,
                                     sol::optional<sol::function> onCompleteLua) {
        std::string s = sha256.value_or("");
        sol::function cb = onCompleteLua.value_or(sol::function());
        PluginManager::instance().installFromUrl(url, s,
            [cb](bool ok, std::string id, std::string err) mutable {
                if (cb.valid()) cb(ok, id, err);
            });
    };

    // v3.5 Lot D: test helpers + startup parity. autoEnableFromSettings is
    // called by main/main_studio at boot, but exposing it to Lua lets tests
    // validate the persistence round-trip without restarting the process.
    pluginNs["autoEnableFromSettings"] = []() {
        PluginManager::instance().autoEnableFromSettings();
    };

    // ── v3.5 Lot E: bbfx.http.* (HTTP client) ─────────────────────────────
    // The user-facing namespace is permission-free at the Lua REPL level.
    // Inside a plugin sandbox the same functions are only exposed if the
    // plugin declared the `network` permission (Lot O wires that gating).
    auto httpNs = bbfx.get_or("http", sol::table(lua, sol::create));
    bbfx["http"] = httpNs;
    httpNs["get"] = [&lua](const std::string& url, sol::function cb, sol::optional<int> timeout) {
        int t = timeout.value_or(30);
        HttpClient::instance().get(url, [cb, &lua](HttpResponse r) mutable {
            if (!cb.valid()) return;
            sol::table t = lua.create_table();
            t["status"] = r.status;
            t["body"]   = r.body;
            t["bytes"]  = r.bytes;
            t["error"]  = r.error;
            sol::table h = lua.create_table();
            for (auto& kv : r.headers) h[kv.first] = kv.second;
            t["headers"] = h;
            cb(t);
        }, t);
    };
    httpNs["getSync"] = [&lua](const std::string& url, sol::optional<int> timeout) -> sol::table {
        int t = timeout.value_or(30);
        HttpResponse r = HttpClient::instance().getSync(url, t);
        sol::table tbl = lua.create_table();
        tbl["status"] = r.status;
        tbl["body"]   = r.body;
        tbl["bytes"]  = r.bytes;
        tbl["error"]  = r.error;
        sol::table h = lua.create_table();
        for (auto& kv : r.headers) h[kv.first] = kv.second;
        tbl["headers"] = h;
        return tbl;
    };
    httpNs["post"] = [&lua](const std::string& url, sol::table opts, sol::function cb) {
        std::string body = opts["body"].get_or<std::string>("");
        std::map<std::string, std::string> headers;
        sol::optional<sol::table> ht = opts["headers"];
        if (ht) {
            for (auto& kv : *ht) {
                if (kv.first.is<std::string>() && kv.second.is<std::string>()) {
                    headers[kv.first.as<std::string>()] = kv.second.as<std::string>();
                }
            }
        }
        int timeout = opts["timeout"].get_or(30);
        HttpClient::instance().post(url, body, headers, [cb, &lua](HttpResponse r) mutable {
            if (!cb.valid()) return;
            sol::table t = lua.create_table();
            t["status"] = r.status;
            t["body"]   = r.body;
            t["bytes"]  = r.bytes;
            t["error"]  = r.error;
            sol::table h = lua.create_table();
            for (auto& kv : r.headers) h[kv.first] = kv.second;
            t["headers"] = h;
            cb(t);
        }, timeout);
    };
    httpNs["download"] = [](const std::string& url, const std::string& destPath, sol::table opts) {
        HttpDownloadRequest req;
        req.url = url;
        req.destPath = destPath;
        req.expectedSha256 = opts["sha256"].get_or<std::string>("");
        req.timeoutSeconds = opts["timeout"].get_or(300);
        sol::optional<sol::function> onProgress = opts["onProgress"];
        sol::optional<sol::function> onComplete = opts["onComplete"];
        if (onProgress) {
            sol::function pf = *onProgress;
            req.onProgress = [pf](size_t d, size_t t) mutable {
                if (pf.valid()) pf(double(d), double(t));
            };
        }
        if (onComplete) {
            sol::function cf = *onComplete;
            req.onComplete = [cf](bool ok, std::string path, std::string err) mutable {
                if (cf.valid()) cf(ok, path, err);
            };
        }
        HttpClient::instance().download(req);
    };
    httpNs["sha256File"] = [](const std::string& path) -> std::string {
        return HttpClient::sha256File(path);
    };
    httpNs["pump"] = []() { HttpClient::instance().pumpMainThread(); };
    httpNs["waitIdle"] = [](sol::optional<int> timeout) -> bool {
        return HttpClient::instance().waitIdle(timeout.value_or(30));
    };

    // ── v3.5 Lot E: bbfx.websocket.* ──────────────────────────────────────
    auto wsNs = bbfx.get_or("websocket", sol::table(lua, sol::create));
    bbfx["websocket"] = wsNs;
    wsNs["connect"] = [&lua](const std::string& url, sol::table cbs)
                          -> std::shared_ptr<WebSocketConnection> {
        WebSocketCallbacks c;
        if (auto o = cbs.get<sol::optional<sol::function>>("onOpen"); o) {
            sol::function f = *o;
            c.onOpen = [f]() mutable { if (f.valid()) f(); };
        }
        if (auto o = cbs.get<sol::optional<sol::function>>("onMessage"); o) {
            sol::function f = *o;
            c.onMessage = [f](const std::string& msg, bool binary) mutable {
                if (f.valid()) f(msg, binary);
            };
        }
        if (auto o = cbs.get<sol::optional<sol::function>>("onClose"); o) {
            sol::function f = *o;
            c.onClose = [f](int code, const std::string& reason) mutable {
                if (f.valid()) f(code, reason);
            };
        }
        if (auto o = cbs.get<sol::optional<sol::function>>("onError"); o) {
            sol::function f = *o;
            c.onError = [f](const std::string& err) mutable {
                if (f.valid()) f(err);
            };
        }
        return WebSocketClient::instance().connect(url, std::move(c));
    };
    lua.new_usertype<WebSocketConnection>("bbfx_WebSocket",
        sol::no_constructor,
        "send", &WebSocketConnection::send,
        "sendBinary", &WebSocketConnection::sendBinary,
        "close", [](WebSocketConnection& self, sol::optional<int> code, sol::optional<std::string> reason) {
            self.close(code.value_or(1000), reason.value_or(""));
        },
        "isSupported", &WebSocketConnection::isSupported,
        "lastError", &WebSocketConnection::lastError
    );

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot O — Filesystem + JSON bindings
    // ══════════════════════════════════════════════════════════════════════
    // `bbfx.fs.*` operates on absolute paths outside the sandbox. Inside a
    // plugin's sandbox, PluginSandboxApi::installSandboxPluginApi overrides
    // the whole table with plugin-dir-relative wrappers (same pattern as
    // the `plugin` authoring API). That way test scripts can exercise the
    // raw API while plugins remain confined.

    auto fsNs = bbfx.get_or("fs", sol::table(lua, sol::create));
    bbfx["fs"] = fsNs;

    fsNs["readFile"] = [&lua](const std::string& path) -> sol::object {
        std::ifstream f(path, std::ios::binary);
        if (!f) return sol::make_object(lua, sol::nil);
        std::string body((std::istreambuf_iterator<char>(f)), {});
        return sol::make_object(lua, body);
    };
    fsNs["writeFile"] = [](const std::string& path, const std::string& content) -> bool {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        return static_cast<bool>(f);
    };
    fsNs["readLines"] = [&lua](const std::string& path) -> sol::table {
        sol::table out = lua.create_table();
        std::ifstream f(path);
        if (!f) return out;
        std::string line;
        int i = 1;
        while (std::getline(f, line)) out[i++] = line;
        return out;
    };
    fsNs["exists"] = [](const std::string& path) -> bool {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    };
    fsNs["listDir"] = [&lua](const std::string& path) -> sol::table {
        sol::table out = lua.create_table();
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec)) return out;
        int i = 1;
        for (auto& e : std::filesystem::directory_iterator(path, ec)) {
            out[i++] = e.path().filename().string();
        }
        return out;
    };

    // ── bbfx.json.* — encode/decode via nlohmann::json ───────────────────
    auto jsonNs = bbfx.get_or("json", sol::table(lua, sol::create));
    bbfx["json"] = jsonNs;

    // Forward declarations for recursive table <-> json conversion.
    struct JsonConv {
        static nlohmann::json fromLua(sol::object o) {
            using nlohmann::json;
            if (o.get_type() == sol::type::nil)      return nullptr;
            if (o.is<bool>())                          return o.as<bool>();
            if (o.is<int64_t>())                       return o.as<int64_t>();
            if (o.is<double>())                        return o.as<double>();
            if (o.is<std::string>())                   return o.as<std::string>();
            if (o.is<sol::table>()) {
                sol::table t = o.as<sol::table>();
                // Heuristic : table is an array iff it has only integer
                // keys 1..n with nothing missing.
                bool isArray = true;
                size_t count = 0;
                for (auto& kv : t) {
                    ++count;
                    if (!kv.first.is<int64_t>()) { isArray = false; break; }
                }
                if (isArray && count == t.size()) {
                    json arr = json::array();
                    for (size_t i = 1; i <= count; ++i) {
                        arr.push_back(fromLua(t[i]));
                    }
                    return arr;
                }
                json obj = json::object();
                for (auto& kv : t) {
                    std::string key = kv.first.is<std::string>()
                        ? kv.first.as<std::string>()
                        : (kv.first.is<int64_t>()
                            ? std::to_string(kv.first.as<int64_t>())
                            : std::string());
                    if (!key.empty()) obj[key] = fromLua(kv.second);
                }
                return obj;
            }
            return nullptr;
        }
        static sol::object toLua(sol::state_view lua, const nlohmann::json& j) {
            using nlohmann::json;
            if (j.is_null())    return sol::make_object(lua, sol::nil);
            if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
            if (j.is_number_integer())
                                return sol::make_object(lua, j.get<int64_t>());
            if (j.is_number())  return sol::make_object(lua, j.get<double>());
            if (j.is_string())  return sol::make_object(lua, j.get<std::string>());
            if (j.is_array()) {
                sol::table t = lua.create_table();
                int i = 1;
                for (auto& v : j) t[i++] = toLua(lua, v);
                return t;
            }
            if (j.is_object()) {
                sol::table t = lua.create_table();
                for (auto it = j.begin(); it != j.end(); ++it) {
                    t[it.key()] = toLua(lua, it.value());
                }
                return t;
            }
            return sol::make_object(lua, sol::nil);
        }
    };

    jsonNs["encode"] = [](sol::object o) -> std::string {
        try { return JsonConv::fromLua(o).dump(); }
        catch (const std::exception& e) {
            std::cerr << "[bbfx.json.encode] " << e.what() << std::endl;
            return "null";
        }
    };
    jsonNs["decode"] = [&lua](const std::string& s) -> sol::object {
        try {
            nlohmann::json j = nlohmann::json::parse(s);
            return JsonConv::toLua(lua, j);
        } catch (const std::exception& e) {
            std::cerr << "[bbfx.json.decode] " << e.what() << std::endl;
            return sol::make_object(lua, sol::nil);
        }
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot Q — Media + Images + Sequences + Models
    // ══════════════════════════════════════════════════════════════════════

    // ── bbfx.media.* — video playback via ffmpeg subprocess ──────────────
    auto mediaNs = bbfx.get_or("media", sol::table(lua, sol::create));
    bbfx["media"] = mediaNs;

    mediaNs["ffmpegAvailable"] = []() -> bool {
        auto p = FFmpegBridge::findFFmpegBinary();
        return !p.empty();
    };
    mediaNs["openVideo"] = [&lua](const std::string& path, sol::optional<sol::table> opts)
                              -> sol::object {
        int w = 1280, h = 720; double fps = 30.0;
        if (opts) {
            if (auto v = opts->get<sol::optional<int>>("width"))  w = *v;
            if (auto v = opts->get<sol::optional<int>>("height")) h = *v;
            if (auto v = opts->get<sol::optional<double>>("fps")) fps = *v;
        }
        auto bridge = std::make_shared<FFmpegBridge>();
        bool ok = bridge->open(path, w, h, fps);
        sol::table handle = lua.create_table();
        handle["_impl"] = bridge;
        handle["ok"]    = ok;
        handle["play"]  = [bridge]()              { bridge->play();  };
        handle["pause"] = [bridge]()              { bridge->pause(); };
        handle["stop"]  = [bridge]()              { bridge->stop();  };
        handle["seek"]  = [bridge](double s)      { bridge->seek(s); };
        handle["setSpeed"]= [bridge](double m)    { bridge->setSpeed(m); };
        handle["setLoop"] = [bridge](bool on)     { bridge->setLoop(on);  };
        handle["update"]  = [bridge](float dt)    { bridge->update(dt); };
        handle["close"]   = [bridge]()            { bridge->close(); };
        handle["getTextureName"] = [bridge]()     { return bridge->getTextureName(); };
        handle["isOpen"]   = [bridge]()           { return bridge->isOpen(); };
        handle["isPlaying"]= [bridge]()           { return bridge->isPlaying(); };
        handle["getWidth"] = [bridge]()           { return bridge->getWidth(); };
        handle["getHeight"]= [bridge]()           { return bridge->getHeight(); };
        handle["getFPS"]   = [bridge]()           { return bridge->getFPS(); };
        return handle;
    };

    // ── bbfx.images.* — static image loader ───────────────────────────────
    auto imagesNs = bbfx.get_or("images", sol::table(lua, sol::create));
    bbfx["images"] = imagesNs;
    imagesNs["load"] = [&lua](const std::string& path) -> sol::object {
        std::string name = ImageLoader::load(path);
        if (name.empty()) return sol::make_object(lua, sol::nil);
        sol::table handle = lua.create_table();
        handle["_name"] = name;
        handle["getTextureName"] = [name]() { return name; };
        handle["release"] = [name]() { ImageLoader::release(name); };
        return handle;
    };

    // ── bbfx.sequences.* — GIF + PNG sequences ────────────────────────────
    auto sequencesNs = bbfx.get_or("sequences", sol::table(lua, sol::create));
    bbfx["sequences"] = sequencesNs;

    auto makeSequenceHandle = [&lua](std::shared_ptr<SequencePlayer> sp) -> sol::table {
        sol::table h = lua.create_table();
        h["_impl"]     = sp;
        h["setFPS"]    = [sp](float f)       { sp->setFPS(f); };
        h["getFPS"]    = [sp]()              { return sp->getFPS(); };
        h["setLoop"]   = [sp](bool on)       { sp->setLoop(on); };
        h["play"]      = [sp]()              { sp->play(); };
        h["pause"]     = [sp]()              { sp->pause(); };
        h["stop"]      = [sp]()              { sp->stop(); };
        h["update"]    = [sp](float dt)      { sp->update(dt); };
        h["release"]   = [sp]()              { sp->release(); };
        h["getTextureName"] = [sp]()         { return sp->getTextureName(); };
        h["frameCount"]     = [sp]()         { return sp->frameCount(); };
        h["currentIndex"]   = [sp]()         { return sp->currentIndex(); };
        h["isPlaying"]      = [sp]()         { return sp->isPlaying(); };
        h["backend"]        = [sp]() -> std::string { return sp->backendName(); };
        return h;
    };

    sequencesNs["loadGif"] = [makeSequenceHandle](const std::string& path) -> sol::table {
        auto sp = std::make_shared<SequencePlayer>();
        sp->loadGif(path);
        return makeSequenceHandle(sp);
    };
    sequencesNs["loadSequence"] = [makeSequenceHandle](const std::string& dir,
                                                            const std::string& pattern,
                                                            int start, int end) -> sol::table {
        auto sp = std::make_shared<SequencePlayer>();
        sp->loadSequence(dir, pattern, start, end);
        return makeSequenceHandle(sp);
    };

    // ── bbfx.models.* — 3D model import via Assimp ────────────────────────
    auto modelsNs = bbfx.get_or("models", sol::table(lua, sol::create));
    bbfx["models"] = modelsNs;
    modelsNs["isAvailable"] = []() -> bool { return MeshImporter::isAvailable(); };
    modelsNs["import"] = [&lua](const std::string& path) -> sol::object {
        std::string name = MeshImporter::import(path);
        if (name.empty()) return sol::make_object(lua, sol::nil);
        sol::table handle = lua.create_table();
        handle["_name"] = name;
        handle["getMeshName"] = [name]() { return name; };
        return handle;
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot R — Geometry / SDF / Fractals / L-system
    // ══════════════════════════════════════════════════════════════════════

    auto geomNs = bbfx.get_or("geometry", sol::table(lua, sol::create));
    bbfx["geometry"] = geomNs;

    geomNs["createMesh"] = [](const std::string& name,
                                sol::table vertsTbl, sol::table idxTbl,
                                sol::optional<std::string> material) -> std::string {
        std::vector<float> verts;
        verts.reserve(vertsTbl.size());
        for (auto& kv : vertsTbl) {
            if (kv.second.is<double>())      verts.push_back(static_cast<float>(kv.second.as<double>()));
            else if (kv.second.is<int>())    verts.push_back(static_cast<float>(kv.second.as<int>()));
        }
        std::vector<uint32_t> idx;
        idx.reserve(idxTbl.size());
        for (auto& kv : idxTbl) {
            if (kv.second.is<int>())         idx.push_back(static_cast<uint32_t>(kv.second.as<int>()));
            else if (kv.second.is<double>()) idx.push_back(static_cast<uint32_t>(kv.second.as<double>()));
        }
        return GeometryGenerator::createMesh(name, verts, idx,
                                                material ? *material : std::string());
    };
    geomNs["updateVertices"] = [](const std::string& meshName, sol::table vertsTbl) -> bool {
        std::vector<float> verts;
        verts.reserve(vertsTbl.size());
        for (auto& kv : vertsTbl) {
            if (kv.second.is<double>())      verts.push_back(static_cast<float>(kv.second.as<double>()));
            else if (kv.second.is<int>())    verts.push_back(static_cast<float>(kv.second.as<int>()));
        }
        return GeometryGenerator::updateVertices(meshName, verts);
    };
    geomNs["createSphere"] = [](const std::string& name, float r, int rings, int segs) {
        return GeometryGenerator::createSphere(name, r, rings, segs);
    };
    geomNs["createPlane"] = [](const std::string& name, float w, float h, int sx, int sy) {
        return GeometryGenerator::createPlane(name, w, h, sx, sy);
    };
    geomNs["createCylinder"] = [](const std::string& name, float r, float h, int segs) {
        return GeometryGenerator::createCylinder(name, r, h, segs);
    };
    geomNs["createTorus"] = [](const std::string& name, float major, float minor,
                                  int rings, int segs) {
        return GeometryGenerator::createTorus(name, major, minor, rings, segs);
    };

    // ── bbfx.sdf.* — signed-distance-field primitives + booleans + mesher
    auto sdfNs = bbfx.get_or("sdf", sol::table(lua, sol::create));
    bbfx["sdf"] = sdfNs;

    sdfNs["sphere"] = &SDFPrimitives::sphere;
    sdfNs["box"]    = &SDFPrimitives::box;
    sdfNs["torus"]  = &SDFPrimitives::torus;
    sdfNs["opUnion"]        = &SDFPrimitives::opUnion;
    sdfNs["opIntersection"] = &SDFPrimitives::opIntersection;
    sdfNs["opSubtraction"]  = &SDFPrimitives::opSubtraction;
    sdfNs["opSmoothUnion"]  = &SDFPrimitives::opSmoothUnion;
    sdfNs["toMesh"] = [](const std::string& meshName, sol::function field,
                           float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           int resolution) -> std::string {
        auto fn = [field](float x, float y, float z) -> float {
            try {
                sol::object r = field(x, y, z);
                if (r.is<float>())  return r.as<float>();
                if (r.is<double>()) return static_cast<float>(r.as<double>());
            } catch (const std::exception& e) {
                std::cerr << "[bbfx.sdf.toMesh] field error: " << e.what() << std::endl;
            }
            return 0.0f;
        };
        return SDFPrimitives::toMesh(meshName, fn,
                                        x0, y0, z0, x1, y1, z1, resolution);
    };

    // ── bbfx.fractals.mandelbrot / julia ─────────────────────────────────
    auto fractalsNs = bbfx.get_or("fractals", sol::table(lua, sol::create));
    bbfx["fractals"] = fractalsNs;

    fractalsNs["mandelbrot"] = [](int w, int h, sol::optional<sol::table> opts) -> std::string {
        double cx = -0.5, cy = 0.0, zoom = 1.0; int maxIter = 100;
        std::string scheme = "rainbow";
        if (opts) {
            if (auto v = opts->get<sol::optional<double>>("centerX")) cx = *v;
            if (auto v = opts->get<sol::optional<double>>("centerY")) cy = *v;
            if (auto v = opts->get<sol::optional<double>>("zoom"))    zoom = *v;
            if (auto v = opts->get<sol::optional<int>>("maxIter"))    maxIter = *v;
            if (auto v = opts->get<sol::optional<std::string>>("colorScheme")) scheme = *v;
        }
        return NoiseGenerator::mandelbrotTexture(w, h, cx, cy, zoom, maxIter, scheme);
    };
    fractalsNs["julia"] = [](int w, int h, sol::optional<sol::table> opts) -> std::string {
        double cr = -0.8, ci = 0.156, zoom = 1.0; int maxIter = 100;
        std::string scheme = "rainbow";
        if (opts) {
            if (auto v = opts->get<sol::optional<double>>("cReal"))  cr = *v;
            if (auto v = opts->get<sol::optional<double>>("cImag"))  ci = *v;
            if (auto v = opts->get<sol::optional<double>>("zoom"))   zoom = *v;
            if (auto v = opts->get<sol::optional<int>>("maxIter"))   maxIter = *v;
            if (auto v = opts->get<sol::optional<std::string>>("colorScheme")) scheme = *v;
        }
        return NoiseGenerator::juliaTexture(w, h, cr, ci, zoom, maxIter, scheme);
    };

    // ── bbfx.lsystem.* — L-system + turtle mesh generator ─────────────────
    auto lsysNs = bbfx.get_or("lsystem", sol::table(lua, sol::create));
    bbfx["lsystem"] = lsysNs;

    lsysNs["create"] = [&lua](sol::table opts) -> sol::table {
        auto ls = std::make_shared<LSystem>();
        if (auto v = opts.get<sol::optional<std::string>>("axiom")) ls->setAxiom(*v);
        if (auto v = opts.get<sol::optional<int>>("iterations"))    ls->setIterations(*v);
        if (auto v = opts.get<sol::optional<float>>("angle"))       ls->setAngleDegrees(*v);
        if (auto v = opts.get<sol::optional<float>>("step"))        ls->setStepLength(*v);
        if (auto rules = opts.get<sol::optional<sol::table>>("rules")) {
            for (auto& kv : *rules) {
                if (kv.first.is<std::string>() && kv.second.is<std::string>()) {
                    auto from = kv.first.as<std::string>();
                    if (!from.empty()) ls->addRule(from[0], kv.second.as<std::string>());
                }
            }
        }
        sol::table h = lua.create_table();
        h["_impl"] = ls;
        h["derive"]         = [ls]()                      { return ls->derive(); };
        h["generateMesh"]   = [ls](const std::string& n)  { return ls->generateMesh(n); };
        h["setIterations"]  = [ls](int n)                 { ls->setIterations(n); };
        h["setAngle"]       = [ls](float a)               { ls->setAngleDegrees(a); };
        h["setStep"]        = [ls](float s)               { ls->setStepLength(s); };
        return h;
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot T — RenderTexture + FrameBuffer + Compositor Lua API
    // ══════════════════════════════════════════════════════════════════════

    auto rtNs = bbfx.get_or("renderTexture", sol::table(lua, sol::create));
    bbfx["renderTexture"] = rtNs;

    rtNs["create"] = [&lua](const std::string& name, int w, int h,
                              sol::optional<sol::table> optsTbl) -> sol::object {
        if (w <= 0 || h <= 0) return sol::make_object(lua, sol::nil);
        std::string fmt = "RGBA8";
        int  msaa = 0;
        bool depth = true;
        if (optsTbl) {
            if (auto v = optsTbl->get<sol::optional<std::string>>("format")) fmt = *v;
            if (auto v = optsTbl->get<sol::optional<int>>("msaa"))           msaa = *v;
            if (auto v = optsTbl->get<sol::optional<bool>>("depthBuffer"))   depth = *v;
        }
        Ogre::PixelFormat pf = (fmt == "RGBA16F") ? Ogre::PF_FLOAT16_RGBA
                                                   : Ogre::PF_A8R8G8B8;
        auto& tm = Ogre::TextureManager::getSingleton();
        try {
            if (auto existing = tm.getByName(name); existing) tm.remove(existing);
            auto tex = tm.createManual(name,
                         Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                         Ogre::TEX_TYPE_2D, w, h, 0, pf,
                         Ogre::TU_RENDERTARGET, nullptr, false, msaa);
            auto* rt = tex->getBuffer()->getRenderTarget();
            if (!rt) {
                std::cerr << "[bbfx.renderTexture] no render target on " << name << std::endl;
                return sol::make_object(lua, sol::nil);
            }
            // Provide a viewport with a black clear colour ; the caller
            // will bind a camera via target:setCamera(name).
            if (rt->getNumViewports() == 0) {
                rt->addViewport(nullptr);
            }
            rt->getViewport(0)->setBackgroundColour(Ogre::ColourValue::Black);
            (void)depth;  // OGRE picks an auto depth buffer by default.

            sol::table h = lua.create_table();
            h["_name"] = name;
            h["getTextureName"] = [name]() { return name; };
            h["getWidth"]  = [w]() { return w; };
            h["getHeight"] = [hh = h]() { return hh; };  // capture height value
            h["setCamera"] = [name](const std::string& cameraName) -> bool {
                auto* eng = Engine::instance();
                auto* sm = eng ? eng->getSceneManager() : nullptr;
                if (!sm || !sm->hasCamera(cameraName)) return false;
                auto& tm2 = Ogre::TextureManager::getSingleton();
                auto tex = tm2.getByName(name);
                if (!tex) return false;
                auto* rt2 = tex->getBuffer()->getRenderTarget();
                if (!rt2) return false;
                if (rt2->getNumViewports() > 0)
                    rt2->removeViewport(rt2->getViewport(0)->getZOrder());
                rt2->addViewport(sm->getCamera(cameraName));
                rt2->getViewport(0)->setBackgroundColour(Ogre::ColourValue::Black);
                return true;
            };
            h["update"] = [name]() {
                auto& tm2 = Ogre::TextureManager::getSingleton();
                auto tex = tm2.getByName(name);
                if (!tex) return;
                auto* rt2 = tex->getBuffer()->getRenderTarget();
                if (rt2) rt2->update();
            };
            h["readPixels"] = [name, w, h_val = h, &lua]() mutable -> sol::table {
                sol::table out = lua.create_table();
                (void)w; (void)h_val;
                auto& tm2 = Ogre::TextureManager::getSingleton();
                auto tex = tm2.getByName(name);
                if (!tex) return out;
                try {
                    int tw = static_cast<int>(tex->getWidth());
                    int th = static_cast<int>(tex->getHeight());
                    Ogre::Image img;
                    img.loadDynamicImage(nullptr, tw, th, 1, Ogre::PF_A8R8G8B8);
                    std::vector<uint8_t> buf(static_cast<size_t>(tw) * th * 4);
                    Ogre::PixelBox pb(tw, th, 1, Ogre::PF_A8R8G8B8, buf.data());
                    tex->getBuffer()->blitToMemory(pb);
                    // Flatten to a contiguous array of ints in the Lua table.
                    int i = 1;
                    for (auto v : buf) out[i++] = static_cast<int>(v);
                } catch (const std::exception& e) {
                    std::cerr << "[bbfx.renderTexture] readPixels failed: "
                               << e.what() << std::endl;
                }
                return out;
            };
            h["release"] = [name]() {
                auto& tm2 = Ogre::TextureManager::getSingleton();
                if (auto tex = tm2.getByName(name); tex) tm2.remove(tex);
            };
            return h;
        } catch (const std::exception& e) {
            std::cerr << "[bbfx.renderTexture] create failed: " << e.what() << std::endl;
            return sol::make_object(lua, sol::nil);
        }
    };

    // ── bbfx.frameBuffer.* ───────────────────────────────────────────────
    auto fbNs = bbfx.get_or("frameBuffer", sol::table(lua, sol::create));
    bbfx["frameBuffer"] = fbNs;

    fbNs["saveToFile"] = [](const std::string& path,
                               sol::optional<std::string> textureName) -> bool {
        auto& tm = Ogre::TextureManager::getSingleton();
        std::string name = textureName ? *textureName : std::string();
        if (name.empty()) {
            // No explicit source — try the main render target via Engine.
            auto* eng = Engine::instance();
            if (!eng) return false;
            auto* rw = eng->getRenderWindow();
            if (!rw) return false;
            try { rw->writeContentsToFile(path); return true; }
            catch (...) { return false; }
        }
        auto tex = tm.getByName(name);
        if (!tex) return false;
        try {
            int w = static_cast<int>(tex->getWidth());
            int h = static_cast<int>(tex->getHeight());
            std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
            Ogre::PixelBox pb(w, h, 1, Ogre::PF_A8R8G8B8, buf.data());
            tex->getBuffer()->blitToMemory(pb);
            Ogre::Image img;
            img.loadDynamicImage(buf.data(), w, h, 1, Ogre::PF_A8R8G8B8);
            img.save(path);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[bbfx.frameBuffer] saveToFile failed: " << e.what() << std::endl;
            return false;
        }
    };
    fbNs["getPixel"] = [](const std::string& textureName, int x, int y)
                           -> std::tuple<int, int, int, int> {
        auto& tm = Ogre::TextureManager::getSingleton();
        auto tex = tm.getByName(textureName);
        if (!tex) return { 0, 0, 0, 0 };
        int w = static_cast<int>(tex->getWidth());
        int h = static_cast<int>(tex->getHeight());
        if (x < 0 || x >= w || y < 0 || y >= h) return { 0, 0, 0, 0 };
        try {
            std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
            Ogre::PixelBox pb(w, h, 1, Ogre::PF_A8R8G8B8, buf.data());
            tex->getBuffer()->blitToMemory(pb);
            size_t o = (static_cast<size_t>(y) * w + x) * 4;
            // PF_A8R8G8B8 on little-endian = BGRA bytes.
            return { buf[o + 2], buf[o + 1], buf[o + 0], buf[o + 3] };
        } catch (...) { return { 0, 0, 0, 0 }; }
    };
    fbNs["getResolution"] = [](sol::optional<std::string> textureName)
                                 -> std::tuple<int, int> {
        if (textureName) {
            auto& tm = Ogre::TextureManager::getSingleton();
            auto tex = tm.getByName(*textureName);
            if (tex) return { (int)tex->getWidth(), (int)tex->getHeight() };
            return { 0, 0 };
        }
        auto* eng = Engine::instance();
        auto* rw = eng ? eng->getRenderWindow() : nullptr;
        if (!rw) return { 0, 0 };
        return { (int)rw->getWidth(), (int)rw->getHeight() };
    };

    // ── bbfx.compositor.* ───────────────────────────────────────────────
    auto compNs = bbfx.get_or("compositor", sol::table(lua, sol::create));
    bbfx["compositor"] = compNs;

    compNs["enable"] = [](const std::string& name) -> bool {
        auto* eng = Engine::instance();
        auto* rw = eng ? eng->getRenderWindow() : nullptr;
        if (!rw || rw->getNumViewports() == 0) return false;
        try {
            auto& cm = Ogre::CompositorManager::getSingleton();
            auto* vp = rw->getViewport(0);
            if (!cm.getByName(name)) return false;
            cm.addCompositor(vp, name);
            cm.setCompositorEnabled(vp, name, true);
            return true;
        } catch (...) { return false; }
    };
    compNs["disable"] = [](const std::string& name) -> bool {
        auto* eng = Engine::instance();
        auto* rw = eng ? eng->getRenderWindow() : nullptr;
        if (!rw || rw->getNumViewports() == 0) return false;
        try {
            auto& cm = Ogre::CompositorManager::getSingleton();
            auto* vp = rw->getViewport(0);
            cm.setCompositorEnabled(vp, name, false);
            cm.removeCompositor(vp, name);
            return true;
        } catch (...) { return false; }
    };
    compNs["listAvailable"] = [&lua]() -> sol::table {
        sol::table t = lua.create_table();
        int i = 1;
        try {
            auto& cm = Ogre::CompositorManager::getSingleton();
            auto it = cm.getResourceIterator();
            while (it.hasMoreElements()) {
                auto res = it.getNext();
                if (res) t[i++] = res->getName();
            }
        } catch (...) {}
        return t;
    };
    compNs["registerCustom"] = [](const std::string& name,
                                      const std::string& scriptBody) -> bool {
        // Parse an inline compositor script via OGRE's script loader. The
        // body must be a valid .compositor text. On success the compositor
        // becomes callable with enable(name).
        try {
            auto stream = std::make_shared<Ogre::MemoryDataStream>(
                const_cast<char*>(scriptBody.data()), scriptBody.size(), false);
            Ogre::DataStreamPtr dp(stream);
            Ogre::CompositorManager::getSingleton().parseScript(
                dp, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            (void)name;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[bbfx.compositor] registerCustom failed: " << e.what() << std::endl;
            return false;
        }
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot S — Plugin Authoring backend
    // ══════════════════════════════════════════════════════════════════════

    auto authNs = bbfx.get_or("authoring", sol::table(lua, sol::create));
    bbfx["authoring"] = authNs;

    auto luaToMeta = [](const sol::table& t) {
        PluginAuthoringBackend::Metadata m;
        if (auto v = t.get<sol::optional<std::string>>("id"))            m.id          = *v;
        if (auto v = t.get<sol::optional<std::string>>("name"))          m.name        = *v;
        if (auto v = t.get<sol::optional<std::string>>("version"))       m.version     = *v;
        if (auto v = t.get<sol::optional<std::string>>("author"))        m.author      = *v;
        if (auto v = t.get<sol::optional<std::string>>("description"))   m.description = *v;
        if (auto v = t.get<sol::optional<std::string>>("license"))       m.license     = *v;
        if (auto v = t.get<sol::optional<std::string>>("category"))      m.category    = *v;
        if (auto v = t.get<sol::optional<std::string>>("bbfx_version"))  m.bbfxVersion = *v;
        if (auto tags = t.get<sol::optional<sol::table>>("tags")) {
            for (auto& kv : *tags) {
                if (kv.second.is<std::string>()) m.tags.emplace_back(kv.second.as<std::string>());
            }
        }
        if (auto perms = t.get<sol::optional<sol::table>>("permissions")) {
            for (auto& kv : *perms) {
                if (kv.second.is<std::string>()) m.permissions.emplace_back(kv.second.as<std::string>());
            }
        }
        return m;
    };

    authNs["slugify"] = &PluginAuthoringBackend::slugify;
    authNs["isValidId"] = &PluginAuthoringBackend::isValidId;
    authNs["detectPermissions"] = [&lua](const std::string& luaSource) -> sol::table {
        sol::table t = lua.create_table();
        int i = 1;
        for (auto& p : PluginAuthoringBackend::detectPermissions(luaSource)) {
            t[i++] = p;
        }
        return t;
    };

    authNs["writePlugin"] = [luaToMeta](sol::table metaTbl,
                                            const std::string& initLuaBody,
                                            sol::optional<sol::table> extraTbl)
                                -> std::string {
        auto m = luaToMeta(metaTbl);
        std::vector<std::pair<std::string, std::string>> extra;
        if (extraTbl) {
            for (auto& kv : *extraTbl) {
                if (kv.first.is<std::string>() && kv.second.is<std::string>()) {
                    extra.emplace_back(kv.first.as<std::string>(),
                                        kv.second.as<std::string>());
                }
            }
        }
        auto dest = PluginManager::instance().getUserPluginsDir();
        auto path = PluginAuthoringBackend::writePlugin(dest, m, initLuaBody, extra);
        return path.string();
    };

    authNs["exportScenePreset"] = [luaToMeta](sol::table metaTbl, sol::table sceneTbl)
                                       -> std::string {
        auto m = luaToMeta(metaTbl);
        // Lua table -> json via existing JsonConv path : serialize via the
        // bbfx.json.encode binding for consistency.
        sol::state_view L(metaTbl.lua_state());
        sol::function encode = L["bbfx"]["json"]["encode"];
        std::string js = encode(sceneTbl).get<std::string>();
        auto path = PluginAuthoringBackend::exportScenePreset(m, nlohmann::json::parse(js));
        return path.string();
    };
    authNs["exportOutputTemplate"] = [luaToMeta](sol::table metaTbl, sol::table outTbl)
                                           -> std::string {
        auto m = luaToMeta(metaTbl);
        sol::state_view L(metaTbl.lua_state());
        sol::function encode = L["bbfx"]["json"]["encode"];
        std::string js = encode(outTbl).get<std::string>();
        auto path = PluginAuthoringBackend::exportOutputTemplate(m, nlohmann::json::parse(js));
        return path.string();
    };
    authNs["exportSubgraph"] = [luaToMeta](sol::table metaTbl, sol::table specTbl)
                                     -> std::string {
        auto m = luaToMeta(metaTbl);
        sol::state_view L(metaTbl.lua_state());
        sol::function encode = L["bbfx"]["json"]["encode"];
        std::string js = encode(specTbl).get<std::string>();
        auto path = PluginAuthoringBackend::exportSubgraph(m, nlohmann::json::parse(js));
        return path.string();
    };

    // v3.5 Lot U — hot reloader + validator bindings.
    authNs["validatePath"] = [&lua](const std::string& p) -> sol::table {
        auto res = PluginValidator::validatePath(p);
        sol::table t = lua.create_table();
        t["ok"] = res.ok;
        sol::table errs = lua.create_table();
        int i = 1;
        for (auto& e : res.errors) errs[i++] = e;
        t["errors"] = errs;
        return t;
    };

    auto hotNs = bbfx.get_or("hotreload", sol::table(lua, sol::create));
    bbfx["hotreload"] = hotNs;
    hotNs["setEnabled"] = [](bool on) { PluginHotReloader::instance().setEnabled(on); };
    hotNs["isEnabled"]  = []() { return PluginHotReloader::instance().isEnabled(); };
    hotNs["tick"]       = []() { PluginHotReloader::instance().tick(); };
    hotNs["invalidate"] = []() { PluginHotReloader::instance().invalidateAll(); };
    hotNs["watchedCount"] = []() -> int {
        return static_cast<int>(PluginHotReloader::instance().watchedFileCount());
    };
    hotNs["reloadsPerformed"] = []() {
        return PluginHotReloader::instance().reloadsPerformedSinceStart();
    };

    // ══════════════════════════════════════════════════════════════════════
    // v3.5 Lot V — GitHub publishing
    // ══════════════════════════════════════════════════════════════════════

    auto ghNs = bbfx.get_or("github", sol::table(lua, sol::create));
    bbfx["github"] = ghNs;

    static GitHubPublisher sPublisher;

    ghNs["beginDeviceFlow"] = [&lua]() -> sol::table {
        auto c = sPublisher.beginDeviceFlow();
        sol::table t = lua.create_table();
        t["deviceCode"]      = c.deviceCode;
        t["userCode"]        = c.userCode;
        t["verificationUri"] = c.verificationUri;
        t["interval"]        = c.interval;
        t["expiresIn"]       = c.expiresIn;
        t["error"]           = c.error;
        return t;
    };
    ghNs["pollDeviceFlow"] = [&lua](sol::table code) -> sol::table {
        GitHubPublisher::DeviceCode c;
        c.deviceCode      = code.get_or<std::string>("deviceCode", "");
        c.userCode        = code.get_or<std::string>("userCode", "");
        c.verificationUri = code.get_or<std::string>("verificationUri", "");
        auto t = sPublisher.pollDeviceFlow(c);
        sol::table o = lua.create_table();
        o["token"]   = t.token;
        o["pending"] = t.pending;
        o["error"]   = t.error;
        return o;
    };
    ghNs["storeToken"] = [](const std::string& token,
                              sol::optional<std::string> login) {
        sPublisher.storeToken(token, login ? *login : std::string());
    };
    ghNs["storedToken"] = []() { return sPublisher.storedToken(); };
    ghNs["storedLogin"] = []() { return sPublisher.storedLogin(); };
    ghNs["isAuthenticated"] = []() { return sPublisher.isAuthenticated(); };
    ghNs["whoami"] = [&lua]() -> sol::object {
        auto w = sPublisher.whoami();
        if (!w) return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, *w);
    };
    ghNs["forkUpstream"] = []() { return sPublisher.forkUpstream(); };
    ghNs["ensureBranch"] = [](const std::string& branch, const std::string& sha) {
        return sPublisher.ensureBranch(branch, sha);
    };
    ghNs["commitFile"] = [](const std::string& branch, const std::string& path,
                              const std::string& content, const std::string& msg,
                              sol::optional<std::string> prevSha) {
        return sPublisher.commitFile(branch, path, content, msg,
            prevSha ? std::optional<std::string>(*prevSha) : std::nullopt);
    };
    ghNs["openPullRequest"] = [&lua](const std::string& branch,
                                         const std::string& title,
                                         const std::string& body) -> sol::object {
        auto url = sPublisher.openPullRequest(branch, title, body);
        if (!url) return sol::make_object(lua, sol::nil);
        return sol::make_object(lua, *url);
    };
    // Token scramble helpers — useful for tests / CLI.
    ghNs["encodeToken"] = &GitHubPublisher::encodeToken;
    ghNs["decodeToken"] = &GitHubPublisher::decodeToken;

    // ── v3.5 Lot H: bbfx.community.* — lightweight Lua surface over
    //    CommunityIndex. The full UX lives in CommunityBrowserPanel; these
    //    bindings let scripts + tests exercise the index programmatically.
    auto communityNs = bbfx.get_or("community", sol::table(lua, sol::create));
    bbfx["community"] = communityNs;

    communityNs["size"] = []() -> size_t {
        return CommunityIndex::instance().size();
    };
    communityNs["loadFromString"] = [&lua](const std::string& jsonText) -> sol::object {
        std::string err;
        bool ok = CommunityIndex::instance().loadFromJsonString(jsonText, err);
        if (ok) return sol::make_object(lua, true);
        sol::table t = lua.create_table();
        t["ok"]    = false;
        t["error"] = err;
        return t;
    };
    communityNs["loadFromFile"] = [&lua](const std::string& path) -> sol::object {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            sol::table t = lua.create_table();
            t["ok"] = false;
            t["error"] = "cannot open file";
            return t;
        }
        std::stringstream b; b << f.rdbuf();
        std::string err;
        bool ok = CommunityIndex::instance().loadFromJsonString(b.str(), err);
        if (ok) return sol::make_object(lua, true);
        sol::table t = lua.create_table();
        t["ok"]    = false;
        t["error"] = err;
        return t;
    };
    communityNs["list"] = [&lua]() -> sol::table {
        sol::table out = lua.create_table();
        int i = 1;
        for (const auto& e : CommunityIndex::instance().entries()) out[i++] = e.id;
        return out;
    };
    communityNs["search"] = [&lua](const std::string& query) -> sol::table {
        CommunityIndex::Filter f; f.search = query;
        auto hits = CommunityIndex::instance().filtered(f);
        sol::table out = lua.create_table();
        int i = 1;
        for (const auto* e : hits) out[i++] = e->id;
        return out;
    };
    communityNs["info"] = [&lua](const std::string& id) -> sol::object {
        const auto* e = CommunityIndex::instance().findById(id);
        if (!e) return sol::nil;
        sol::table t = lua.create_table();
        t["id"] = e->id;
        t["name"] = e->name;
        t["version"] = e->version;
        t["author"] = e->author;
        t["description"] = e->description;
        t["category"] = e->category;
        t["license"] = e->license;
        t["installs"] = e->installs;
        t["rating"] = e->rating;
        t["ratingCount"] = e->ratingCount;
        t["featured"] = e->featured;
        t["downloadUrl"] = e->downloadUrl;
        t["sha256"] = e->sha256;
        sol::table tags = lua.create_table();
        int i = 1;
        for (const auto& tg : e->tags) tags[i++] = tg;
        t["tags"] = tags;
        return t;
    };
    communityNs["refresh"] = [](sol::function cb) {
        CommunityIndex::instance().refresh([cb](bool ok) mutable {
            if (cb.valid()) cb(ok);
        });
    };
    communityNs["indexUrl"] = []() -> std::string {
        return CommunityIndex::instance().indexUrl();
    };
    communityNs["setIndexUrl"] = [](const std::string& url) {
        CommunityIndex::instance().setIndexUrl(url);
    };
    communityNs["lastError"] = []() -> std::string {
        return CommunityIndex::instance().lastError();
    };

    // v3.5 Lot I — bbfx.deeplink.* : parse + dispatch deep links.
    auto deepNs = bbfx.get_or("deeplink", sol::table(lua, sol::create));
    bbfx["deeplink"] = deepNs;
    deepNs["parse"] = [&lua](const std::string& url) -> sol::table {
        auto a = DeepLinkHandler::parse(url);
        sol::table t = lua.create_table();
        const char* kind = "Unknown";
        switch (a.kind) {
            case DeepLinkHandler::Action::Kind::Install: kind = "Install"; break;
            case DeepLinkHandler::Action::Kind::Enable:  kind = "Enable";  break;
            case DeepLinkHandler::Action::Kind::Disable: kind = "Disable"; break;
            case DeepLinkHandler::Action::Kind::Run:     kind = "Run";     break;
            default: break;
        }
        t["kind"]     = kind;
        t["pluginId"] = a.pluginId;
        t["extra"]    = a.extra;
        t["rawUrl"]   = a.rawUrl;
        return t;
    };
    deepNs["dispatch"] = [](const std::string& url) {
        auto a = DeepLinkHandler::parse(url);
        DeepLinkHandler::instance().handle(a);
    };

    // v3.5 Lot I — bbfx.ratings.* — live rating via GitHub Reactions.
    auto ratingsNs = bbfx.get_or("ratings", sol::table(lua, sol::create));
    bbfx["ratings"] = ratingsNs;
    ratingsNs["cached"] = [&lua](int issue) -> sol::object {
        auto r = GithubReactionsFetcher::instance().cached(issue);
        if (!r.valid) return sol::nil;
        sol::table t = lua.create_table();
        t["thumbsUp"]   = r.thumbsUp;
        t["thumbsDown"] = r.thumbsDown;
        t["rating"]     = r.rating;
        return t;
    };
    ratingsNs["injectForTests"] = [](int issue, int up, int down) {
        GithubReactionsFetcher::Result r;
        r.thumbsUp   = up;
        r.thumbsDown = down;
        int tot = up + down;
        r.rating = tot == 0 ? 0.0f : (float(up) / tot) * 5.0f;
        r.valid  = true;
        GithubReactionsFetcher::instance().injectForTests(issue, r);
    };
    ratingsNs["setRepo"] = [](const std::string& repo) {
        GithubReactionsFetcher::instance().setRepo(repo);
    };
}

} // namespace bbfx

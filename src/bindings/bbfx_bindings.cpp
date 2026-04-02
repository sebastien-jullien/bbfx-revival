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
        "getBackgroundColour", &Ogre::Viewport::getBackgroundColour
    );

    lua.new_usertype<Ogre::RenderWindow>("Ogre_RenderWindow",
        sol::no_constructor,
        "getViewport", &Ogre::RenderWindow::getViewport,
        "getNumViewports", &Ogre::RenderWindow::getNumViewports,
        "getWidth", &Ogre::RenderWindow::getWidth,
        "getHeight", &Ogre::RenderWindow::getHeight
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
}

} // namespace bbfx

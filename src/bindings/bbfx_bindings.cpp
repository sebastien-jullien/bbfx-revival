#include "bbfx_bindings.h"
#include "../core/Engine.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../input/InputManager.h"

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
        "getRenderWindow", &Engine::getRenderWindow
    );
    bbfx["Engine"] = lua["bbfx_Engine"];

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
        "removeNode", [](Animator& self, AnimationNode* node) {
            for (auto& [name, port] : node->getInputs()) {
                self.remove(port);
            }
            for (auto& [name, port] : node->getOutputs()) {
                self.remove(port);
            }
        },
        "renderOneFrame", &Animator::renderOneFrame,
        "setPreOp", [](Animator& self, bool isLink, AnimationPort* from, AnimationPort* to, float time) {
            self.schedule(Operation(isLink, from, to), time);
        },
        "setPostOp", [](Animator& /*self*/) {
            // Post-ops are handled internally by the Animator
        }
    );
    bbfx["Animator"] = lua["bbfx_Animator"];

    // ── I-030: Animation node bindings ──────────────────────────────────
    lua.new_usertype<RootTimeNode>("bbfx_RootTimeNode",
        sol::call_constructor, sol::constructors<RootTimeNode()>(),
        "instance", []() -> RootTimeNode* { return RootTimeNode::instance(); },
        "reset", &RootTimeNode::reset,
        "getTotalTime", &RootTimeNode::getTotalTime,
        "update", &RootTimeNode::update
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

    // AnimationNode base (for receiving in Animator::addNode)
    lua.new_usertype<AnimationNode>("bbfx_AnimationNode",
        sol::no_constructor,
        "getName", &AnimationNode::getName,
        "update", &AnimationNode::update
    );
    bbfx["AnimationNode"] = lua["bbfx_AnimationNode"];

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
}

} // namespace bbfx

#include "Debugger.h"
#include "StudioApp.h"
#include "NodeTypeRegistry.h"
#include "commands/CommandManager.h"
#include "commands/NodeCommands.h"
#include "commands/LinkCommands.h"
#include "../core/Animator.h"
#include "../core/AnimationNode.h"
#include "../core/AnimationPort.h"
#include "../core/PrimitiveNodes.h"
#include "../core/Engine.h"
#include "nodes/SceneObjectNode.h"
#include "panels/ViewportPanel.h"

#include <iostream>
#include <filesystem>
#include <unordered_map>

namespace bbfx {

static StudioApp* sApp = nullptr;

// Preset group tracking: maps any node name to the list of all node names in its preset group
static std::unordered_map<std::string, std::vector<std::string>> sPresetGroups;

void Debugger::install(sol::state& lua, StudioApp* app) {
    sApp = app;
    sol::table dbg = lua.create_named_table("dbg");

    // ── Deferred operations queue ──────────────────────────────────────
    // Node creation via TCP crashes due to Lua re-entrancy (the TCP shell
    // callback is inside a Lua call, and the factory also calls lua.safe_script).
    // Solution: queue the operation and process it at the start of the next frame.
    struct PendingOp { std::string action, arg1, arg2; };
    static std::vector<PendingOp> sPending;

    lua.set_function("_dbg_process_pending", [&lua]() {
        if (sPending.empty()) return;
        auto ops = std::move(sPending);
        sPending.clear();
        for (auto& op : ops) {
            if (op.action == "create") {
                CommandManager::instance().execute(
                    std::make_unique<CreateNodeCommand>(op.arg1, op.arg2, lua));
                auto* node = Animator::instance()
                    ? Animator::instance()->getRegisteredNode(op.arg2) : nullptr;
                std::cout << "[dbg] Created " << op.arg1 << " '" << op.arg2 << "': "
                          << (node ? "OK" : "FAILED") << std::endl;
            } else if (op.action == "preset") {
                std::string path = "lua/presets/" + op.arg1 + ".lua";
                auto result = lua.safe_script_file(path, sol::script_pass_on_error);
                std::string nodeType = "LuaAnimationNode";
                sol::table built;
                bool hasBuilt = false;
                sol::table presetDefaults;
                bool hasDefaults = false;
                if (result.valid()) {
                    sol::table preset = result;
                    sol::optional<sol::function> buildFn = preset["build"];
                    if (buildFn) {
                        // Read ParamSpec defaults from preset["params"]["_values"]
                        sol::optional<sol::table> pspec = preset["params"];
                        if (pspec) {
                            sol::optional<sol::table> vals = (*pspec)["_values"];
                            if (vals) {
                                presetDefaults = *vals;
                                hasDefaults = true;
                            }
                        }
                        // Pass defaults to build() so it returns them as params
                        sol::table params = hasDefaults ? presetDefaults : lua.create_table();
                        auto br = (*buildFn)(params);
                        if (br.valid() && br.get_type() == sol::type::table) {
                            built = br;
                            hasBuilt = true;
                            sol::optional<std::string> t = built["type"];
                            if (t) nodeType = *t;
                        }
                    }
                }

                // Helper lambda: apply params from a sol::table to a node's ports and ParamSpec
                auto applyParams = [](AnimationNode* n, sol::table& paramsTbl) {
                    if (!n) return;
                    auto& inputs = n->getInputs();
                    auto* spec = n->getParamSpec();
                    for (auto& [key, val] : paramsTbl) {
                        if (!key.is<std::string>()) continue;
                        std::string pname = key.as<std::string>();
                        if (val.is<double>() || val.is<float>() || val.is<int>()) {
                            float fv = val.as<float>();
                            auto it = inputs.find(pname);
                            if (it != inputs.end()) it->second->setValue(fv);
                            if (spec) {
                                auto* pd = spec->getParam(pname);
                                if (pd) {
                                    if (pd->type == ParamType::FLOAT) pd->floatVal = fv;
                                    else if (pd->type == ParamType::INT) pd->intVal = static_cast<int>(fv);
                                }
                            }
                        } else if (val.is<bool>()) {
                            bool bv = val.as<bool>();
                            auto it = inputs.find(pname);
                            if (it != inputs.end()) it->second->setValue(bv ? 1.0f : 0.0f);
                            if (spec) {
                                auto* pd = spec->getParam(pname);
                                if (pd && pd->type == ParamType::BOOL) pd->boolVal = bv;
                            }
                        } else if (val.is<std::string>()) {
                            std::string sv = val.as<std::string>();
                            if (spec) {
                                auto* pd = spec->getParam(pname);
                                if (pd) pd->stringVal = sv;
                            }
                        }
                    }
                };

                // Check for Format B (multi-node): built["nodes"] is a table
                sol::optional<sol::table> multiNodes = hasBuilt ? built.get<sol::optional<sol::table>>("nodes") : sol::nullopt;
                if (multiNodes) {
                    // === Format B: multi-node preset (undoable via CompoundCommand) ===
                    auto compound = std::make_unique<CompoundCommand>("Preset '" + op.arg1 + "'");
                    std::vector<std::string> groupNames;
                    std::string presetPrefix = op.arg1;

                    // Collect node specs for param application after creation
                    struct NodeParamInfo { std::string name; std::string paramName; std::string sVal; float fVal; bool bVal; enum { S, F, B } kind; };
                    std::vector<NodeParamInfo> deferredParams;

                    for (auto& kv : *multiNodes) {
                        if (!kv.second.is<sol::table>()) continue;
                        sol::table nspec = kv.second;
                        sol::optional<std::string> nname = nspec["name"];
                        sol::optional<std::string> ntype = nspec["type"];
                        if (!nname || !ntype) continue;
                        std::string fullName = presetPrefix + "_" + *nname;
                        groupNames.push_back(fullName);
                        compound->add(std::make_unique<CreateNodeCommand>(*ntype, fullName, lua));
                        // Collect per-node params for deferred application
                        sol::optional<sol::table> nodeParams = nspec["params"];
                        if (nodeParams) {
                            for (auto& pk : *nodeParams) {
                                if (!pk.first.is<std::string>()) continue;
                                NodeParamInfo pi;
                                pi.name = fullName;
                                pi.paramName = pk.first.as<std::string>();
                                if (pk.second.is<std::string>()) { pi.sVal = pk.second.as<std::string>(); pi.kind = NodeParamInfo::S; }
                                else if (pk.second.is<double>()) { pi.fVal = static_cast<float>(pk.second.as<double>()); pi.kind = NodeParamInfo::F; }
                                else if (pk.second.is<bool>()) { pi.bVal = pk.second.as<bool>(); pi.kind = NodeParamInfo::B; }
                                else continue;
                                deferredParams.push_back(pi);
                            }
                        }
                    }

                    // Add a lambda command to apply per-node params after all nodes are created
                    if (!deferredParams.empty()) {
                        auto params = std::make_shared<std::vector<NodeParamInfo>>(std::move(deferredParams));
                        compound->add(std::make_unique<LambdaCommand>("Apply preset params",
                            [params]() {
                                auto* animator = Animator::instance();
                                if (!animator) return;
                                for (auto& pi : *params) {
                                    auto* n = animator->getRegisteredNode(pi.name);
                                    if (!n || !n->getParamSpec()) continue;
                                    auto* pd = n->getParamSpec()->getParam(pi.paramName);
                                    if (!pd) continue;
                                    if (pi.kind == NodeParamInfo::S) pd->stringVal = pi.sVal;
                                    else if (pi.kind == NodeParamInfo::F) pd->floatVal = pi.fVal;
                                    else if (pi.kind == NodeParamInfo::B) pd->boolVal = pi.bVal;
                                }
                            },
                            [params]() {
                                // Undo: clear applied params (nodes will be deleted by CreateNodeCommand::undo anyway)
                            }
                        ));
                    }

                    // Add link commands
                    sol::optional<sol::table> multiLinks = built.get<sol::optional<sol::table>>("links");
                    if (multiLinks) {
                        for (auto& kv : *multiLinks) {
                            if (!kv.second.is<sol::table>()) continue;
                            sol::table lk = kv.second;
                            sol::optional<std::string> from = lk["from"];
                            sol::optional<std::string> fromPort = lk["fromPort"];
                            sol::optional<std::string> to = lk["to"];
                            sol::optional<std::string> toPort = lk["toPort"];
                            if (!from || !fromPort || !to || !toPort) continue;
                            std::string fn = presetPrefix + "_" + *from;
                            std::string tn = presetPrefix + "_" + *to;
                            compound->add(std::make_unique<CreateLinkCommand>(fn, *fromPort, tn, *toPort));
                        }
                    }

                    // Execute the whole compound as one undoable operation
                    CommandManager::instance().execute(std::move(compound));

                    // Register preset group for cascade deletion
                    if (!groupNames.empty()) {
                        for (auto& gn : groupNames) {
                            sPresetGroups[gn] = groupNames;
                        }
                    }
                    sol::optional<std::string> primary = built["primary"];
                    std::string primaryName = primary ? (presetPrefix + "_" + *primary) : (groupNames.empty() ? "" : groupNames[0]);
                    std::cout << "[dbg] Preset '" << op.arg1 << "' multi-node (" << groupNames.size()
                              << " nodes, primary=" << primaryName << ")" << std::endl;
                } else {
                    // === Format A: single-node preset ===
                    // Store preset defaults in Lua global so factories can read them at creation time
                    // (e.g., ShaderFxNode needs to know which shader to load before node exists)
                    if (hasDefaults) {
                        lua["_preset_params"] = presetDefaults;
                    } else {
                        lua["_preset_params"] = sol::nil;
                    }
                    CommandManager::instance().execute(
                        std::make_unique<CreateNodeCommand>(nodeType, op.arg1, lua));
                    auto* node = Animator::instance()
                        ? Animator::instance()->getRegisteredNode(op.arg1) : nullptr;
                    lua["_preset_params"] = sol::nil; // clean up
                    if (node) {
                        // Apply preset defaults to node ports and ParamSpec
                        if (hasDefaults) {
                            applyParams(node, presetDefaults);
                        }
                        // Also apply anything from built["params"] (may overlap, that's OK)
                        if (hasBuilt) {
                            sol::optional<sol::table> bparams = built.get<sol::optional<sol::table>>("params");
                            if (bparams) {
                                sol::table p = *bparams;
                                applyParams(node, p);
                            }
                        }
                    }
                    std::cout << "[dbg] Preset '" << op.arg1 << "' -> " << nodeType
                              << ": " << (node ? "OK" : "FAILED") << std::endl;
                }
            } else if (op.action == "delete") {
                auto* animator = Animator::instance();
                if (animator) {
                    // Check for preset group cascade deletion
                    auto git = sPresetGroups.find(op.arg1);
                    if (git != sPresetGroups.end()) {
                        auto group = git->second;
                        for (auto& gn : group) {
                            CommandManager::instance().execute(
                                std::make_unique<DeleteNodeCommand>(gn, lua));
                            sPresetGroups.erase(gn);
                        }
                        std::cout << "[dbg] Deleted preset group (" << group.size() << " nodes)" << std::endl;
                    } else {
                        CommandManager::instance().execute(
                            std::make_unique<DeleteNodeCommand>(op.arg1, lua));
                        std::cout << "[dbg] Deleted '" << op.arg1 << "'" << std::endl;
                    }
                }
            }
        }
    });

    // ── Node creation (deferred) ───────────────────────────────────────
    dbg["create"] = [](const std::string& typeName, const std::string& nodeName) -> bool {
        sPending.push_back({"create", typeName, nodeName});
        std::cout << "[dbg] Queued: create " << typeName << " '" << nodeName << "'" << std::endl;
        return true;
    };

    // ── Node deletion (deferred — same as create) ──
    dbg["delete"] = [](const std::string& nodeName) -> bool {
        sPending.push_back({"delete", nodeName, ""});
        std::cout << "[dbg] Queued: delete '" << nodeName << "'" << std::endl;
        return true;
    };

    // ── Node deletion via UI path (CommandManager — same as Inspector Delete button) ──
    dbg["ui_delete"] = [&lua](const std::string& nodeName) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) {
            std::cerr << "[dbg] ui_delete: Node '" << nodeName << "' not found" << std::endl;
            return false;
        }
        std::cout << "[dbg] ui_delete: Executing DeleteNodeCommand for '" << nodeName << "'..." << std::endl;
        CommandManager::instance().execute(
            std::make_unique<DeleteNodeCommand>(nodeName, lua));
        std::cout << "[dbg] ui_delete: Done." << std::endl;
        return true;
    };

    // ── Undo / Redo ──────────────────────────────────────────────────
    dbg["undo"] = []() {
        CommandManager::instance().undo();
        std::cout << "[dbg] undo" << std::endl;
    };
    dbg["redo"] = []() {
        CommandManager::instance().redo();
        std::cout << "[dbg] redo" << std::endl;
    };

    // ── Enable / Disable ──────────────────────────────────────────────
    dbg["set_enabled"] = [](const std::string& nodeName, bool en) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* node = animator->getRegisteredNode(nodeName);
        if (node) {
            node->setEnabled(en);
            std::cout << "[dbg] " << nodeName << " enabled=" << (en ? "true" : "false") << std::endl;
        }
    };
    dbg["is_enabled"] = [](const std::string& nodeName) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* node = animator->getRegisteredNode(nodeName);
        return node ? node->isEnabled() : false;
    };

    // ── Preset instantiation (deferred) ──────────────────────────────
    dbg["preset"] = [](const std::string& presetName) -> bool {
        // Deferred: loads preset file and creates node at start of next frame
        sPending.push_back({"preset", presetName, ""});
        std::cout << "[dbg] Queued: preset '" << presetName << "'" << std::endl;
        return false;
    };

    // ── Link creation ──────────────────────────────────────────────────
    dbg["link"] = [](const std::string& fromNode, const std::string& fromPort,
                     const std::string& toNode, const std::string& toPort) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* fn = animator->getRegisteredNode(fromNode);
        auto* tn = animator->getRegisteredNode(toNode);
        if (!fn || !tn) {
            std::cerr << "[dbg] Link failed: node not found" << std::endl;
            return false;
        }
        auto& outs = fn->getOutputs();
        auto& ins = tn->getInputs();
        auto oit = outs.find(fromPort);
        auto iit = ins.find(toPort);
        if (oit == outs.end() || iit == ins.end()) {
            std::cerr << "[dbg] Link failed: port not found" << std::endl;
            return false;
        }
        animator->link(oit->second, iit->second);
        // Auto-fill target_entity ParamSpec when entity→entity link is created
        if (fromPort == "entity" && toPort == "entity") {
            if (tn->getParamSpec()) {
                auto* td = tn->getParamSpec()->getParam("target_entity");
                if (td) td->stringVal = fromNode;
            }
        }
        std::cout << "[dbg] Linked " << fromNode << "." << fromPort
                  << " -> " << toNode << "." << toPort << std::endl;
        return true;
    };

    // ── Link removal ───────────────────────────────────────────────────
    dbg["unlink"] = [](const std::string& fromNode, const std::string& fromPort,
                       const std::string& toNode, const std::string& toPort) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* fn = animator->getRegisteredNode(fromNode);
        auto* tn = animator->getRegisteredNode(toNode);
        if (!fn || !tn) return false;
        auto& outs = fn->getOutputs();
        auto& ins = tn->getInputs();
        auto oit = outs.find(fromPort);
        auto iit = ins.find(toPort);
        if (oit == outs.end() || iit == ins.end()) return false;
        animator->unlink(oit->second, iit->second);
        // Clear target_entity ParamSpec when entity→entity link is removed
        if (fromPort == "entity" && toPort == "entity") {
            if (tn->getParamSpec()) {
                auto* td = tn->getParamSpec()->getParam("target_entity");
                if (td) td->stringVal.clear();
            }
            tn->onLinkChanged();
        }
        std::cout << "[dbg] Unlinked " << fromNode << "." << fromPort
                  << " -> " << toNode << "." << toPort << std::endl;
        return true;
    };

    // ── Set port value ─────────────────────────────────────────────────
    dbg["set"] = [](const std::string& nodeName, const std::string& portName, float value) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) return false;
        auto& ins = node->getInputs();
        auto it = ins.find(portName);
        if (it == ins.end()) return false;
        it->second->setValue(value);
        std::cout << "[dbg] " << nodeName << "." << portName << " = " << value << std::endl;
        return true;
    };

    // ── Get port value ─────────────────────────────────────────────────
    dbg["get"] = [](const std::string& nodeName, const std::string& portName) -> float {
        auto* animator = Animator::instance();
        if (!animator) return 0.0f;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) return 0.0f;
        // Check inputs first, then outputs
        auto& ins = node->getInputs();
        auto it = ins.find(portName);
        if (it != ins.end()) return it->second->getValue();
        auto& outs = node->getOutputs();
        auto oit = outs.find(portName);
        if (oit != outs.end()) return oit->second->getValue();
        return 0.0f;
    };

    // ── List all nodes ─────────────────────────────────────────────────
    dbg["list"] = []() -> sol::as_table_t<std::vector<std::string>> {
        auto* animator = Animator::instance();
        std::vector<std::string> result;
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                std::string info = name;
                if (node) info += " (" + node->getTypeName() + ")";
                result.push_back(info);
                std::cout << "  " << info << std::endl;
            }
        }
        std::cout << "[dbg] " << result.size() << " nodes" << std::endl;
        return sol::as_table(result);
    };

    // ── List all links ─────────────────────────────────────────────────
    dbg["links"] = []() -> sol::as_table_t<std::vector<std::string>> {
        auto* animator = Animator::instance();
        std::vector<std::string> result;
        if (animator) {
            for (auto& lk : animator->getLinks()) {
                std::string info = lk.fromNode + "." + lk.fromPort +
                                   " -> " + lk.toNode + "." + lk.toPort;
                result.push_back(info);
                std::cout << "  " << info << std::endl;
            }
        }
        std::cout << "[dbg] " << result.size() << " links" << std::endl;
        return sol::as_table(result);
    };

    // ── Inspect a node ─────────────────────────────────────────────────
    dbg["inspect"] = [](const std::string& nodeName) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) {
            std::cout << "[dbg] Node '" << nodeName << "' not found" << std::endl;
            return false;
        }
        std::cout << "--- " << nodeName << " (" << node->getTypeName() << ") ---" << std::endl;
        std::cout << "Inputs:" << std::endl;
        for (auto& [name, port] : node->getInputs())
            std::cout << "  " << name << " = " << port->getValue() << std::endl;
        std::cout << "Outputs:" << std::endl;
        for (auto& [name, port] : node->getOutputs())
            std::cout << "  " << name << " = " << port->getValue() << std::endl;
        if (node->getParamSpec() && !node->getParamSpec()->empty()) {
            std::cout << "ParamSpec:" << std::endl;
            for (auto& p : node->getParamSpec()->getParams())
                std::cout << "  " << p.name << " (" << p.displayLabel() << ")" << std::endl;
        }
        return true;
    };

    // ── Select a node (updates Inspector) ──────────────────────────────
    dbg["select"] = [app](const std::string& nodeName) {
        // Trigger the selection callback (same as clicking a node in the editor)
        if (app && app->getNodeEditorPanel()) {
            app->getNodeEditorPanel()->selectNode(nodeName);
        }
        std::cout << "[dbg] Selected '" << nodeName << "'" << std::endl;
    };

    // ── List available node types ──────────────────────────────────────
    dbg["types"] = []() -> sol::as_table_t<std::vector<std::string>> {
        std::vector<std::string> result;
        auto categories = NodeTypeRegistry::instance().getByCategory();
        for (auto& [cat, types] : categories) {
            for (auto* info : types) {
                result.push_back(info->typeName);
                std::cout << "  " << cat << " / " << info->typeName << std::endl;
            }
        }
        std::cout << "[dbg] " << result.size() << " types" << std::endl;
        return sol::as_table(result);
    };

    // ── List presets ───────────────────────────────────────────────────
    dbg["presets"] = []() -> sol::as_table_t<std::vector<std::string>> {
        std::vector<std::string> result;
        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator("lua/presets", ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua")
                result.push_back(entry.path().stem().string());
        }
        std::sort(result.begin(), result.end());
        for (auto& p : result) std::cout << "  " << p << std::endl;
        std::cout << "[dbg] " << result.size() << " presets" << std::endl;
        return sol::as_table(result);
    };

    // ── Screenshot ─────────────────────────────────────────────────────
    dbg["screenshot"] = [](const std::string& filename) -> bool {
        if (!sApp || !sApp->getEngine()) return false;
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
        bool ok = sApp->getEngine()->captureFrame(filename);
        std::cout << "[dbg] Screenshot " << (ok ? "saved" : "FAILED") << ": " << filename << std::endl;
        return ok;
    };

    // ── FPS ────────────────────────────────────────────────────────────
    dbg["fps"] = []() -> float {
        // ImGui tracks FPS
        float fps = ImGui::GetIO().Framerate;
        std::cout << "[dbg] " << fps << " FPS" << std::endl;
        return fps;
    };

    // ── Clear DAG (new project) ────────────────────────────────────────
    dbg["clear"] = []() {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto names = animator->getRegisteredNodeNames();
        for (auto& n : names) {
            if (n == "time") continue; // keep RootTimeNode
            if (n.rfind("shell/", 0) == 0) continue; // keep TCP shell node
            auto* node = animator->getRegisteredNode(n);
            if (node) {
                try { node->cleanup(); } catch (...) {}
                try { animator->removeNode(node); } catch (...) {}
            }
        }
        sPresetGroups.clear();
        std::cout << "[dbg] DAG cleared" << std::endl;
    };

    // ── Save/Load project ──────────────────────────────────────────────
    dbg["save"] = [](const std::string& path) {
        std::cout << "[dbg] Save to " << path << " (use File > Save)" << std::endl;
    };

    // ── Lock-on orbit test ──────────────────────────────────────────────
    // Full 360° orbit around the ogre: 8 screenshots every 45°.
    // Uses direct camera positioning (same math as applyOrbit) to verify.
    dbg["lockon_test"] = [app]() {
        if (!app) return;
        auto* sm = app->getEngine()->getSceneManager();
        auto* cam = sm ? sm->getCamera("MainCamera") : nullptr;
        if (!sm || !cam) {
            std::cout << "[dbg] lockon_test: no camera" << std::endl;
            return;
        }
        auto* camNode = cam->getParentSceneNode();
        if (!camNode) return;

        // Ensure render target is big enough to see the ogre
        app->getEngine()->resizeRenderTexture(800, 600);

        // Find target
        Ogre::Vector3 center = Ogre::Vector3::ZERO;
        auto* animator = Animator::instance();
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* dagNode = animator->getRegisteredNode(name);
                auto* soNode = dynamic_cast<SceneObjectNode*>(dagNode);
                if (soNode && soNode->getSceneNode()) {
                    center = soNode->getSceneNode()->_getDerivedPosition();
                    std::cout << "[dbg] lockon_test: target '" << name << "' at ("
                              << center.x << "," << center.y << "," << center.z << ")" << std::endl;
                    break;
                }
            }
        }

        // Orbit params: distance 120, pitch 20° — ogre (scale 30) fills ~1/3 of frame
        float distance = 120.0f;
        float pitchDeg = 20.0f;
        float pitchRad = Ogre::Math::DegreesToRadians(pitchDeg);
        float cosP = std::cos(pitchRad);

        std::cout << "[dbg] lockon_test: 8 shots, dist=" << distance << " pitch=" << pitchDeg << std::endl;

        for (int i = 0; i < 8; i++) {
            float yawDeg = i * 45.0f;
            float yawRad = Ogre::Math::DegreesToRadians(yawDeg);

            Ogre::Vector3 offset(
                distance * std::sin(yawRad) * cosP,
                distance * std::sin(pitchRad),
                distance * std::cos(yawRad) * cosP
            );
            camNode->setPosition(center + offset);
            camNode->lookAt(center, Ogre::Node::TS_WORLD);

            app->getEngine()->updateRenderTarget();
            std::string filename = "lockon_orbit_" + std::to_string(i) + "_deg" + std::to_string((int)yawDeg) + ".png";
            app->getEngine()->captureFrame(filename);

            auto pos = camNode->getPosition();
            std::cout << "[dbg] shot " << i << " yaw=" << (int)yawDeg << " cam=("
                      << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
        }

        // Now test handleLockOn specifically: enter + orbit 90° + screenshot
        auto* vp = app->getViewportPanel();
        auto* camCtrl = vp ? vp->getCameraController() : nullptr;
        if (camCtrl) {
            // Reset camera position
            camNode->setPosition(center + Ogre::Vector3(0, distance * std::sin(pitchRad), distance * cosP));
            camNode->lookAt(center, Ogre::Node::TS_WORLD);

            // Enter lock-on
            camCtrl->handleLockOn(0.016f, 0, 0, true);
            app->getEngine()->updateRenderTarget();
            app->getEngine()->captureFrame("lockon_handleLockOn_enter.png");
            std::cout << "[dbg] handleLockOn enter: cam=(" << camNode->getPosition().x
                      << "," << camNode->getPosition().y << "," << camNode->getPosition().z << ")" << std::endl;

            // Orbit 90° right via handleLockOn (90° / 0.15 sensitivity = 600 px total, 60 frames × 10px)
            for (int f = 0; f < 60; f++)
                camCtrl->handleLockOn(0.016f, 10.0f, 0, false);
            app->getEngine()->updateRenderTarget();
            app->getEngine()->captureFrame("lockon_handleLockOn_90deg.png");
            std::cout << "[dbg] handleLockOn +90°: cam=(" << camNode->getPosition().x
                      << "," << camNode->getPosition().y << "," << camNode->getPosition().z << ")" << std::endl;

            // Orbit another 90° right
            for (int f = 0; f < 60; f++)
                camCtrl->handleLockOn(0.016f, 10.0f, 0, false);
            app->getEngine()->updateRenderTarget();
            app->getEngine()->captureFrame("lockon_handleLockOn_180deg.png");
            std::cout << "[dbg] handleLockOn +180°: cam=(" << camNode->getPosition().x
                      << "," << camNode->getPosition().y << "," << camNode->getPosition().z << ")" << std::endl;
        }

        std::cout << "[dbg] lockon_test: done — check screenshots" << std::endl;
    };

    // ── Automated test suite ───────────────────────────────────────────
    dbg["test"] = [&lua]() {
        std::cout << "\n=== BBFx Studio Debugger Test Suite ===" << std::endl;
        int pass = 0, fail = 0;

        auto check = [&](const std::string& name, bool ok) {
            if (ok) { pass++; std::cout << "  PASS: " << name << std::endl; }
            else    { fail++; std::cerr << "  FAIL: " << name << std::endl; }
        };

        // Clear
        lua.script("dbg.clear()");

        // Test: list types
        auto typeResult = lua.script("return #dbg.types()");
        int numTypes = typeResult.valid() ? typeResult.get<int>() : 0;
        check("Node types available", numTypes > 10);

        // Test: create LuaAnimationNode
        bool c1 = lua.script("return dbg.create('LuaAnimationNode', 'test_lua')").get<bool>();
        check("Create LuaAnimationNode", c1);

        // Test: create AccumulatorNode
        bool c2 = lua.script("return dbg.create('AccumulatorNode', 'test_acc')").get<bool>();
        check("Create AccumulatorNode", c2);

        // Test: create MathNode
        bool c3 = lua.script("return dbg.create('MathNode', 'test_math')").get<bool>();
        check("Create MathNode", c3);

        // Test: inspect
        bool i1 = lua.script("return dbg.inspect('test_lua')").get<bool>();
        check("Inspect test_lua", i1);

        // Test: set/get port
        lua.script("dbg.set('test_lua', 'in', 3.14)");
        float v = lua.script("return dbg.get('test_lua', 'in')").get<float>();
        check("Set/Get port value", std::abs(v - 3.14f) < 0.01f);

        // Test: link
        bool lk = lua.script("return dbg.link('test_lua', 'out', 'test_acc', 'delta')").get<bool>();
        check("Create link", lk);

        // Test: links list
        auto linksResult = lua.script("return #dbg.links()");
        int numLinks = linksResult.valid() ? linksResult.get<int>() : 0;
        check("Links count > 0", numLinks > 0);

        // Test: unlink
        bool ulk = lua.script("return dbg.unlink('test_lua', 'out', 'test_acc', 'delta')").get<bool>();
        check("Unlink", ulk);

        // Test: delete
        lua.script("dbg.delete('test_lua')");
        lua.script("dbg.delete('test_acc')");
        lua.script("dbg.delete('test_math')");
        bool gone = lua.script("return not dbg.inspect('test_lua')").get<bool>();
        check("Delete node (no crash)", gone);

        // Test: screenshot
        bool ss = lua.script("return dbg.screenshot('output/inspect/dbg_test.png')").get<bool>();
        check("Screenshot", ss);

        std::cout << "\n=== Results: " << pass << " PASS, " << fail << " FAIL ===" << std::endl;
        if (fail == 0) std::cout << "ALL TESTS PASSED" << std::endl;
    };

    // ── Help ───────────────────────────────────────────────────────────
    dbg["help"] = []() {
        std::cout << "--- BBFx Studio Debugger ---" << std::endl;
        std::cout << "  dbg.create(type, name)              Create node" << std::endl;
        std::cout << "  dbg.delete(name)                    Delete node" << std::endl;
        std::cout << "  dbg.preset(name)                    Instantiate preset" << std::endl;
        std::cout << "  dbg.link(from, fport, to, tport)    Create link" << std::endl;
        std::cout << "  dbg.unlink(from, fport, to, tport)  Remove link" << std::endl;
        std::cout << "  dbg.set(node, port, value)          Set port value" << std::endl;
        std::cout << "  dbg.get(node, port)                 Get port value" << std::endl;
        std::cout << "  dbg.list()                          List all nodes" << std::endl;
        std::cout << "  dbg.links()                         List all links" << std::endl;
        std::cout << "  dbg.inspect(name)                   Detail a node" << std::endl;
        std::cout << "  dbg.types()                         List node types" << std::endl;
        std::cout << "  dbg.presets()                       List presets" << std::endl;
        std::cout << "  dbg.screenshot(path)                Capture viewport" << std::endl;
        std::cout << "  dbg.clear()                         Clear DAG" << std::endl;
        std::cout << "  dbg.test()                          Run test suite" << std::endl;
        std::cout << "  dbg.fps()                           Show FPS" << std::endl;
        std::cout << "  dbg.help()                          This help" << std::endl;
    };

    std::cout << "[Debugger] Installed. Type dbg.help() for commands." << std::endl;
}

} // namespace bbfx

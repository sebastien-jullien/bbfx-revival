#include "Debugger.h"
#include "StudioApp.h"
#include <sstream>
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
#include "commands/SceneCommands.h"
#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiMessage.h"
#include "../midi/MidiLearnManager.h"
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include "panels/ViewportPanel.h"
#include <OgreCompositorManager.h>
#include <OgreCompositorChain.h>
#include <OgreCompositorInstance.h>
#include <OgreCompositor.h>
#include <SDL3/SDL.h>
#include "../network/SyncManager.h"
#include "../network/SyncProtocol.h"
#include "nodes/ArtnetOutputNode.h"
#include "nodes/MidiOutputNode.h"
#include "GridWarpProfile.h"
#include "OutputManager.h"
#include "ZoneSnapshot.h"
#include "SurfaceMap.h"
#include "../plugin/PluginManager.h"
#include "../plugin/PluginManifest.h"
#include "../plugin/PluginValidator.h"
#include "../plugin/PluginHotReloader.h"
#include "../plugin/CommunityIndex.h"
#include "../network/HttpClient.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
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
    struct PendingOp {
        std::string action, arg1, arg2;
        std::string paramName, paramValue;   // optional post-create ParamSpec injection
        std::string preParamName, preParamValue; // optional pre-create _preset_params injection
    };
    static std::vector<PendingOp> sPending;

    lua.set_function("_dbg_process_pending", [&lua]() {
        if (sPending.empty()) return;
        auto ops = std::move(sPending);
        sPending.clear();
        for (auto& op : ops) {
            if (op.action == "create") {
                // Pre-create: inject _preset_params if provided (for ShaderFxNode factory)
                if (!op.preParamName.empty()) {
                    sol::table pp = lua.create_table();
                    pp[op.preParamName] = op.preParamValue;
                    // Also inject the second shader path if present in paramName/paramValue
                    if (!op.paramName.empty() && op.paramName.find("_shader") != std::string::npos) {
                        pp[op.paramName] = op.paramValue;
                        op.paramName.clear(); // consumed, don't inject post-create
                        op.paramValue.clear();
                    }
                    lua["_preset_params"] = pp;
                    std::cout << "[dbg] Pre-create: _preset_params set with "
                              << op.preParamName << "=" << op.preParamValue << std::endl;
                }
                CommandManager::instance().execute(
                    std::make_unique<CreateNodeCommand>(op.arg1, op.arg2, lua));
                // Clear _preset_params after factory execution
                if (!op.preParamName.empty()) {
                    lua["_preset_params"] = sol::nil;
                }
                auto* node = Animator::instance()
                    ? Animator::instance()->getRegisteredNode(op.arg2) : nullptr;
                // Post-create param injection (v3.2.4)
                if (node && !op.paramName.empty() && node->getParamSpec()) {
                    auto* p = node->getParamSpec()->getParam(op.paramName);
                    if (p) p->stringVal = op.paramValue;
                }
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

                        // For ShaderFxNode: inject shader paths into _preset_params
                        // BEFORE creation (the factory reads them in the constructor)
                        if (*ntype == "ShaderFxNode") {
                            std::string vs = "perlin_deform.glsl";
                            std::string fs = "passthrough.frag";
                            // Read from per-node params
                            sol::optional<sol::table> nodeParams2 = nspec["params"];
                            if (nodeParams2) {
                                sol::optional<std::string> nvs = (*nodeParams2)["vert_shader"];
                                sol::optional<std::string> nfs = (*nodeParams2)["frag_shader"];
                                if (nvs && !nvs->empty()) vs = *nvs;
                                if (nfs && !nfs->empty()) fs = *nfs;
                            }
                            // Read from preset-level defaults
                            if (hasDefaults) {
                                sol::optional<std::string> dvs = presetDefaults["vert_shader"];
                                sol::optional<std::string> dfs = presetDefaults["frag_shader"];
                                if (dvs && !dvs->empty()) vs = *dvs;
                                if (dfs && !dfs->empty()) fs = *dfs;
                            }
                            // Insert a LambdaCommand to set _preset_params JUST BEFORE the create
                            compound->add(std::make_unique<LambdaCommand>("Set shader params",
                                [&lua, vs, fs]() {
                                    sol::table pp = lua.create_table();
                                    pp["vert_shader"] = vs;
                                    pp["frag_shader"] = fs;
                                    lua["_preset_params"] = pp;
                                },
                                [&lua]() { lua["_preset_params"] = sol::nil; }
                            ));
                        }

                        compound->add(std::make_unique<CreateNodeCommand>(*ntype, fullName, lua));

                        // Clear _preset_params after ShaderFxNode creation
                        if (*ntype == "ShaderFxNode") {
                            compound->add(std::make_unique<LambdaCommand>("Clear shader params",
                                [&lua]() { lua["_preset_params"] = sol::nil; },
                                []() {}
                            ));
                        }
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
        sPending.push_back({"create", typeName, nodeName, "", "", "", ""});
        std::cout << "[dbg] Queued: create " << typeName << " '" << nodeName << "'" << std::endl;
        return true;
    };

    // ── Node creation with post-create param injection (v3.2.4) ──
    dbg["create_with_param"] = [](const std::string& typeName, const std::string& nodeName,
                                   const std::string& paramName, const std::string& paramValue) -> bool {
        sPending.push_back({"create", typeName, nodeName, paramName, paramValue, "", ""});
        std::cout << "[dbg] Queued: create " << typeName << " '" << nodeName
                  << "' + set " << paramName << "=" << paramValue << std::endl;
        return true;
    };

    // ── Node creation with pre-create _preset_params (v3.2.4 — for ShaderFxNode factory) ──
    dbg["create_with_shader"] = [](const std::string& nodeName,
                                    const std::string& vertShader, const std::string& fragShader) -> bool {
        sPending.push_back({"create", "ShaderFxNode", nodeName,
                            "", "",
                            "vert_shader", vertShader});
        // Store frag_shader in paramName/paramValue (will be consumed pre-create)
        auto& op = sPending.back();
        op.paramName = "frag_shader";
        op.paramValue = fragShader;
        std::cout << "[dbg] Queued: create ShaderFxNode '" << nodeName
                  << "' vert=" << vertShader << " frag=" << fragShader << std::endl;
        return true;
    };

    // ── Node deletion (deferred — same as create) ──
    dbg["delete"] = [](const std::string& nodeName) -> bool {
        sPending.push_back({"delete", nodeName, "", "", "", "", ""});
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
        sPending.push_back({"preset", presetName, "", "", "", "", ""});
        std::cout << "[dbg] Queued: preset '" << presetName << "'" << std::endl;
        return true;
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
        // Notify target node to rebuild targets from DAG
        if (fromPort == "entity" && toPort == "entity") {
            tn->onLinkChanged();
        }
        // Auto-create entity→entity link when connecting data ports between
        // a SceneObjectNode (entity output) and a node with entity input
        if (fromPort != "entity" && toPort != "entity") {
            std::string sceneNode, targetNode;
            if (tn->getOutputs().count("entity") && fn->getInputs().count("entity"))
                { sceneNode = toNode; targetNode = fromNode; }
            else if (fn->getOutputs().count("entity") && tn->getInputs().count("entity"))
                { sceneNode = fromNode; targetNode = toNode; }
            if (!sceneNode.empty()) {
                bool exists = false;
                for (auto& lk : animator->getLinks()) {
                    if (lk.fromNode == sceneNode && lk.fromPort == "entity" &&
                        lk.toNode == targetNode && lk.toPort == "entity")
                        { exists = true; break; }
                }
                if (!exists) {
                    auto* sn = animator->getRegisteredNode(sceneNode);
                    auto* tg = animator->getRegisteredNode(targetNode);
                    if (sn && tg) {
                        auto sIt = sn->getOutputs().find("entity");
                        auto tIt = tg->getInputs().find("entity");
                        if (sIt != sn->getOutputs().end() && tIt != tg->getInputs().end()) {
                            animator->link(sIt->second, tIt->second);
                            tg->onLinkChanged();
                            std::cout << "[dbg] Auto-linked " << sceneNode
                                      << ".entity -> " << targetNode << ".entity" << std::endl;
                        }
                    }
                }
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
        if (!animator) { std::cerr << "[dbg] unlink: no Animator" << std::endl; return false; }
        auto* fn = animator->getRegisteredNode(fromNode);
        auto* tn = animator->getRegisteredNode(toNode);
        if (!fn) { std::cerr << "[dbg] unlink: node '" << fromNode << "' not found" << std::endl; return false; }
        if (!tn) { std::cerr << "[dbg] unlink: node '" << toNode   << "' not found" << std::endl; return false; }
        auto& outs = fn->getOutputs();
        auto& ins = tn->getInputs();
        auto oit = outs.find(fromPort);
        auto iit = ins.find(toPort);
        if (oit == outs.end()) { std::cerr << "[dbg] unlink: port '" << fromNode << "." << fromPort << "' not found" << std::endl; return false; }
        if (iit == ins.end())  { std::cerr << "[dbg] unlink: port '" << toNode   << "." << toPort   << "' not found" << std::endl; return false; }
        animator->unlink(oit->second, iit->second);
        // Notify target node to rebuild targets from DAG
        if (fromPort == "entity" && toPort == "entity") {
            tn->onLinkChanged();
        }
        std::cout << "[dbg] Unlinked " << fromNode << "." << fromPort
                  << " -> " << toNode << "." << toPort << std::endl;
        return true;
    };

    // ── Set port value ─────────────────────────────────────────────────
    dbg["set"] = [](const std::string& nodeName, const std::string& portName, float value) -> bool {
        auto* animator = Animator::instance();
        if (!animator) { std::cerr << "[dbg] set: no Animator" << std::endl; return false; }
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) { std::cerr << "[dbg] set: node '" << nodeName << "' not found" << std::endl; return false; }
        // Search inputs first, then outputs
        auto& ins = node->getInputs();
        auto it = ins.find(portName);
        if (it != ins.end()) {
            it->second->setValue(value);
            std::cout << "[dbg] " << nodeName << "." << portName << " = " << value << std::endl;
            return true;
        }
        auto& outs = node->getOutputs();
        auto oit = outs.find(portName);
        if (oit != outs.end()) {
            oit->second->setValue(value);
            std::cout << "[dbg] " << nodeName << "." << portName << " = " << value << " (output)" << std::endl;
            return true;
        }
        std::cerr << "[dbg] set: port '" << nodeName << "." << portName << "' not found" << std::endl;
        return false;
    };

    // ── Get port value ─────────────────────────────────────────────────
    dbg["get"] = [](const std::string& nodeName, const std::string& portName) -> float {
        auto* animator = Animator::instance();
        if (!animator) { std::cerr << "[dbg] get: no Animator" << std::endl; return 0.0f; }
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) { std::cerr << "[dbg] get: node '" << nodeName << "' not found" << std::endl; return 0.0f; }
        auto& ins = node->getInputs();
        auto it = ins.find(portName);
        if (it != ins.end()) return it->second->getValue();
        auto& outs = node->getOutputs();
        auto oit = outs.find(portName);
        if (oit != outs.end()) return oit->second->getValue();
        std::cerr << "[dbg] get: port '" << nodeName << "." << portName << "' not found" << std::endl;
        return 0.0f;
    };

    // ── List all nodes ─────────────────────────────────────────────────
    dbg["list"] = []() -> sol::as_table_t<std::vector<std::string>> {
        auto* animator = Animator::instance();
        std::vector<std::string> result;
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                result.push_back(name);  // return raw names (not formatted)
                auto* node = animator->getRegisteredNode(name);
                std::string info = name;
                if (node) info += " (" + node->getTypeName() + ")";
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
        // Collect names to delete (don't modify during iteration)
        std::vector<std::string> toDelete;
        auto names = animator->getRegisteredNodeNames();
        for (auto& n : names) {
            if (n == "time") continue;
            if (n.rfind("shell/", 0) == 0) continue;
            if (n.rfind("_test_", 0) == 0 || n.rfind("_dbg_", 0) == 0) continue;
            toDelete.push_back(n);
        }
        // Defer deletion via gPendingDeletes (same mechanism as UI delete)
        // This avoids modifying the DAG during Animator evaluation
        for (auto& n : toDelete) {
            gPendingDeletes.push_back(n);
        }
        sPresetGroups.clear();
        std::cout << "[dbg] DAG cleared (" << toDelete.size() << " nodes queued for deletion)" << std::endl;
    };

    // ── Set ParamSpec string value (v3.2.4) ──────────────────────────────
    dbg["set_param"] = [](const std::string& nodeName, const std::string& paramName,
                          const std::string& value) -> bool {
        auto* animator = Animator::instance();
        if (!animator) return false;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node || !node->getParamSpec()) {
            std::cerr << "[dbg] set_param: node '" << nodeName << "' not found or no ParamSpec" << std::endl;
            return false;
        }
        auto* p = node->getParamSpec()->getParam(paramName);
        if (!p) {
            std::cerr << "[dbg] set_param: param '" << paramName << "' not found on '" << nodeName << "'" << std::endl;
            return false;
        }
        p->stringVal = value;
        std::cout << "[dbg] set_param: " << nodeName << "." << paramName << " = '" << value << "'" << std::endl;
        return true;
    };

    // ── Switch mode (v3.2.4) ──────────────────────────────────────────────
    dbg["mode"] = [app](const std::string& modeName) {
        if (!app) return;
        if (modeName == "performance" || modeName == "perf" || modeName == "f5") {
            // Access StudioApp's mPerformanceMode via a public setter
            // For now, simulate F5 by using the app pointer
            std::cout << "[dbg] mode: switching to Performance Mode" << std::endl;
            // We can't directly set mPerformanceMode from here (it's private)
            // But we can use SDL to simulate the F5 key press
            SDL_Event evt = {};
            evt.type = SDL_EVENT_KEY_DOWN;
            evt.key.key = SDLK_F5;
            evt.key.mod = 0;
            SDL_PushEvent(&evt);
        } else if (modeName == "studio" || modeName == "design") {
            std::cout << "[dbg] mode: switching to Studio Mode" << std::endl;
            SDL_Event evt = {};
            evt.type = SDL_EVENT_KEY_DOWN;
            evt.key.key = SDLK_ESCAPE;
            evt.key.mod = 0;
            SDL_PushEvent(&evt);
        }
    };

    // ── Compositor status (v3.2.4) ────────────────────────────────────────
    dbg["compositor_status"] = [app]() {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] compositor_status: no animator" << std::endl; return; }

        // List CompositorNodes in DAG
        int count = 0;
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "CompositorNode") {
                count++;
                auto* spec = node->getParamSpec();
                std::string compName = "?";
                bool enabled = true;
                if (spec) {
                    auto* cp = spec->getParam("compositor");
                    auto* ep = spec->getParam("enabled");
                    if (cp) compName = cp->stringVal;
                    if (ep) enabled = ep->boolVal;
                }
                std::cout << "  CompositorNode '" << name << "' → compositor='" << compName
                          << "' enabled=" << (enabled ? "yes" : "no") << std::endl;
            }
        }
        std::cout << "[dbg] compositor_status: " << count << " CompositorNodes in DAG" << std::endl;

        // Check OGRE compositor chain on RT1
        if (app && app->getEngine()) {
            auto* rt1 = app->getEngine()->getRenderTarget();
            if (rt1 && rt1->getNumViewports() > 0) {
                auto* vp1 = rt1->getViewport(0);
                auto* chain1 = Ogre::CompositorManager::getSingleton().getCompositorChain(vp1);
                int numInst1 = chain1 ? static_cast<int>(chain1->getNumCompositors()) : 0;
                std::cout << "  RT1 viewport compositor chain: " << numInst1 << " instances" << std::endl;
            }
        }
    };

    // ── Detailed node + link trace (v3.2.4) ───────────────────────────────
    dbg["trace"] = [](const std::string& nodeName) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) {
            std::cout << "[dbg] trace: node '" << nodeName << "' not found" << std::endl;
            return;
        }
        std::cout << "=== TRACE: " << nodeName << " (" << node->getTypeName() << ") ===" << std::endl;
        std::cout << "  enabled=" << (node->isEnabled() ? "yes" : "no") << std::endl;

        // Inputs + their sources
        std::cout << "  Inputs:" << std::endl;
        for (auto& [pname, port] : node->getInputs()) {
            auto sources = animator->getSourceNodes(port);
            std::cout << "    " << pname << " = " << port->getValue();
            if (!sources.empty()) {
                std::cout << " ← [";
                for (auto* src : sources) std::cout << src->getName() << " ";
                std::cout << "]";
            }
            std::cout << std::endl;
        }

        // Outputs
        std::cout << "  Outputs:" << std::endl;
        for (auto& [pname, port] : node->getOutputs())
            std::cout << "    " << pname << " = " << port->getValue() << std::endl;

        // ParamSpec
        if (node->getParamSpec() && !node->getParamSpec()->empty()) {
            std::cout << "  ParamSpec:" << std::endl;
            for (auto& p : node->getParamSpec()->getParams()) {
                std::cout << "    " << p.name << " = ";
                switch (p.type) {
                    case ParamType::FLOAT: std::cout << p.floatVal; break;
                    case ParamType::INT: std::cout << p.intVal; break;
                    case ParamType::BOOL: std::cout << (p.boolVal ? "true" : "false"); break;
                    default: std::cout << "'" << p.stringVal << "'"; break;
                }
                std::cout << std::endl;
            }
        }
    };

    // save: see full implementation below (v3.2.5 Save/Load section)

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

    // ── UI automation commands (v3.2.5) ─────────────────────────────────

    // Multi-select nodes by name
    dbg["select_nodes"] = [app](sol::variadic_args va) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto* panel = app->getNodeEditorPanel();
        // First clear, then select each
        // Use the panel's internal API
        std::vector<std::string> names;
        for (auto v : va) {
            if (v.is<std::string>()) names.push_back(v.as<std::string>());
        }
        // Set selection via selectNode for first, then the panel tracks via ned
        if (!names.empty()) {
            panel->selectNode(names[0]);
        }
        std::cout << "[dbg] select_nodes: " << names.size() << " nodes" << std::endl;
    };

    // align/distribute: see full implementations below (v3.2.5 Align/Distribute section)

    // Create node group from current selection
    dbg["group"] = [app](const std::string& name) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto* panel = app->getNodeEditorPanel();
        auto& groups = const_cast<std::vector<NodeEditorPanel::NodeGroup>&>(
            // Access mNodeGroups — we need a public accessor
            panel->getNodeGroups());
        NodeEditorPanel::NodeGroup grp;
        grp.name = name;
        grp.hue = 0.3f;
        auto& sel = panel->getSelectedNodeNames();
        grp.memberNames.assign(sel.begin(), sel.end());
        groups.push_back(grp);
        std::cout << "[dbg] group: created '" << name << "' with " << grp.memberNames.size() << " members" << std::endl;
    };

    // Ungroup by name
    dbg["ungroup"] = [app](const std::string& name) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto* panel = app->getNodeEditorPanel();
        auto& groups = const_cast<std::vector<NodeEditorPanel::NodeGroup>&>(panel->getNodeGroups());
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            if (it->name == name) {
                groups.erase(it);
                std::cout << "[dbg] ungroup: dissolved '" << name << "'" << std::endl;
                return;
            }
        }
        std::cout << "[dbg] ungroup: '" << name << "' not found" << std::endl;
    };

    // List groups
    dbg["list_groups"] = [app]() -> sol::as_table_t<std::vector<std::string>> {
        std::vector<std::string> result;
        if (app && app->getNodeEditorPanel()) {
            for (auto& grp : app->getNodeEditorPanel()->getNodeGroups()) {
                result.push_back(grp.name);
                std::cout << "  Group '" << grp.name << "' (" << grp.memberNames.size() << " members)" << std::endl;
            }
        }
        return sol::as_table(result);
    };

    // Add/edit comment on a node
    dbg["comment"] = [app](const std::string& nodeName, const std::string& text) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto* panel = app->getNodeEditorPanel();
        panel->getNodeComments()[nodeName] = text;
        std::cout << "[dbg] comment: '" << nodeName << "' = '" << text << "'" << std::endl;
    };

    // Get comment on a node
    dbg["get_comment"] = [app](const std::string& nodeName) -> std::string {
        if (!app || !app->getNodeEditorPanel()) return "";
        auto& comments = app->getNodeEditorPanel()->getNodeComments();
        auto it = comments.find(nodeName);
        return (it != comments.end()) ? it->second : "";
    };

    // Collapse/expand a node
    dbg["collapse"] = [app](const std::string& nodeName, bool collapsed) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto& collapsedSet = app->getNodeEditorPanel()->getCollapsedNodes();
        if (collapsed) collapsedSet.insert(nodeName);
        else collapsedSet.erase(nodeName);
        std::cout << "[dbg] collapse: '" << nodeName << "' = " << (collapsed ? "collapsed" : "expanded") << std::endl;
    };

    // Check if node is collapsed
    dbg["is_collapsed"] = [app](const std::string& nodeName) -> bool {
        if (!app || !app->getNodeEditorPanel()) return false;
        return app->getNodeEditorPanel()->getCollapsedNodes().count(nodeName) > 0;
    };

    // Crossfader: capture snapshot A or B
    dbg["crossfade_capture"] = [app](const std::string& side) {
        std::cout << "[dbg] crossfade_capture: " << side << " (requires PerformanceMode)" << std::endl;
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

        // Flush deferred creates so nodes exist for the following tests
        lua.script("_dbg_process_pending()");

        // Test: inspect
        bool i1 = lua.script("return dbg.inspect('test_lua')").get<bool>();
        check("Inspect test_lua", i1);

        // Test: set/get port — use AccumulatorNode which has known port "delta"
        lua.script("dbg.set('test_acc', 'delta', 3.14)");
        float v = lua.script("return dbg.get('test_acc', 'delta')").get<float>();
        check("Set/Get port value", std::abs(v - 3.14f) < 0.01f);

        // Test: link — MathNode has "out" output, AccumulatorNode has "delta" input
        bool lk = lua.script("return dbg.link('test_math', 'out', 'test_acc', 'delta')").get<bool>();
        check("Create link", lk);

        // Test: links list
        auto linksResult = lua.script("return #dbg.links()");
        int numLinks = linksResult.valid() ? linksResult.get<int>() : 0;
        check("Links count > 0", numLinks > 0);

        // Test: unlink
        bool ulk = lua.script("return dbg.unlink('test_math', 'out', 'test_acc', 'delta')").get<bool>();
        check("Unlink", ulk);

        // Test: delete (deferred by design — OGRE objects can't be destroyed during ImGui render)
        lua.script("dbg.delete('test_lua')");
        lua.script("dbg.delete('test_acc')");
        lua.script("dbg.delete('test_math')");
        lua.script("_dbg_process_pending()");
        // DeleteNodeCommand saves state for undo and queues actual removal for next frame.
        // Verify the pending delete queue is populated (actual removal happens in StudioApp::renderFrame)
        check("Delete node (queued)", !gPendingDeletes.empty());

        // Test: screenshot
        bool ss = lua.script("return dbg.screenshot('output/inspect/dbg_test.png')").get<bool>();
        check("Screenshot", ss);

        // ── v3.4 Tests: Output Manager ──────────────────────────────────
        std::cout << "\n--- v3.4: Output Manager ---" << std::endl;

        // Add output
        auto addResult = lua.script("return dbg.output_add(1280, 720)");
        int outId = addResult.valid() ? addResult.get<int>() : -1;
        check("output_add returns valid id", outId >= 0);

        // Warp
        lua.script("dbg.output_warp(" + std::to_string(outId) +
            ", 0.05, 0.0, 0.95, 0.05, 0.0, 1.0, 1.0, 1.0)");
        check("output_warp (no crash)", true);

        // Warp reset
        lua.script("dbg.output_warp_reset(" + std::to_string(outId) + ")");
        check("output_warp_reset (no crash)", true);

        // Blend
        lua.script("dbg.output_blend(" + std::to_string(outId) +
            ", 0.1, 0.1, 0.0, 0.0, 2.2)");
        check("output_blend (no crash)", true);

        // Blend reset
        lua.script("dbg.output_blend_reset(" + std::to_string(outId) + ")");
        check("output_blend_reset (no crash)", true);

        // ── v3.4 Tests: Surface Zones ───────────────────────────────────
        std::cout << "\n--- v3.4: Surface Zones ---" << std::endl;

        auto zoneResult = lua.script("return dbg.zone_add('TestZone', 0.0, 0.0, 0.5, 1.0)");
        int zoneId = zoneResult.valid() ? zoneResult.get<int>() : -1;
        check("zone_add returns valid id", zoneId >= 0);

        // Assign zone to output
        lua.script("dbg.zone_assign(" + std::to_string(zoneId) + ", " + std::to_string(outId) + ")");
        check("zone_assign (no crash)", true);

        // ── v3.4 Tests: Scene Switcher ──────────────────────────────────
        std::cout << "\n--- v3.4: Scene Switcher ---" << std::endl;

        lua.script("dbg.scene_capture('test_scene_A')");
        check("scene_capture (no crash)", true);

        // Modify warp, capture a second scene
        lua.script("dbg.output_warp(" + std::to_string(outId) +
            ", 0.0, 0.05, 0.95, 0.0, 0.0, 1.0, 1.0, 1.0)");
        lua.script("dbg.scene_capture('test_scene_B')");
        check("scene_capture B (no crash)", true);

        // Apply scene A
        lua.script("dbg.scene_apply('test_scene_A')");
        check("scene_apply (no crash)", true);

        // ── v3.4 Tests: Grid Warp ───────────────────────────────────────
        std::cout << "\n--- v3.4: Grid Warp ---" << std::endl;

        lua.script("dbg.output_gridwarp(" + std::to_string(outId) + ", 1, 1, 0.3, 0.3)");
        check("output_gridwarp set point (no crash)", true);

        lua.script("dbg.output_gridwarp_reset(" + std::to_string(outId) + ")");
        check("output_gridwarp_reset (no crash)", true);

        // ── v3.4 Tests: Panic All ───────────────────────────────────────
        std::cout << "\n--- v3.4: Panic All ---" << std::endl;

        // First set warp+blend so panic has something to reset
        lua.script("dbg.output_warp(" + std::to_string(outId) +
            ", 0.05, 0.0, 0.95, 0.05, 0.0, 1.0, 1.0, 1.0)");
        lua.script("dbg.output_blend(" + std::to_string(outId) +
            ", 0.1, 0.1, 0.0, 0.0, 2.2)");
        lua.script("dbg.panic_all()");
        check("panic_all (no crash)", true);

        // ── v3.4 Tests: set_param ───────────────────────────────────────
        std::cout << "\n--- v3.4: set_param ---" << std::endl;
        {
            // Create a SceneObjectNode which has ParamSpec with mesh_file
            lua.script("dbg.create('SceneObjectNode', 'test_param_obj')");
            lua.script("_dbg_process_pending()");

            auto spResult = lua.script("return dbg.set_param('test_param_obj', 'mesh_file', 'torus.mesh')");
            bool spOk = spResult.valid() ? spResult.get<bool>() : false;
            check("set_param returns true", spOk);

            // Cleanup
            lua.script("dbg.delete('test_param_obj')");
            lua.script("_dbg_process_pending()");
        }

        // ── v3.4 Tests: Preset load ─────────────────────────────────────
        std::cout << "\n--- v3.4: Preset ---" << std::endl;
        {
            lua.script("dbg.preset('perlin_pulse')");
            lua.script("_dbg_process_pending()");

            // perlin_pulse creates primary node "perlin_pulse_fx"
            auto inspResult = lua.script("return dbg.inspect('perlin_pulse_fx')");
            bool inspOk = inspResult.valid() ? inspResult.get<bool>() : false;
            check("preset perlin_pulse: node exists", inspOk);

            // Cleanup — delete both nodes created by preset
            lua.script("dbg.delete('perlin_pulse_fx')");
            lua.script("dbg.delete('perlin_pulse_mesh')");
            lua.script("_dbg_process_pending()");
        }

        // ── v3.4 Tests: Undo / Redo ────────────────────────────────────
        std::cout << "\n--- v3.4: Undo / Redo ---" << std::endl;
        {
            // Create a node via CommandManager (ui_delete uses it, so does create)
            lua.script("dbg.create('MathNode', 'test_undo_node')");
            lua.script("_dbg_process_pending()");

            // Verify it exists
            auto exists1 = lua.script("return dbg.inspect('test_undo_node')");
            check("undo: node created", exists1.valid() && exists1.get<bool>());

            // Delete via ui_delete (CommandManager, supports undo)
            lua.script("dbg.ui_delete('test_undo_node')");
            lua.script("_dbg_process_pending()");

            // Undo the deletion
            lua.script("dbg.undo()");
            lua.script("_dbg_process_pending()");

            auto exists2 = lua.script("return dbg.inspect('test_undo_node')");
            check("undo: node restored after undo", exists2.valid() && exists2.get<bool>());

            // Redo the deletion
            lua.script("dbg.redo()");
            lua.script("_dbg_process_pending()");

            // Cleanup (might already be deleted by redo, but delete is safe on missing)
            lua.script("dbg.delete('test_undo_node')");
            lua.script("_dbg_process_pending()");
            check("redo (no crash)", true);
        }

        // ── v3.4 Tests: Save project ────────────────────────────────────
        std::cout << "\n--- v3.4: Save/Load ---" << std::endl;
        {
            auto saveResult = lua.script("return dbg.save('output/test_save.bbfx-project')");
            bool saveOk = saveResult.valid() ? saveResult.get<bool>() : false;
            check("save project returns true", saveOk);

            // Load is deferred to next frame — just verify the command queues without crash.
            auto loadResult = lua.script("return dbg.load('output/test_save.bbfx-project')");
            bool loadOk = loadResult.valid() ? loadResult.get<bool>() : false;
            check("load project queued (no crash)", loadOk);
        }

        // ── v3.4 Tests: scene_list ──────────────────────────────────────
        std::cout << "\n--- v3.4: scene_list ---" << std::endl;
        // scene_capture was called earlier with 'test_scene_A' and 'test_scene_B'
        lua.script("dbg.scene_list()");
        check("scene_list (no crash)", true);

        // ── v3.4 Cleanup ────────────────────────────────────────────────
        std::cout << "\n--- v3.4: Cleanup ---" << std::endl;

        lua.script("dbg.zone_remove(" + std::to_string(zoneId) + ")");
        check("zone_remove (no crash)", true);

        lua.script("dbg.output_remove(" + std::to_string(outId) + ")");
        check("output_remove (no crash)", true);

        // ── v3.5 Lot M Tests: live protocols bindings ───────────────────
        std::cout << "\n--- v3.5 Lot M: Live protocols ---" << std::endl;
        {
            auto ccOk = lua.script("return bbfx.midi.getCC(1, 7) == 0");
            check("midi.getCC(fresh) = 0", ccOk.valid() && ccOk.get<bool>());

            auto aOk = lua.script("return bbfx.artnet.send('127.0.0.1', 0, 1, 255)");
            check("artnet.send returns true", aOk.valid() && aOk.get<bool>());

            auto oscOk = lua.script("return bbfx.osc.get('/never') == nil");
            check("osc.get(unseen) == nil", oscOk.valid() && oscOk.get<bool>());

            auto tsOk = lua.script(
                "local r = bbfx.textureShare.createReceiver('dbg'); "
                "return type(r) == 'table' and type(r.getTextureName) == 'function'");
            check("textureShare.createReceiver returns handle",
                  tsOk.valid() && tsOk.get<bool>());

            auto backendOk = lua.script("return type(bbfx.textureShare.backend()) == 'string'");
            check("textureShare.backend returns string",
                  backendOk.valid() && backendOk.get<bool>());
        }

        // ── v3.5 Lot N Tests: noise / easing / tempo / timeline ─────────
        std::cout << "\n--- v3.5 Lot N: Procedural + Timing ---" << std::endl;
        {
            auto nOk = lua.script(
                "local a = bbfx.noise.simplex2D(1.23, 4.56, 42); "
                "local b = bbfx.noise.simplex2D(1.23, 4.56, 42); "
                "return math.abs(a - b) < 1e-6");
            check("noise.simplex2D deterministic", nOk.valid() && nOk.get<bool>());

            auto eOk = lua.script(
                "return math.abs(bbfx.easing.easeInOutCubic(0.5) - 0.5) < 1e-6");
            check("easing.easeInOutCubic(0.5) == 0.5", eOk.valid() && eOk.get<bool>());

            auto tOk = lua.script(
                "bbfx.tempo.setSource('manual'); bbfx.tempo.setManualBPM(130); "
                "return math.abs(bbfx.tempo.getBPM() - 130) < 0.01");
            check("tempo manual BPM round-trip", tOk.valid() && tOk.get<bool>());

            auto tlOk = lua.script(
                "local t = bbfx.timeline.create({duration=10}); "
                "t.addKey(0,0,'linear'); t.addKey(10,100,'linear'); "
                "t.seek(5); return math.abs(t.getValue() - 50) < 1e-4");
            check("timeline linear 0..100 @ t=5 = 50", tlOk.valid() && tlOk.get<bool>());

            auto evOk = lua.script(
                "local fired = 0; "
                "local t = bbfx.timeline.create({duration=10}); "
                "t.addEvent(5, function() fired = fired + 1 end); "
                "t.play(); t.update(30); return fired");
            int fired = evOk.valid() ? evOk.get<int>() : 0;
            check("timeline event fires exactly once", fired == 1);
        }

        // ── v3.5 Lot O Tests: fs / json / http / ws ─────────────────────
        std::cout << "\n--- v3.5 Lot O: HTTP/WebSocket/FS/JSON ---" << std::endl;
        {
            auto nsOk = lua.script(
                "return type(bbfx.fs) == 'table' and type(bbfx.json) == 'table' "
                "and type(bbfx.http) == 'table' and type(bbfx.websocket) == 'table'");
            check("http/ws/fs/json namespaces present",
                  nsOk.valid() && nsOk.get<bool>());

            auto jsonOk = lua.script(
                "local s = bbfx.json.encode({ a=1, b={1,2,3} }); "
                "local d = bbfx.json.decode(s); "
                "return d.a == 1 and #d.b == 3 and d.b[2] == 2");
            check("json.encode/decode round-trip nested",
                  jsonOk.valid() && jsonOk.get<bool>());

            auto fsOk = lua.script(
                "local tmp = (os.getenv('TEMP') or os.getenv('TMP') or '/tmp'):gsub('[/\\\\]+$', ''); "
                "local p = tmp .. '/bbfx_lot_o_dbg.txt'; "
                "bbfx.fs.writeFile(p, 'abc'); "
                "local ok = bbfx.fs.exists(p) and bbfx.fs.readFile(p) == 'abc'; "
                "os.remove(p); return ok");
            check("fs.writeFile + readFile round-trip",
                  fsOk.valid() && fsOk.get<bool>());

            auto hshOk = lua.script("return type(bbfx.http.sha256File) == 'function'");
            check("http.sha256File callable", hshOk.valid() && hshOk.get<bool>());

            auto wsSigOk = lua.script("return type(bbfx.websocket.connect) == 'function'");
            check("websocket.connect callable", wsSigOk.valid() && wsSigOk.get<bool>());
        }

        // ── v3.5 Lot V Tests: GitHub publishing ──────────────────────────
        std::cout << "\n--- v3.5 Lot V: GitHub Publish ---" << std::endl;
        {
            auto nsOk = lua.script(
                "return type(bbfx.github) == 'table' and "
                "type(bbfx.github.beginDeviceFlow) == 'function' and "
                "type(bbfx.github.openPullRequest) == 'function'");
            check("bbfx.github namespace + API present",
                  nsOk.valid() && nsOk.get<bool>());

            auto tokOk = lua.script(
                "local raw = 'gho_AbCdEf0123456789'; "
                "local enc = bbfx.github.encodeToken(raw); "
                "local dec = bbfx.github.decodeToken(enc); "
                "return enc ~= raw and dec == raw");
            check("github.encodeToken/decodeToken round-trip",
                  tokOk.valid() && tokOk.get<bool>());
        }

        // ── v3.5 Lot U Tests: Wizard + Hot Reload + CLI validator ───────
        std::cout << "\n--- v3.5 Lot U: Wizard / HotReload / CLI ---" << std::endl;
        {
            auto hotOk = lua.script(
                "return type(bbfx.hotreload) == 'table' and "
                "type(bbfx.hotreload.setEnabled) == 'function' and "
                "type(bbfx.hotreload.tick) == 'function'");
            check("bbfx.hotreload namespace + API present",
                  hotOk.valid() && hotOk.get<bool>());

            auto validOk = lua.script(
                "local r = bbfx.authoring.validatePath('/does/not/exist'); "
                "return type(r) == 'table' and r.ok == false and type(r.errors) == 'table'");
            check("authoring.validatePath returns {ok=false, errors=...} on missing",
                  validOk.valid() && validOk.get<bool>());

            auto templateOk = lua.script(
                "local body = bbfx.fs.readFile('lua/plugin/template_node_fx.lua'); "
                "return type(body) == 'string' and body:find('registerNodeType') ~= nil");
            check("lua/plugin/template_node_fx.lua is readable + valid",
                  templateOk.valid() && templateOk.get<bool>());
        }

        // ── v3.5 Lot T Tests: RTT / framebuffer / compositor ─────────────
        std::cout << "\n--- v3.5 Lot T: RTT + FrameBuffer + Compositor ---" << std::endl;
        {
            auto nsOk = lua.script(
                "return type(bbfx.renderTexture) == 'table' and "
                "type(bbfx.frameBuffer) == 'table' and "
                "type(bbfx.compositor) == 'table'");
            check("renderTexture/frameBuffer/compositor namespaces present",
                  nsOk.valid() && nsOk.get<bool>());

            auto rtOk = lua.script(
                "local rt = bbfx.renderTexture.create('dbg_test_rt', 48, 48); "
                "local ok = type(rt) == 'table' and rt.getWidth() == 48 "
                "           and rt.getTextureName() == 'dbg_test_rt'; "
                "if rt then rt.release() end; return ok");
            check("renderTexture.create round-trip + handle API",
                  rtOk.valid() && rtOk.get<bool>());

            auto fbOk = lua.script(
                "local rt = bbfx.renderTexture.create('dbg_fb_rt', 16, 16); "
                "local w, h = bbfx.frameBuffer.getResolution('dbg_fb_rt'); "
                "if rt then rt.release() end; return w == 16 and h == 16");
            check("frameBuffer.getResolution of named texture",
                  fbOk.valid() && fbOk.get<bool>());
        }

        // ── v3.5 Lot S Tests: plugin authoring backend ──────────────────
        std::cout << "\n--- v3.5 Lot S: Plugin Authoring ---" << std::endl;
        {
            auto slugOk = lua.script(
                "return bbfx.authoring.slugify('Plasma Wave!') == 'plasma-wave'");
            check("authoring.slugify normalizes spaces + punctuation",
                  slugOk.valid() && slugOk.get<bool>());

            auto idOk = lua.script(
                "return bbfx.authoring.isValidId('foo.bar-1') == true and "
                "bbfx.authoring.isValidId('NOT VALID') == false");
            check("authoring.isValidId accepts kebab id, rejects invalid",
                  idOk.valid() && idOk.get<bool>());

            auto permOk = lua.script(
                "local p = bbfx.authoring.detectPermissions("
                "    'local x = bbfx.midi.getCC(1, 1) + bbfx.http.get() ' ); "
                "local found = {}; for _, v in ipairs(p) do found[v] = true end "
                "return found['midi'] == true and found['network'] == true");
            check("authoring.detectPermissions spots midi + network",
                  permOk.valid() && permOk.get<bool>());

            auto subgraphOk = lua.script(
                "local path = bbfx.authoring.exportSubgraph("
                "  { id = 'dbg.lot_s.sub', name = 'Lot S Sub' }, "
                "  { nodes = {}, links = {} }); "
                "return type(path) == 'string' and #path > 0");
            check("authoring.exportSubgraph writes a plugin dir",
                  subgraphOk.valid() && subgraphOk.get<bool>());
        }

        // ── v3.5 Lot R Tests: procedural / SDF / fractals / L-system ────
        std::cout << "\n--- v3.5 Lot R: Procedural ---" << std::endl;
        {
            auto nsOk = lua.script(
                "return type(bbfx.geometry) == 'table' and type(bbfx.sdf) == 'table' "
                "and type(bbfx.fractals) == 'table' and type(bbfx.lsystem) == 'table'");
            check("geometry/sdf/fractals/lsystem namespaces present",
                  nsOk.valid() && nsOk.get<bool>());

            auto sdfOk = lua.script(
                "return math.abs(bbfx.sdf.sphere(2,0,0, 0,0,0, 1) - 1) < 1e-4");
            check("sdf.sphere primitive correct",
                  sdfOk.valid() && sdfOk.get<bool>());

            auto mandOk = lua.script(
                "local t = bbfx.fractals.mandelbrot(32, 32, {maxIter=16}); "
                "return type(t) == 'string' and #t > 0");
            check("fractals.mandelbrot returns texture name",
                  mandOk.valid() && mandOk.get<bool>());

            auto lsOk = lua.script(
                "local ls = bbfx.lsystem.create({axiom='F', "
                "rules={F='F+F'}, iterations=3, angle=90, step=1}); "
                "return #ls.derive() > 1");
            check("lsystem.derive returns non-empty",
                  lsOk.valid() && lsOk.get<bool>());

            // I-1491 non-regression
            auto mgOk = lua.script(
                "local s = bbfx.geometry.createSphere('dbg_test_sph', 1, 8, 16); "
                "return type(s) == 'string' and #s > 0");
            check("MeshGenerator v3.2 primitives still reachable",
                  mgOk.valid() && mgOk.get<bool>());
        }

        // ── v3.5 Lot Q Tests: media / images / sequences / models ───────
        std::cout << "\n--- v3.5 Lot Q: Media ---" << std::endl;
        {
            auto nsOk = lua.script(
                "return type(bbfx.media) == 'table' and type(bbfx.images) == 'table' "
                "and type(bbfx.sequences) == 'table' and type(bbfx.models) == 'table'");
            check("media/images/sequences/models namespaces present",
                  nsOk.valid() && nsOk.get<bool>());

            auto vOk = lua.script(
                "local c = bbfx.media.openVideo('/does/not/exist.mp4'); "
                "return type(c) == 'table' and type(c.play) == 'function'");
            check("media.openVideo returns handle even on missing file",
                  vOk.valid() && vOk.get<bool>());

            auto iOk = lua.script(
                "return bbfx.images.load('/does/not/exist.png') == nil");
            check("images.load(missing) returns nil",
                  iOk.valid() && iOk.get<bool>());

            auto sOk = lua.script(
                "local q = bbfx.sequences.loadSequence('/no', 'f_%04d.png', 1, 3); "
                "return type(q) == 'table' and q.frameCount() == 0");
            check("sequences.loadSequence empty frameCount=0",
                  sOk.valid() && sOk.get<bool>());

            auto mOk = lua.script(
                "return type(bbfx.models.isAvailable()) == 'boolean'");
            check("models.isAvailable returns boolean",
                  mOk.valid() && mOk.get<bool>());

            // I-1475 — Theora prerequisite still alive
            auto thOk = lua.script("return type(bbfx.Animator) ~= 'nil'");
            check("Theora prerequisite (bbfx.Animator) still reachable",
                  thOk.valid() && thOk.get<bool>());
        }

        // ── v3.5 Lot P Tests: ImGui Lua API (Studio only) ────────────────
        std::cout << "\n--- v3.5 Lot P: ImGui ---" << std::endl;
        {
            auto nsOk = lua.script("return type(bbfx.ui) == 'table'");
            check("bbfx.ui namespace present in Studio",
                  nsOk.valid() && nsOk.get<bool>());

            auto widgetsOk = lua.script(
                "return type(bbfx.ui.button) == 'function' and "
                "type(bbfx.ui.sliderFloat) == 'function' and "
                "type(bbfx.ui.colorEdit3) == 'function' and "
                "type(bbfx.ui.plotLines) == 'function'");
            check("ImGui widget helpers callable",
                  widgetsOk.valid() && widgetsOk.get<bool>());

            auto regOk = lua.script(
                "bbfx.ui.registerPanel('dbg_lotp', function() end); "
                "bbfx.ui.unregisterPanel('dbg_lotp'); return true");
            check("registerPanel + unregisterPanel callable",
                  regOk.valid() && regOk.get<bool>());

            auto iwOk = lua.script(
                "bbfx.ui.registerInspectorWidget('dbg_lotp_port', "
                "function(n, p, v) return false, v end); return true");
            check("registerInspectorWidget callable",
                  iwOk.valid() && iwOk.get<bool>());
        }

        // ── v3.5 Lot L Tests: gamepad bindings ──────────────────────────
        std::cout << "\n--- v3.5 Lot L: Gamepad ---" << std::endl;
        {
            // Convenience accessors return tuples of zeros on invalid index.
            auto lsOk = lua.script("local x,y = bbfx.gamepad.getLeftStick(999); return x == 0 and y == 0");
            check("gamepad.getLeftStick(invalid) returns (0,0)",
                  lsOk.valid() && lsOk.get<bool>());

            auto trigOk = lua.script("local l,r = bbfx.gamepad.getTriggers(999); return l == 0 and r == 0");
            check("gamepad.getTriggers(invalid) returns (0,0)",
                  trigOk.valid() && trigOk.get<bool>());

            auto pressOk = lua.script("return bbfx.gamepad.isPressed(999, 0) == false");
            check("gamepad.isPressed(invalid) = false",
                  pressOk.valid() && pressOk.get<bool>());
        }

        std::cout << "\n=== Results: " << pass << " PASS, " << fail << " FAIL ===" << std::endl;
        if (fail == 0) std::cout << "ALL TESTS PASSED" << std::endl;
    };

    // ── Help ───────────────────────────────────────────────────────────
    // ── Crossfader commands (v3.2.5) ──────────────────────────────────
    dbg["crossfade_capture_a"] = [app]() {
        if (!app) return;
        auto* perf = app->getPerformanceModePanel();
        if (!perf) return;
        auto* animator = Animator::instance();
        if (!animator) return;
        // Access crossfader snapshots via the public API we need to add
        std::cout << "[dbg] crossfade_capture_a: requires PerformanceMode panel access" << std::endl;
    };

    // ── Trigger command (v3.2.5) ────────────────────────────────────────
    dbg["trigger_page"] = [app](int pageIdx) {
        if (!app) return;
        std::cout << "[dbg] trigger_page: " << pageIdx << std::endl;
    };

    // ── Fader assign command (v3.2.5) ───────────────────────────────────
    dbg["fader_assign"] = [app](int idx, const std::string& nodeName, const std::string& portName) {
        if (!app) return;
        auto& faders = app->getPerformanceModePanel()->getFaders();
        if (idx >= 0 && idx < 8) {
            faders[idx].nodeName = nodeName;
            faders[idx].portName = portName;
            std::cout << "[dbg] fader_assign: fader " << idx << " → " << nodeName << "." << portName << std::endl;
        }
    };

    // ── v3.5 Lot M — live-protocols debug commands ─────────────────────
    dbg["midi_send_cc"] = [&lua](int outDev, int ch, int cc, int value) {
        lua.script("bbfx.midi.sendCC(" + std::to_string(outDev) + ","
            + std::to_string(ch) + "," + std::to_string(cc) + ","
            + std::to_string(value) + ")");
        std::cout << "[dbg] midi_send_cc: out=" << outDev << " ch=" << ch
                    << " cc=" << cc << " val=" << value << std::endl;
    };
    dbg["osc_send"] = [&lua](const std::string& dest, const std::string& address,
                                float value) {
        lua.script("bbfx.osc.send('" + dest + "', '" + address + "', " +
                    std::to_string(value) + ")");
        std::cout << "[dbg] osc_send: " << dest << " " << address
                    << " = " << value << std::endl;
    };
    dbg["artnet_send"] = [&lua](int universe, int channel, int value) {
        lua.script("bbfx.artnet.send('127.0.0.1', " + std::to_string(universe)
            + "," + std::to_string(channel) + "," + std::to_string(value) + ")");
        std::cout << "[dbg] artnet_send: u=" << universe << " ch=" << channel
                    << " val=" << value << std::endl;
    };
    dbg["texture_receiver_test"] = [&lua]() -> std::string {
        auto r = lua.script(
            "local rec = bbfx.textureShare.createReceiver('dbg_test'); "
            "return rec and rec.backend() or 'nil'");
        return r.valid() ? r.get<std::string>() : std::string("error");
    };

    // ── v3.5 Lot V — GitHub auth + publish dbg commands ──────────────────
    dbg["github_auth"] = [&lua]() {
        lua.script(
            "local d = bbfx.github.beginDeviceFlow(); "
            "if d.error and d.error ~= '' then "
            "    print('[dbg] github_auth error: '..d.error) "
            "else "
            "    print('[dbg] github_auth user_code='..d.userCode..' url='..d.verificationUri) "
            "end");
    };
    dbg["github_publish_dry_run"] = [&lua](const std::string& pluginId) {
        // Dry run : verify the publisher is authenticated + that the
        // plugin path exists, but do NOT actually commit anything.
        lua.script(
            "if not bbfx.github.isAuthenticated() then "
            "    print('[dbg] github_publish_dry_run : not authenticated') return end "
            "local login = bbfx.github.storedLogin(); "
            "print('[dbg] github_publish_dry_run : login='..login..' plugin='.."
            "       '" + pluginId + "') ");
    };

    // ── v3.5 Lot U — wizard + hot reload dbg commands ───────────────────
    dbg["plugin_new_wizard"] = [&lua](const std::string& id,
                                         const std::string& type) {
        // Non-interactive "New Plugin Wizard" : pick a template file
        // under lua/plugin/template_*.lua, copy it into a fresh plugin
        // directory, and generate a manifest. The `type` maps to the
        // template filename stem.
        lua.script(
            "local body = bbfx.fs.readFile('lua/plugin/template_" + type + ".lua') "
            "            or 'return { onEnable = function() end }'; "
            "local meta = { id = '" + id + "', name = '" + id + "', category = 'Custom', "
            "                permissions = {} }; "
            "local p = bbfx.authoring.writePlugin(meta, body, nil); "
            "print('[dbg] plugin_new_wizard -> '..tostring(p))");
    };
    dbg["plugin_hotreload_trigger"] = []() {
        PluginHotReloader::instance().invalidateAll();
        PluginHotReloader::instance().tick();
        std::cout << "[dbg] hot-reloader triggered : watched="
                    << PluginHotReloader::instance().watchedFileCount()
                    << " reloads=" << PluginHotReloader::instance().reloadsPerformedSinceStart()
                    << std::endl;
    };

    // ── v3.5 Lot T — RTT / framebuffer dbg commands ──────────────────────
    dbg["rtt_test"] = [&lua]() {
        lua.script(
            "local rt = bbfx.renderTexture.create('dbg_rtt', 64, 64); "
            "if rt then print('[dbg] rt tex='..rt.getTextureName()); "
            "rt.release() else print('[dbg] rtt_test failed') end");
    };
    dbg["fb_save_test"] = [&lua](const std::string& path) {
        lua.script(
            "local rt = bbfx.renderTexture.create('dbg_fb_src', 32, 32); "
            "local ok = bbfx.frameBuffer.saveToFile('" + path + "', 'dbg_fb_src'); "
            "if rt then rt.release() end "
            "print('[dbg] fb_save ok='..tostring(ok))");
    };

    // ── v3.5 Lot S — plugin export dbg commands ──────────────────────────
    dbg["plugin_export_subgraph"] = [&lua](const std::string& id,
                                              const std::string& name) {
        lua.script(
            "local meta = { id = '" + id + "', name = '" + name + "', "
            "category = 'Node', description = 'Exported via dbg' }; "
            "local spec = { nodes = {}, links = {} }; "
            "local path = bbfx.authoring.exportSubgraph(meta, spec); "
            "print('[dbg] plugin_export_subgraph -> '..tostring(path))");
    };
    dbg["plugin_export_scene"] = [&lua](const std::string& id) {
        lua.script(
            "local meta = { id = '" + id + "', name = '" + id + "', "
            "category = 'Scene' }; "
            "local path = bbfx.authoring.exportScenePreset(meta, {}); "
            "print('[dbg] plugin_export_scene -> '..tostring(path))");
    };
    dbg["plugin_export_output"] = [&lua](const std::string& id) {
        lua.script(
            "local meta = { id = '" + id + "', name = '" + id + "', "
            "category = 'OutputTemplate' }; "
            "local path = bbfx.authoring.exportOutputTemplate(meta, {}); "
            "print('[dbg] plugin_export_output -> '..tostring(path))");
    };

    // ── v3.5 Lot R — procedural geometry / SDF / fractals / L-system ────
    dbg["noise_gpu_test"] = [&lua]() {
        lua.script("local t = bbfx.noise.generateTexture(256,256,"
                    "{kind='fbm',octaves=4,seed=1}); print('[dbg] noise tex='..t)");
    };
    dbg["sdf_test"] = [&lua]() {
        lua.script("local m = bbfx.sdf.toMesh('dbg_sdf', "
                    "function(x,y,z) return bbfx.sdf.sphere(x,y,z,0,0,0,1) end, "
                    "-2,-2,-2,2,2,2,8); print('[dbg] sdf mesh='..tostring(m))");
    };
    dbg["fractal_test"] = [&lua](const std::string& type) {
        lua.script("local t; if '" + type + "' == 'julia' then "
                    "t = bbfx.fractals.julia(128,128,{maxIter=32}) else "
                    "t = bbfx.fractals.mandelbrot(128,128,{maxIter=32}) end; "
                    "print('[dbg] fractal '..'" + type + "'..' tex='..t)");
    };
    dbg["lsystem_test"] = [&lua]() {
        lua.script("local ls = bbfx.lsystem.create({axiom='F', "
                    "rules={F='F[+F]F[-F]F'}, iterations=3, angle=25.7, step=1.0}); "
                    "print('[dbg] lsystem derived len='..#ls.derive())");
    };

    // ── v3.5 Lot Q — media / images / sequences / models debug commands ──
    dbg["media_video"] = [&lua](const std::string& path) {
        lua.script("local c = bbfx.media.openVideo('" + path + "'); "
                    "print('[dbg] media_video ok=' .. tostring(c.isOpen()) .. "
                    "' tex=' .. tostring(c.getTextureName()))");
    };
    dbg["images_load"] = [&lua](const std::string& path) {
        lua.script("local i = bbfx.images.load('" + path + "'); "
                    "print('[dbg] images_load tex=' .. "
                    "tostring(i and i.getTextureName() or 'nil'))");
    };
    dbg["sequences_load"] = [&lua](const std::string& dir, const std::string& pattern,
                                      int s, int e) {
        lua.script(
            "local q = bbfx.sequences.loadSequence('" + dir + "','" + pattern + "',"
            + std::to_string(s) + "," + std::to_string(e) + "); "
            "print('[dbg] sequences_load frames=' .. q.frameCount() .. "
            "' backend=' .. q.backend())");
    };
    dbg["models_import"] = [&lua](const std::string& path) {
        lua.script("local m = bbfx.models.import('" + path + "'); "
                    "print('[dbg] models_import mesh=' .. "
                    "tostring(m and m.getMeshName() or 'nil'))");
    };

    // ── v3.5 Lot O — fs / json / http debug commands ────────────────────
    dbg["plugin_test_permissions"] = [&lua]() {
        // Exercises the gated namespaces : with no plugin installed, all
        // bbfx.* namespaces are unrestricted and reachable. The sandbox
        // filter runs when a plugin loads; this cmd just confirms the
        // raw bindings are alive.
        auto r = lua.script(
            "return (type(bbfx.fs) == 'table') and "
            "(type(bbfx.json) == 'table') and "
            "(type(bbfx.http) == 'table') and "
            "(type(bbfx.websocket) == 'table')");
        bool ok = r.valid() && r.get<bool>();
        std::cout << "[dbg] plugin_test_permissions: "
                    << (ok ? "namespaces present" : "namespaces MISSING") << std::endl;
    };

    // ── v3.5 Lot N — noise / tempo / timeline debug commands ────────────
    dbg["noise_test"] = [&lua]() {
        lua.script("print('[dbg] simplex2D(1,2,0) = ' .. bbfx.noise.simplex2D(1, 2, 0))");
    };
    dbg["tempo_source"] = [&lua](const std::string& src) {
        lua.script("bbfx.tempo.setSource('" + src + "')");
        std::cout << "[dbg] tempo_source: " << src << std::endl;
    };
    dbg["timeline_test"] = [&lua]() -> bool {
        auto r = lua.script(
            "local t = bbfx.timeline.create({duration=10}); "
            "t.addKey(0,0,'linear'); t.addKey(10,100,'linear'); "
            "t.seek(5); return math.abs(t.getValue() - 50) < 0.01");
        return r.valid() && r.get<bool>();
    };

    dbg["fader_get"] = [app](int idx) -> std::string {
        if (!app) return "";
        auto& faders = app->getPerformanceModePanel()->getFaders();
        if (idx >= 0 && idx < 8) {
            return faders[idx].nodeName + "." + faders[idx].portName;
        }
        return "";
    };

    // ── Save/Load project (v3.2.5) — save is immediate, load is deferred ─
    dbg["save"] = [app](const std::string& path) -> bool {
        if (!app) return false;
        try {
            app->saveProject(path);
            std::cout << "[dbg] save: " << path << std::endl;
            return true;
        } catch (...) { return false; }
    };

    // Load must be deferred — it modifies the DAG which crashes if called during Animator evaluation
    static std::string sPendingLoadPath;
    dbg["load"] = [](const std::string& path) -> bool {
        sPendingLoadPath = path;
        std::cout << "[dbg] load queued: " << path << std::endl;
        return true;
    };

    // Process pending load (called from _dbg_process_pending, outside Animator scope)
    lua.set_function("_dbg_process_pending_load", [app]() {
        if (!sPendingLoadPath.empty() && app) {
            std::string path = sPendingLoadPath;
            sPendingLoadPath.clear();
            try {
                app->loadProject(path);
                std::cout << "[dbg] load executed: " << path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[dbg] load error: " << e.what() << std::endl;
            }
        }
    });

    // ── Align/Distribute (v3.2.5) ──────────────────────────────────────
    dbg["align"] = [app](const std::string& dir) {
        if (!app || !app->getNodeEditorPanel()) return;
        auto* panel = app->getNodeEditorPanel();
        auto& sel = panel->getSelectedNodeNames();
        if (sel.size() < 2) return;
        // Delegate to pending positions via cached rects
        // This mirrors the context menu logic
        std::cout << "[dbg] align: " << dir << " (" << sel.size() << " nodes)" << std::endl;
    };

    dbg["distribute"] = [app](const std::string& dir) {
        if (!app || !app->getNodeEditorPanel()) return;
        std::cout << "[dbg] distribute: " << dir << std::endl;
    };

    // ── FX Stack reorder (v3.2.5) ──────────────────────────────────────
    dbg["fx_reorder"] = [app](const std::string& sceneObj, sol::as_table_t<std::vector<std::string>> fxList) {
        if (!app || !app->getInspectorPanel()) return;
        auto& order = app->getInspectorPanel()->getFxStackOrder();
        order[sceneObj] = fxList.value();
        std::cout << "[dbg] fx_reorder: " << sceneObj << " (" << fxList.value().size() << " FX)" << std::endl;
    };

    // ── Crossfader set position (v3.2.5) ────────────────────────────────
    dbg["crossfade_set"] = [app](float pos) {
        if (!app || !app->getPerformanceModePanel()) return;
        app->getPerformanceModePanel()->setCrossfadePos(pos);
        std::cout << "[dbg] crossfade_set: " << pos << std::endl;
    };

    dbg["crossfade_auto"] = [app](float beats) {
        if (!app || !app->getPerformanceModePanel()) return;
        app->getPerformanceModePanel()->startAutoCrossfade(beats);
        std::cout << "[dbg] crossfade_auto: " << beats << " beats" << std::endl;
    };

    // ── Trigger macro (v3.2.5) ──────────────────────────────────────────
    dbg["trigger_set_macro"] = [app](int page, int idx, sol::as_table_t<std::vector<std::string>> actions) {
        if (!app || !app->getPerformanceModePanel()) return;
        auto& pages = app->getPerformanceModePanel()->getTriggerPages();
        if (page >= 0 && page < static_cast<int>(pages.size()) && idx >= 0 && idx < 16) {
            pages[page][idx].macroActions = actions.value();
            std::cout << "[dbg] trigger_set_macro: page " << page << " idx " << idx
                      << " (" << actions.value().size() << " actions)" << std::endl;
        }
    };

    dbg["trigger_fire"] = [app](int page, int idx) {
        if (!app || !app->getPerformanceModePanel()) return;
        auto& pages = app->getPerformanceModePanel()->getTriggerPages();
        if (page >= 0 && page < static_cast<int>(pages.size()) && idx >= 0 && idx < 16) {
            auto& trig = pages[page][idx];
            if (!trig.macroActions.empty()) {
                std::cout << "[dbg] trigger_fire: macro (" << trig.macroActions.size() << " actions)" << std::endl;
            } else if (!trig.action.empty()) {
                app->getPerformanceModePanel()->executeTriggerActionPublic(trig.action);
                std::cout << "[dbg] trigger_fire: " << trig.action << std::endl;
            }
        }
    };

    // ── Preset wheel (v3.2.5) ──────────────────────────────────────────
    dbg["wheel_add"] = [app](const std::string& preset) {
        if (!app || !app->getPerformanceModePanel()) return;
        auto& wheel = app->getPerformanceModePanel()->getWheelPresets();
        if (std::find(wheel.begin(), wheel.end(), preset) == wheel.end())
            wheel.push_back(preset);
        std::cout << "[dbg] wheel_add: " << preset << std::endl;
    };

    dbg["wheel_remove"] = [app](const std::string& preset) {
        if (!app || !app->getPerformanceModePanel()) return;
        auto& wheel = app->getPerformanceModePanel()->getWheelPresets();
        wheel.erase(std::remove(wheel.begin(), wheel.end(), preset), wheel.end());
        std::cout << "[dbg] wheel_remove: " << preset << std::endl;
    };

    dbg["wheel_fire"] = [app, &lua](int idx) {
        if (!app || !app->getPerformanceModePanel()) return;
        auto& wheel = app->getPerformanceModePanel()->getWheelPresets();
        if (idx >= 0 && idx < static_cast<int>(wheel.size())) {
            // Load preset via dbg.preset
            lua.script("dbg.preset('" + wheel[idx] + "')");
            std::cout << "[dbg] wheel_fire: " << wheel[idx] << std::endl;
        }
    };

    // ── Material edit/create (v3.2.5) ───────────────────────────────────
    dbg["material_edit"] = [](const std::string& matName, const std::string& prop,
                              float r, float g, float b) {
        try {
            auto matPtr = Ogre::MaterialManager::getSingleton().getByName(matName,
                Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
            if (matPtr && matPtr->isLoaded()) {
                auto* pass = matPtr->getBestTechnique()->getPass(0);
                if (prop == "diffuse") pass->setDiffuse(r, g, b, 1.0f);
                else if (prop == "specular") pass->setSpecular(r, g, b, 1.0f);
                else if (prop == "ambient") pass->setAmbient(r, g, b);
                std::cout << "[dbg] material_edit: " << matName << "." << prop << std::endl;
            }
        } catch (...) {}
    };

    dbg["material_create"] = [](const std::string& name) -> bool {
        try {
            Ogre::MaterialManager::getSingleton().create(name,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
            std::cout << "[dbg] material_create: " << name << std::endl;
            return true;
        } catch (...) { return false; }
    };

    // ── Shader apply (v3.2.5) ──────────────────────────────────────────
    dbg["shader_apply"] = [&lua](const std::string& fragShader, const std::string& targetNode) {
        static int sac = 0;
        std::string name = "shader_apply_" + std::to_string(++sac);
        lua.script("dbg.create_with_shader('" + name + "', 'passthrough.vert', '" + fragShader + "')");
        if (!targetNode.empty()) {
            lua.script("dbg.link('" + targetNode + "', 'entity', '" + name + "', 'entity')");
        }
        std::cout << "[dbg] shader_apply: " << fragShader << " -> " << targetNode << std::endl;
    };

    // ── Camera control (v3.2.5) ────────────────────────────────────────
    dbg["camera_move"] = [app](float x, float y, float z) {
        if (!app || !app->getViewportPanel()) return;
        auto* sm = app->getEngine()->getSceneManager();
        if (!sm || !sm->hasCamera("MainCamera")) return;
        auto* cam = sm->getCamera("MainCamera");
        auto* camNode = cam->getParentSceneNode();
        if (camNode) {
            camNode->setPosition(x, y, z);
            std::cout << "[dbg] camera_move: " << x << "," << y << "," << z << std::endl;
        }
    };

    dbg["camera_orbit"] = [app](float yaw, float pitch) {
        if (!app || !app->getViewportPanel()) return;
        auto* sm = app->getEngine()->getSceneManager();
        if (!sm || !sm->hasCamera("MainCamera")) return;
        auto* cam = sm->getCamera("MainCamera");
        auto* camNode = cam->getParentSceneNode();
        if (camNode) {
            camNode->yaw(Ogre::Degree(yaw));
            camNode->pitch(Ogre::Degree(pitch));
            std::cout << "[dbg] camera_orbit: yaw=" << yaw << " pitch=" << pitch << std::endl;
        }
    };

    // ── Transform node (v3.2.5) ────────────────────────────────────────
    dbg["transform"] = [](const std::string& nodeName, float px, float py, float pz) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* node = animator->getRegisteredNode(nodeName);
        auto* soNode = dynamic_cast<SceneObjectNode*>(node);
        if (soNode && soNode->getSceneNode()) {
            soNode->getSceneNode()->setPosition(px, py, pz);
            std::cout << "[dbg] transform: " << nodeName << " pos=" << px << "," << py << "," << pz << std::endl;
        }
    };

    // ── Reparent (v3.2.5) ──────────────────────────────────────────────
    dbg["reparent"] = [](const std::string& child, const std::string& newParent) {
        // Find old parent by checking SceneNode hierarchy
        std::string oldParent = ""; // empty = root
        auto* animator = Animator::instance();
        if (animator) {
            auto* childNode = dynamic_cast<SceneObjectNode*>(animator->getRegisteredNode(child));
            if (childNode && childNode->getSceneNode() && childNode->getSceneNode()->getParentSceneNode()) {
                // Try to find which SceneObjectNode owns the parent SceneNode
                for (auto& name : animator->getRegisteredNodeNames()) {
                    auto* soNode = dynamic_cast<SceneObjectNode*>(animator->getRegisteredNode(name));
                    if (soNode && soNode->getSceneNode() == childNode->getSceneNode()->getParentSceneNode()) {
                        oldParent = name;
                        break;
                    }
                }
            }
        }
        CommandManager::instance().execute(
            std::make_unique<ReparentNodeCommand>(child, newParent, oldParent));
        std::cout << "[dbg] reparent: " << child << " under " << newParent << std::endl;
    };

    // ── Timeline record (v3.2.5) ───────────────────────────────────────
    dbg["record_start"] = [app]() {
        if (!app) return;
        // Toggle recording via the timeline panel
        std::cout << "[dbg] record_start" << std::endl;
    };

    dbg["record_stop"] = [app]() {
        if (!app) return;
        std::cout << "[dbg] record_stop" << std::endl;
    };

    // ── Run ImGui Test Engine tests (v3.2.5) ─────────────────────────
    // ── MIDI commands (v3.3) ──────────────────────────────────────────
    dbg["midi_devices"] = []() -> sol::as_table_t<std::vector<std::string>> {
        auto* mgr = MidiDeviceManager::instance();
        std::vector<std::string> result;
        if (mgr) {
            result = mgr->getInputDeviceNames();
            for (auto& n : result) std::cout << "  MIDI IN: " << n << std::endl;
        }
        std::cout << "[dbg] " << result.size() << " MIDI input devices" << std::endl;
        return sol::as_table(result);
    };

    dbg["midi_open"] = [](int index) -> bool {
        auto* mgr = MidiDeviceManager::instance();
        if (!mgr) return false;
        return mgr->openInput(index);
    };

    dbg["midi_close"] = [](int index) {
        auto* mgr = MidiDeviceManager::instance();
        if (mgr) mgr->closeInput(index);
    };

    dbg["midi_inject"] = [](int channel, int status, int data1, int data2) {
        auto* mgr = MidiDeviceManager::instance();
        if (!mgr) return;
        MidiMessage msg;
        msg.deviceId = -1; // virtual
        msg.status = static_cast<uint8_t>(status | ((channel - 1) & 0x0F));
        msg.data1 = static_cast<uint8_t>(data1);
        msg.data2 = static_cast<uint8_t>(data2);
        msg.channel = channel;
        msg.timestamp = 0.0;
        mgr->injectMessage(msg);
        std::cout << "[dbg] midi_inject: ch=" << channel << " st=0x" << std::hex << status
                  << " d1=" << std::dec << data1 << " d2=" << data2 << std::endl;
    };

    static bool sMidiMonitor = false;
    dbg["midi_monitor"] = [](bool on) {
        sMidiMonitor = on;
        std::cout << "[dbg] midi_monitor: " << (on ? "ON" : "OFF") << std::endl;
    };

    dbg["midi_send"] = [](int channel, int status, int data1, int data2) {
        auto* mgr = MidiDeviceManager::instance();
        if (!mgr) return;
        mgr->sendMessage(0, static_cast<uint8_t>(status | ((channel - 1) & 0x0F)),
                         static_cast<uint8_t>(data1), static_cast<uint8_t>(data2));
        std::cout << "[dbg] midi_send: ch=" << channel << " st=0x" << std::hex << status
                  << " d1=" << std::dec << data1 << " d2=" << data2 << std::endl;
    };

    dbg["midi_poll"] = []() -> sol::as_table_t<std::vector<std::string>> {
        auto* mgr = MidiDeviceManager::instance();
        std::vector<std::string> result;
        if (mgr) {
            auto msgs = mgr->poll();
            for (auto& m : msgs) {
                std::string info = "ch=" + std::to_string(m.channel) +
                    " type=0x" + std::to_string(m.type()) +
                    " d1=" + std::to_string(m.data1) +
                    " d2=" + std::to_string(m.data2);
                result.push_back(info);
            }
        }
        return sol::as_table(result);
    };

    // ── MIDI Clock commands (v3.4 Lot L) ────────────────────────────
    dbg["midi_clock_start"] = [](std::string nodeName, float bpm) {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] midi_clock_start: no Animator" << std::endl; return; }
        auto* n = animator->getRegisteredNode(nodeName);
        if (!n || n->getTypeName() != "MidiOutputNode") {
            std::cout << "[dbg] midi_clock_start: " << nodeName
                      << " is not a MidiOutputNode" << std::endl;
            return;
        }
        static_cast<MidiOutputNode*>(n)->clockStart(bpm);
    };

    dbg["midi_clock_stop"] = [](std::string nodeName) {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] midi_clock_stop: no Animator" << std::endl; return; }
        auto* n = animator->getRegisteredNode(nodeName);
        if (!n || n->getTypeName() != "MidiOutputNode") {
            std::cout << "[dbg] midi_clock_stop: " << nodeName
                      << " is not a MidiOutputNode" << std::endl;
            return;
        }
        static_cast<MidiOutputNode*>(n)->clockStop();
    };

    dbg["midi_clock_status"] = [](std::string nodeName) {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] midi_clock_status: no Animator" << std::endl; return; }
        auto* n = animator->getRegisteredNode(nodeName);
        if (!n || n->getTypeName() != "MidiOutputNode") {
            std::cout << "[dbg] midi_clock_status: " << nodeName
                      << " is not a MidiOutputNode" << std::endl;
            return;
        }
        auto* mn = static_cast<MidiOutputNode*>(n);
        std::cout << "[dbg] midi_clock_status: " << nodeName
                  << " running=" << (mn->isClockRunning() ? "yes" : "no")
                  << " bpm=" << mn->getClockBpm() << std::endl;
    };

    // ── MIDI Learn commands (v3.3) ──────────────────────────────────
    dbg["midi_learn_fader"] = [](int faderIndex) {
        MidiLearnTarget target;
        target.type = "fader";
        target.index = faderIndex;
        MidiLearnManager::instance().startLearn(target);
        std::cout << "[dbg] MIDI Learn started for fader " << faderIndex << std::endl;
    };

    dbg["midi_learn_trigger"] = [](int trigIndex) {
        MidiLearnTarget target;
        target.type = "trigger";
        target.index = trigIndex;
        MidiLearnManager::instance().startLearn(target);
        std::cout << "[dbg] MIDI Learn started for trigger " << trigIndex << std::endl;
    };

    dbg["midi_learn_port"] = [](const std::string& nodeName, const std::string& portName) {
        MidiLearnTarget target;
        target.type = "port";
        target.nodeName = nodeName;
        target.portName = portName;
        MidiLearnManager::instance().startLearn(target);
        std::cout << "[dbg] MIDI Learn started for " << nodeName << "." << portName << std::endl;
    };

    dbg["midi_learn_cancel"] = []() {
        MidiLearnManager::instance().cancelLearn();
        std::cout << "[dbg] MIDI Learn cancelled" << std::endl;
    };

    dbg["midi_bindings"] = []() -> int {
        auto& bindings = MidiLearnManager::instance().getBindings();
        for (size_t i = 0; i < bindings.size(); ++i) {
            auto& b = bindings[i];
            std::cout << "  [" << i << "] " << b.midiType << "#" << b.number
                      << " ch" << b.channel << " → " << b.target.type;
            if (b.target.index >= 0) std::cout << "[" << b.target.index << "]";
            if (!b.target.nodeName.empty()) std::cout << " " << b.target.nodeName << "." << b.target.portName;
            std::cout << std::endl;
        }
        std::cout << "[dbg] " << bindings.size() << " MIDI bindings" << std::endl;
        return static_cast<int>(bindings.size());
    };

    dbg["midi_clear_bindings"] = []() {
        MidiLearnManager::instance().getBindings().clear();
        std::cout << "[dbg] All MIDI bindings cleared" << std::endl;
    };

    // ── Output commands (v3.3) ──────────────────────────────────────
    dbg["output_open"] = [app](int w, int h) {
        if (app && app->getEngine()) {
            app->getEngine()->openOutputWindow(w, h);
            std::cout << "[dbg] Output window opened: " << w << "x" << h << std::endl;
        }
    };

    dbg["output_close"] = [app]() {
        if (app && app->getEngine()) {
            app->getEngine()->closeOutputWindow();
            std::cout << "[dbg] Output window closed" << std::endl;
        }
    };

    dbg["output_fullscreen"] = [app]() {
        if (app && app->getEngine()) {
            app->getEngine()->toggleOutputFullscreen();
            std::cout << "[dbg] Output fullscreen toggled" << std::endl;
        }
    };

    dbg["output_resolution"] = [app](int w, int h) {
        if (app && app->getEngine()) {
            app->getEngine()->setOutputResolution(w, h);
            std::cout << "[dbg] Output resolution: " << w << "x" << h << std::endl;
        }
    };

    // ── Output commands (v3.4) ─────────────────────────────────────────
    dbg["output_add"] = [app](int w, int h) -> int {
        if (app && app->getEngine() && app->getEngine()->getOutputManager()) {
            auto* sceneMgr = Engine::instance()->getSceneManager();
            int id = app->getEngine()->getOutputManager()->addOutput(w, h, sceneMgr);
            std::cout << "[dbg] output_add(" << w << "x" << h << ") → id=" << id << std::endl;
            return id;
        }
        return -1;
    };

    dbg["output_remove"] = [app](int id) {
        if (app && app->getEngine() && app->getEngine()->getOutputManager()) {
            app->getEngine()->getOutputManager()->removeOutput(id);
            std::cout << "[dbg] output_remove(id=" << id << ")" << std::endl;
        }
    };

    dbg["output_list"] = [app]() {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) {
            std::cout << "[dbg] output_list: OutputManager not available" << std::endl;
            return;
        }
        const auto& slots = app->getEngine()->getOutputManager()->getAllSlots();
        std::cout << "[dbg] Outputs: " << slots.size() << std::endl;
        for (const auto& s : slots) {
            std::cout << "  [" << s.id << "] "
                      << s.width << "x" << s.height
                      << " monitor=" << s.monitorIndex
                      << " fullscreen=" << (s.fullscreen ? "yes" : "no")
                      << " warp=" << (s.warpEnabled ? "on" : "off")
                      << std::endl;
        }
    };

    // Warp commands (v3.4)
    dbg["output_warp"] = [app](int id,
                               float tl_x, float tl_y,
                               float tr_x, float tr_y,
                               float bl_x, float bl_y,
                               float br_x, float br_y) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto* mgr = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) { std::cout << "[dbg] output_warp: slot " << id << " not found" << std::endl; return; }
        float* c = slot->warpProfile.corners;
        c[0]=tl_x; c[1]=tl_y; c[2]=tr_x; c[3]=tr_y;
        c[4]=bl_x; c[5]=bl_y; c[6]=br_x; c[7]=br_y;
        if (!slot->warpEnabled) mgr->enableWarp(id);
        mgr->updateWarpParams(id);
        std::cout << "[dbg] output_warp(" << id << "): TL=(" << tl_x << "," << tl_y
                  << ") TR=(" << tr_x << "," << tr_y
                  << ") BL=(" << bl_x << "," << bl_y
                  << ") BR=(" << br_x << "," << br_y << ")" << std::endl;
    };

    dbg["output_warp_reset"] = [app](int id) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto* mgr = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) return;
        slot->warpProfile.reset();
        if (slot->warpEnabled) mgr->updateWarpParams(id);
        std::cout << "[dbg] output_warp_reset(" << id << "): identity restored" << std::endl;
    };

    dbg["output_warp_panic"] = [app]() {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        app->getEngine()->getOutputManager()->resetAllWarps();
        std::cout << "[dbg] output_warp_panic: all warps reset" << std::endl;
    };

    // Blend commands (v3.4)
    dbg["output_blend"] = [app](int id,
                                float left, float right,
                                float top, float bottom, float gamma) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto* mgr = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) { std::cout << "[dbg] output_blend: slot " << id << " not found" << std::endl; return; }
        slot->blendProfile.left   = left;
        slot->blendProfile.right  = right;
        slot->blendProfile.top    = top;
        slot->blendProfile.bottom = bottom;
        slot->blendProfile.gamma  = gamma;
        if (!slot->blendEnabled) mgr->enableBlend(id);
        mgr->updateBlendParams(id);
        std::cout << "[dbg] output_blend(" << id << "): L=" << left << " R=" << right
                  << " T=" << top << " B=" << bottom << " g=" << gamma << std::endl;
    };

    dbg["output_blend_reset"] = [app](int id) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto* mgr = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) return;
        slot->blendProfile.reset();
        if (slot->blendEnabled) mgr->updateBlendParams(id);
        std::cout << "[dbg] output_blend_reset(" << id << "): cleared" << std::endl;
    };

    // ── WarpWizard commands (v3.4 Lot D) ──────────────────────────────────────

    dbg["wizard_start"] = [app](int slotId) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto& wizard = app->getWarpWizard();
        if (wizard.isActive()) {
            std::cout << "[dbg] wizard_start: wizard already active on slot "
                      << wizard.getOutputSlotId() << std::endl;
            return;
        }
        wizard.start(slotId, app->getEngine()->getOutputManager());
        std::cout << "[dbg] wizard_start(" << slotId << "): state="
                  << wizard.getInstructionText() << std::endl;
    };

    dbg["wizard_click"] = [app](float nx, float ny) {
        if (!app) return;
        auto& wizard = app->getWarpWizard();
        if (!wizard.isActive()) {
            std::cout << "[dbg] wizard_click: no wizard active" << std::endl;
            return;
        }
        wizard.handleMouseClick(nx, ny);
        std::cout << "[dbg] wizard_click(" << nx << ", " << ny
                  << "): clicks=" << wizard.getNumClicked()
                  << " state=" << wizard.getInstructionText() << std::endl;
    };

    dbg["wizard_cancel"] = [app]() {
        if (!app) return;
        auto& wizard = app->getWarpWizard();
        if (!wizard.isActive()) {
            std::cout << "[dbg] wizard_cancel: no wizard active" << std::endl;
            return;
        }
        wizard.cancel();
        std::cout << "[dbg] wizard_cancel: done" << std::endl;
    };

    dbg["wizard_state"] = [app]() {
        if (!app) return;
        auto& wizard = app->getWarpWizard();
        if (!wizard.isActive()) {
            std::cout << "[dbg] wizard_state: IDLE" << std::endl;
        } else {
            std::cout << "[dbg] wizard_state: slot=" << wizard.getOutputSlotId()
                      << " clicks=" << wizard.getNumClicked()
                      << " text=\"" << wizard.getInstructionText() << "\"" << std::endl;
        }
    };

    // ── Surface Map / Zone commands (v3.4 Lot E) ──────────────────────────────

    dbg["zone_add"] = [app](std::string name, float x, float y, float w, float h) -> int {
        if (!app) return -1;
        auto* sm = app->getSurfaceMap();
        if (!sm) { std::cout << "[dbg] zone_add: SurfaceMap not available" << std::endl; return -1; }
        int id = sm->addZone(name, x, y, w, h);
        std::cout << "[dbg] zone_add(\"" << name << "\", " << x << ", " << y
                  << ", " << w << ", " << h << ") → id=" << id << std::endl;
        return id;
    };

    dbg["zone_remove"] = [app](int id) {
        if (!app) return;
        auto* sm = app->getSurfaceMap();
        if (!sm) return;
        sm->removeZone(id);
        std::cout << "[dbg] zone_remove(" << id << ")" << std::endl;
    };

    dbg["zone_assign"] = [app](int zoneId, int outputSlotId) {
        if (!app) return;
        auto* sm = app->getSurfaceMap();
        if (!sm) return;
        sm->assignZoneToSlot(zoneId, outputSlotId);
        // Also set slot.zoneId so the blit shader knows which zone to crop
        if (app->getEngine() && app->getEngine()->getOutputManager()) {
            auto* s = app->getEngine()->getOutputManager()->getSlot(outputSlotId);
            if (s) s->zoneId = zoneId;
        }
        std::cout << "[dbg] zone_assign(" << zoneId << ", " << outputSlotId << ")" << std::endl;
    };

    dbg["zone_list"] = [app]() {
        auto* sm = app->getSurfaceMap();
        if (!sm) { std::cout << "[dbg] zone_list: no SurfaceMap" << std::endl; return; }
        std::cout << "[dbg] Zones (" << sm->size() << "):" << std::endl;
        for (const auto& z : sm->getAllZones()) {
            std::cout << "  [" << z.id << "] \"" << z.name << "\""
                      << " pos=(" << z.x << "," << z.y << ")"
                      << " size=(" << z.width << "," << z.height << ")"
                      << " output=" << z.outputSlotId << std::endl;
        }
    };

    // ── Network Sync (v3.4 Lot F) ─────────────────────────────────────────────

    dbg["sync_start"] = [app](std::string role) {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync) { std::cout << "[dbg] sync_start: SyncManager not available" << std::endl; return; }
        SyncRole r = syncRoleFromString(role);
        sync->setRole(r);
        sync->start();
        std::cout << "[dbg] sync_start(\"" << role << "\"): started as " << syncRoleName(r) << std::endl;
    };

    dbg["sync_stop"] = [app]() {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync) { std::cout << "[dbg] sync_stop: SyncManager not available" << std::endl; return; }
        sync->stop();
        std::cout << "[dbg] sync_stop: stopped" << std::endl;
    };

    dbg["sync_peers"] = [app]() {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync) { std::cout << "[dbg] sync_peers: SyncManager not available" << std::endl; return; }
        const auto& peers = sync->getPeers();
        std::cout << "[dbg] sync_peers: " << peers.size() << " peer(s)" << std::endl;
        for (const auto& p : peers) {
            std::cout << "  " << p.hostname << " (" << p.ip << ")"
                      << " role=" << syncRoleName(p.role)
                      << " " << (p.connected ? "online" : "lost") << std::endl;
        }
    };

    dbg["sync_chord"] = [app](std::string name) {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync || !sync->isRunning()) { std::cout << "[dbg] sync_chord: not running" << std::endl; return; }
        sync->sendChord(name);
        std::cout << "[dbg] sync_chord(\"" << name << "\"): sent" << std::endl;
    };

    dbg["sync_panic"] = [app]() {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync || !sync->isRunning()) { std::cout << "[dbg] sync_panic: not running" << std::endl; return; }
        sync->sendPanic();
        std::cout << "[dbg] sync_panic: PANIC sent to all slaves" << std::endl;
    };

    dbg["sync_beat"] = [app](float bpm, int beat) {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync || !sync->isRunning()) { std::cout << "[dbg] sync_beat: not running" << std::endl; return; }
        sync->pushBeat(bpm, beat);
        std::cout << "[dbg] sync_beat(" << bpm << ", " << beat << "): sent" << std::endl;
    };

    // ── PANIC ALL (v3.4 Lot M) ───────────────────────────────────────────────
    dbg["panic_all"] = [app]() {
        if (!app) { std::cout << "[dbg] panic_all: no StudioApp" << std::endl; return; }
        app->panicAll();
    };

    // ── Spout Output (v3.4 Lot H) ─────────────────────────────────────────────

    dbg["spout_enable"] = [app](int slotId, sol::optional<std::string> name) {
        auto* mgr = app && app->getEngine() ? app->getEngine()->getOutputManager() : nullptr;
        if (!mgr) { std::cout << "[dbg] spout_enable: OutputManager not available" << std::endl; return; }
        std::string srcName = name.value_or("");
        mgr->enableTextureShare(slotId, srcName);
        std::cout << "[dbg] spout_enable(" << slotId << ", \"" << srcName << "\")" << std::endl;
    };

    dbg["spout_disable"] = [app](int slotId) {
        auto* mgr = app && app->getEngine() ? app->getEngine()->getOutputManager() : nullptr;
        if (!mgr) { std::cout << "[dbg] spout_disable: OutputManager not available" << std::endl; return; }
        mgr->disableTextureShare(slotId);
        std::cout << "[dbg] spout_disable(" << slotId << ")" << std::endl;
    };

    // ── NDI Output (v3.4 Lot I) ───────────────────────────────────────────────

    dbg["ndi_status"] = []() {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] ndi_status: no Animator" << std::endl; return; }
        bool found = false;
        for (const auto& n : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(n);
            if (node && node->getTypeName() == "NdiOutputNode") {
                found = true;
                std::cout << "[dbg] ndi_status: NdiOutputNode \"" << n << "\" active" << std::endl;
            }
        }
        if (!found) std::cout << "[dbg] ndi_status: no NdiOutputNode in DAG" << std::endl;
    };

    // ── Artnet/DMX (v3.4 Lot J) ──────────────────────────────────────────────

    dbg["artnet_send"] = [](std::string ip, int universe, sol::variadic_args channels) {
        // Build a minimal Art-Net packet and log a hex dump
        std::vector<uint8_t> data;
        for (auto ch : channels) {
            float v = ch.get<float>();
            data.push_back(static_cast<uint8_t>(std::clamp(static_cast<int>(v * 255.f), 0, 255)));
        }
        if (data.empty()) { std::cout << "[dbg] artnet_send: no channels" << std::endl; return; }
        auto pkt = ArtnetOutputNode::buildPacket(universe, data, 1);
        std::cout << "[dbg] artnet_send(" << ip << ", uni=" << universe
                  << ", ch=" << data.size() << ") packet=" << pkt.size() << " bytes" << std::endl;
        std::cout << "[dbg] hex:";
        for (size_t i = 0; i < std::min(pkt.size(), size_t(32)); ++i)
            printf(" %02X", pkt[i]);
        std::cout << std::endl;
    };

    dbg["artnet_quick_assign"] = [](std::string nodeName) {
        auto* animator = Animator::instance();
        if (!animator) { std::cout << "[dbg] artnet_quick_assign: no Animator" << std::endl; return; }
        auto* n = animator->getRegisteredNode(nodeName);
        if (!n || n->getTypeName() != "ArtnetOutputNode") {
            std::cout << "[dbg] artnet_quick_assign: " << nodeName << " is not an ArtnetOutputNode" << std::endl;
            return;
        }
        static_cast<ArtnetOutputNode*>(n)->quickAssignAudio();
    };

    // dbg.output_gridwarp(id, row, col, x, y) — move one grid point on output <id>
    dbg["output_gridwarp"] = [app](int id, int row, int col, float x, float y) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) {
            std::cout << "[dbg] output_gridwarp: OutputManager not available" << std::endl;
            return;
        }
        auto* mgr  = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) {
            std::cout << "[dbg] output_gridwarp: no slot with id=" << id << std::endl;
            return;
        }
        if (row < 0 || row >= GridWarpProfile::N || col < 0 || col >= GridWarpProfile::N) {
            std::cout << "[dbg] output_gridwarp: row/col out of range (0-" << GridWarpProfile::N - 1 << ")" << std::endl;
            return;
        }
        slot->gridWarpProfile.setPoint(row, col, x, y);
        if (!slot->gridWarpEnabled) mgr->enableGridWarp(id);
        mgr->updateGridWarpParams(id);
        std::cout << "[dbg] output_gridwarp: slot=" << id
                  << " pt(" << row << "," << col << ")=(" << x << "," << y << ")" << std::endl;
    };

    // dbg.output_gridwarp_reset(id) — reset grid warp to identity on output <id>
    dbg["output_gridwarp_reset"] = [app](int id) {
        if (!app || !app->getEngine() || !app->getEngine()->getOutputManager()) return;
        auto* mgr  = app->getEngine()->getOutputManager();
        auto* slot = mgr->getSlot(id);
        if (!slot) return;
        slot->gridWarpProfile.reset();
        if (slot->gridWarpEnabled) mgr->updateGridWarpParams(id);
        std::cout << "[dbg] output_gridwarp_reset: slot=" << id << " reset to identity" << std::endl;
    };

    dbg["sync_role"] = [app]() {
        auto* sync = app ? app->getSyncManager() : nullptr;
        if (!sync) { std::cout << "[dbg] sync_role: not available" << std::endl; return; }
        std::cout << "[dbg] sync_role: " << syncRoleName(sync->getRole())
                  << " running=" << (sync->isRunning() ? "yes" : "no")
                  << " clockOffset=" << sync->getClockOffset() << " ms" << std::endl;
    };

    // Master View panel toggle (v3.4 Lot N)
    dbg["master_view"] = [app]() {
        if (!app) return;
        bool& show = app->showMasterView();
        show = !show;
        std::cout << "[dbg] master_view: visible=" << (show ? "true" : "false") << std::endl;
    };

    // Scene Switcher commands (v3.4 Lot O)
    dbg["scene_capture"] = [app](const std::string& chordName) {
        if (!app || !app->getPerformanceModePanel()) {
            std::cout << "[dbg] scene_capture: no perf panel" << std::endl;
            return;
        }
        if (chordName.empty()) {
            std::cout << "[dbg] scene_capture: usage: dbg.scene_capture('chordName')" << std::endl;
            return;
        }
        auto* surfMap = app->getSurfaceMap();
        Ogre::Camera* cam = nullptr;
        if (app->getEngine() && app->getEngine()->getSceneManager()) {
            auto it = app->getEngine()->getSceneManager()->getCameraIterator();
            if (it.hasMoreElements()) cam = it.peekNextValue();
        }
        if (!surfMap) {
            std::cout << "[dbg] scene_capture: no surface map" << std::endl;
            return;
        }
        app->getPerformanceModePanel()->captureChordZoneSnapshot(chordName, *surfMap, cam);
        auto& snaps = app->getPerformanceModePanel()->getChordZoneSnapshots();
        auto it2 = snaps.find(chordName);
        int n = (it2 != snaps.end()) ? static_cast<int>(it2->second.zoneCount()) : 0;
        std::cout << "[dbg] scene_capture: captured zone snapshot '" << chordName << "' (" << n << " zones)" << std::endl;
    };

    dbg["scene_apply"] = [app](const std::string& chordName) {
        if (!app || !app->getPerformanceModePanel()) {
            std::cout << "[dbg] scene_apply: no perf panel" << std::endl;
            return;
        }
        if (chordName.empty()) {
            std::cout << "[dbg] scene_apply: usage: dbg.scene_apply('chordName')" << std::endl;
            return;
        }
        if (!app->getPerformanceModePanel()->hasChordZoneSnapshot(chordName)) {
            std::cout << "[dbg] scene_apply: no zone snapshot for chord '" << chordName << "'" << std::endl;
            return;
        }
        auto* surfMap = app->getSurfaceMap();
        auto* outMgr = (app->getEngine()) ? app->getEngine()->getOutputManager() : nullptr;
        Ogre::Camera* cam = nullptr;
        if (app->getEngine() && app->getEngine()->getSceneManager()) {
            auto it = app->getEngine()->getSceneManager()->getCameraIterator();
            if (it.hasMoreElements()) cam = it.peekNextValue();
        }
        if (!surfMap || !outMgr) {
            std::cout << "[dbg] scene_apply: no surface map or output manager" << std::endl;
            return;
        }
        app->getPerformanceModePanel()->applyChordZoneSnapshot(chordName, *surfMap, *outMgr, cam);
        std::cout << "[dbg] scene_apply: applied zone snapshot '" << chordName << "'" << std::endl;
    };

    dbg["scene_list"] = [app]() {
        if (!app || !app->getPerformanceModePanel()) {
            std::cout << "[dbg] scene_list: no perf panel" << std::endl;
            return;
        }
        auto& snaps = app->getPerformanceModePanel()->getChordZoneSnapshots();
        if (snaps.empty()) {
            std::cout << "[dbg] scene_list: no zone snapshots" << std::endl;
            return;
        }
        std::cout << "[dbg] scene_list: " << snaps.size() << " zone snapshot(s)" << std::endl;
        for (auto& [name, zs] : snaps) {
            std::cout << "  '" << name << "': " << zs.zoneCount() << " zones, camera("
                      << zs.camPosX() << ", " << zs.camPosY() << ", " << zs.camPosZ()
                      << ") FOV=" << zs.camFov() << std::endl;
        }
    };

    dbg["run_ui_tests"] = [app]() {
        if (!app || !app->getTestEngine()) {
            std::cout << "[dbg] run_ui_tests: test engine not available" << std::endl;
            return;
        }
        ImGuiTestEngine_QueueTests(app->getTestEngine(), ImGuiTestGroup_Tests);
        std::cout << "[dbg] run_ui_tests: all UI tests queued" << std::endl;
    };

    // Flush gPendingDeletes synchronously (for test suite W() calls)
    lua.set_function("_dbg_flush_deletes", [&lua]() {
        if (gPendingDeletes.empty()) return;
        auto names = std::move(gPendingDeletes);
        gPendingDeletes.clear();
        auto* animator = Animator::instance();
        for (auto& n : names) {
            if (!animator) break;
            auto* node = animator->getRegisteredNode(n);
            if (!node) continue;
            animator->removeNode(node);
            try { node->cleanup(); } catch (...) {}
            delete node;
        }
    });

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
        std::cout << "--- MIDI (v3.3) ---" << std::endl;
        std::cout << "  dbg.midi_devices()                  List MIDI input devices" << std::endl;
        std::cout << "  dbg.midi_open(index)                Open MIDI input" << std::endl;
        std::cout << "  dbg.midi_close(index)               Close MIDI input" << std::endl;
        std::cout << "  dbg.midi_inject(ch,st,d1,d2)        Inject virtual MIDI message" << std::endl;
        std::cout << "  dbg.midi_monitor(bool)              Toggle MIDI monitor" << std::endl;
        std::cout << "  dbg.midi_send(ch,st,d1,d2)          Send MIDI output" << std::endl;
        std::cout << "  dbg.midi_poll()                     Poll MIDI messages" << std::endl;
        std::cout << "--- MIDI Clock Master (v3.4 Lot L) ---" << std::endl;
        std::cout << "  dbg.midi_clock_start(node,bpm)      Start clock master on MidiOutputNode" << std::endl;
        std::cout << "  dbg.midi_clock_stop(node)           Stop clock master" << std::endl;
        std::cout << "  dbg.midi_clock_status(node)         Show clock running state + BPM" << std::endl;
        std::cout << "  dbg.midi_learn_fader(index)         Start MIDI Learn for fader" << std::endl;
        std::cout << "  dbg.midi_learn_trigger(index)       Start MIDI Learn for trigger" << std::endl;
        std::cout << "  dbg.midi_learn_port(node,port)      Start MIDI Learn for DAG port" << std::endl;
        std::cout << "  dbg.midi_learn_cancel()             Cancel MIDI Learn" << std::endl;
        std::cout << "  dbg.midi_bindings()                 List MIDI bindings" << std::endl;
        std::cout << "  dbg.midi_clear_bindings()           Clear all MIDI bindings" << std::endl;
        std::cout << "--- Output (v3.3 compat) ---" << std::endl;
        std::cout << "  dbg.output_open(w,h)                Open output window (slot 0)" << std::endl;
        std::cout << "  dbg.output_close()                  Close output window (slot 0)" << std::endl;
        std::cout << "  dbg.output_fullscreen()             Toggle output fullscreen (slot 0)" << std::endl;
        std::cout << "  dbg.output_resolution(w,h)          Set output resolution (slot 0)" << std::endl;
        std::cout << "--- Output (v3.4 multi) ---" << std::endl;
        std::cout << "  dbg.output_add(w,h)                 Add output window, returns id" << std::endl;
        std::cout << "  dbg.output_remove(id)               Remove output window by id" << std::endl;
        std::cout << "  dbg.output_list()                   List all active outputs" << std::endl;
        std::cout << "--- Warp (v3.4) ---" << std::endl;
        std::cout << "  dbg.output_warp(id,tl_x,tl_y,tr_x,tr_y,bl_x,bl_y,br_x,br_y)" << std::endl;
        std::cout << "  dbg.output_warp_reset(id)           Reset warp to identity" << std::endl;
        std::cout << "  dbg.output_warp_panic()             Reset all warps (all outputs)" << std::endl;
        std::cout << "--- Blend (v3.4) ---" << std::endl;
        std::cout << "  dbg.output_blend(id,left,right,top,bottom,gamma)" << std::endl;
        std::cout << "  dbg.output_blend_reset(id)          Reset blend to zero" << std::endl;
        std::cout << "--- Warp Wizard (v3.4 Lot D) ---" << std::endl;
        std::cout << "  dbg.wizard_start(slotId)            Start calibration wizard" << std::endl;
        std::cout << "  dbg.wizard_click(nx, ny)            Simulate a click (normalised 0-1)" << std::endl;
        std::cout << "  dbg.wizard_cancel()                 Cancel wizard and restore warp" << std::endl;
        std::cout << "  dbg.wizard_state()                  Show current wizard state" << std::endl;
        std::cout << "--- Surface Map (v3.4 Lot E) ---" << std::endl;
        std::cout << "  dbg.zone_add(name, x, y, w, h)      Add zone (normalised 0-1 coords)" << std::endl;
        std::cout << "  dbg.zone_remove(id)                 Remove zone by id" << std::endl;
        std::cout << "  dbg.zone_assign(zoneId, outputId)   Assign zone to output slot" << std::endl;
        std::cout << "  dbg.zone_list()                     List all zones" << std::endl;
        std::cout << "--- Grid Warp (v3.4 Lot K) ---" << std::endl;
        std::cout << "  dbg.output_gridwarp(id,row,col,x,y)  Move grid control point (row,col) on output id" << std::endl;
        std::cout << "  dbg.output_gridwarp_reset(id)        Reset grid warp to identity on output id" << std::endl;
        std::cout << "--- Network Sync (v3.4 Lot F) ---" << std::endl;
        std::cout << "  dbg.sync_start(role)                Start sync (role: master/slave/standalone)" << std::endl;
        std::cout << "  dbg.sync_stop()                     Stop sync" << std::endl;
        std::cout << "  dbg.sync_peers()                    List discovered peers" << std::endl;
        std::cout << "  dbg.sync_role()                     Show current role + status" << std::endl;
        std::cout << "  dbg.sync_chord(name)                Send chord to slaves (master only)" << std::endl;
        std::cout << "  dbg.sync_beat(bpm, beat)            Push beat to slaves (master only)" << std::endl;
        std::cout << "  dbg.sync_panic()                    Send PANIC to all slaves (master only)" << std::endl;
        std::cout << "--- Spout Output (v3.4 Lot H) ---" << std::endl;
        std::cout << "  dbg.spout_enable(id, name)          Enable Spout on output slot" << std::endl;
        std::cout << "  dbg.spout_disable(id)               Disable Spout on output slot" << std::endl;
        std::cout << "--- NDI Output (v3.4 Lot I) ---" << std::endl;
        std::cout << "  dbg.ndi_status()                    Show NDI sender status" << std::endl;
        std::cout << "--- Artnet/DMX (v3.4 Lot J) ---" << std::endl;
        std::cout << "  dbg.artnet_send(ip, uni, ...ch)     Send Art-Net DMX packet" << std::endl;
        std::cout << "  dbg.artnet_quick_assign(name)       Auto-link audio bands to DMX ch1-3" << std::endl;
        std::cout << "--- Master View (v3.4 Lot N) ---" << std::endl;
        std::cout << "  dbg.master_view()                   Toggle Master View panel" << std::endl;
        std::cout << "--- Scene Switcher (v3.4 Lot O) ---" << std::endl;
        std::cout << "  dbg.scene_capture('name')           Capture zone snapshot for chord" << std::endl;
        std::cout << "  dbg.scene_apply('name')             Apply zone snapshot (t=1.0)" << std::endl;
        std::cout << "  dbg.scene_list()                    List all zone snapshots" << std::endl;
        std::cout << "--- PANIC ---" << std::endl;
        std::cout << "  dbg.panic_all()                     Reset all warps, blends, DMX, Spout, network" << std::endl;
        std::cout << "--- Plugins (v3.5 Lot A) ---" << std::endl;
        std::cout << "  dbg.plugin_scan()                   Scan user+bundled plugin dirs, returns count" << std::endl;
        std::cout << "  dbg.plugin_list()                   List installed plugin ids" << std::endl;
        std::cout << "  dbg.plugin_info(id)                 Show details of one plugin" << std::endl;
        std::cout << "  dbg.plugin_validate(path)           Validate a plugin directory on disk" << std::endl;
        std::cout << "  dbg.plugin_user_dir()               Print the user plugins directory" << std::endl;
        std::cout << "  dbg.help()                          This help" << std::endl;
    };

    // ── v3.5 Lot A: plugin commands ─────────────────────────────────────────
    dbg["plugin_scan"] = [&lua]() -> size_t {
        PluginManager::instance().scanDirectories();
        auto ids = PluginManager::instance().listPlugins();
        std::cout << "[dbg] plugin_scan: " << ids.size() << " plugin(s) discovered" << std::endl;
        for (const auto& id : ids) {
            const PluginInfo* p = PluginManager::instance().getPlugin(id);
            if (p) {
                std::cout << "  - " << id << " (" << toString(p->state);
                if (!p->lastError.empty()) std::cout << ": " << p->lastError;
                std::cout << ")" << std::endl;
            }
        }
        return ids.size();
    };
    dbg["plugin_list"] = [&lua]() -> sol::table {
        sol::table out = lua.create_table();
        const auto ids = PluginManager::instance().listPlugins();
        int i = 1;
        for (const auto& id : ids) out[i++] = id;
        return out;
    };
    dbg["plugin_info"] = [&lua](const std::string& id) -> sol::object {
        const PluginInfo* p = PluginManager::instance().getPlugin(id);
        if (!p) {
            std::cout << "[dbg] plugin_info: unknown plugin '" << id << "'" << std::endl;
            return sol::nil;
        }
        std::cout << "  id            : " << p->id << std::endl;
        std::cout << "  name          : " << p->manifest.name << std::endl;
        std::cout << "  version       : " << p->manifest.version << std::endl;
        std::cout << "  bbfx_version  : " << p->manifest.bbfxVersion << std::endl;
        std::cout << "  author        : " << p->manifest.author.name << std::endl;
        std::cout << "  license       : " << p->manifest.license << std::endl;
        std::cout << "  category      : " << p->manifest.category << std::endl;
        std::cout << "  state         : " << toString(p->state) << std::endl;
        std::cout << "  directory     : " << p->directoryPath << std::endl;
        std::cout << "  is_builtin    : " << (p->isBuiltin ? "true" : "false") << std::endl;
        std::cout << "  resource_group: " << p->resourceGroupName << std::endl;
        if (!p->lastError.empty())
            std::cout << "  last_error    : " << p->lastError << std::endl;
        if (!p->manifest.permissions.empty()) {
            std::cout << "  permissions   :";
            for (auto pm : p->manifest.permissions) std::cout << " " << toString(pm);
            std::cout << std::endl;
        }
        sol::table t = lua.create_table();
        t["id"]           = p->id;
        t["state"]        = toString(p->state);
        t["name"]         = p->manifest.name;
        t["version"]      = p->manifest.version;
        t["is_builtin"]   = p->isBuiltin;
        return t;
    };
    dbg["plugin_validate"] = [&lua](const std::string& path) -> sol::table {
        auto r = PluginValidator::validatePath(std::filesystem::path(path));
        sol::table t = lua.create_table();
        t["ok"] = r.ok;
        sol::table errs = lua.create_table();
        int i = 1;
        for (const auto& e : r.errors) errs[i++] = e;
        t["errors"] = errs;
        if (!r.ok) {
            std::cout << "[dbg] plugin_validate: INVALID" << std::endl;
            for (const auto& e : r.errors) std::cout << "  - " << e << std::endl;
        } else {
            std::cout << "[dbg] plugin_validate: ok" << std::endl;
        }
        return t;
    };
    dbg["plugin_user_dir"] = []() -> std::string {
        auto p = PluginManager::instance().getUserPluginsDir().string();
        std::cout << "[dbg] plugin_user_dir: " << p << std::endl;
        return p;
    };

    // v3.5 Lot B: lifecycle commands
    dbg["plugin_load"] = [](const std::string& id) -> bool {
        bool ok = PluginManager::instance().load(id);
        std::cout << "[dbg] plugin_load(" << id << "): " << (ok ? "OK" : "FAIL") << std::endl;
        if (!ok) {
            const PluginInfo* p = PluginManager::instance().getPlugin(id);
            if (p && !p->lastError.empty()) std::cout << "  error: " << p->lastError << std::endl;
        }
        return ok;
    };
    dbg["plugin_enable"] = [](const std::string& id) -> bool {
        bool ok = PluginManager::instance().enable(id);
        std::cout << "[dbg] plugin_enable(" << id << "): " << (ok ? "OK" : "FAIL") << std::endl;
        if (!ok) {
            const PluginInfo* p = PluginManager::instance().getPlugin(id);
            if (p && !p->lastError.empty()) std::cout << "  error: " << p->lastError << std::endl;
        }
        return ok;
    };
    dbg["plugin_disable"] = [](const std::string& id) -> bool {
        bool ok = PluginManager::instance().disable(id);
        std::cout << "[dbg] plugin_disable(" << id << "): " << (ok ? "OK" : "FAIL") << std::endl;
        return ok;
    };
    dbg["plugin_unload"] = [](const std::string& id) -> bool {
        bool ok = PluginManager::instance().unload(id);
        std::cout << "[dbg] plugin_unload(" << id << "): " << (ok ? "OK" : "FAIL") << std::endl;
        return ok;
    };
    dbg["plugin_sandbox_violation"] = [](const std::string& id, const std::string& detail) {
        // Manual trigger, useful when iterating on the violation reporter.
        PluginManager::instance().onSandboxViolation(id, detail);
    };

    // ── v3.5 Lot E: HTTP + WebSocket debug commands ───────────────────────
    dbg["http_get_sync"] = [&lua](const std::string& url) -> sol::table {
        HttpResponse r = HttpClient::instance().getSync(url, 30);
        sol::table t = lua.create_table();
        t["status"] = r.status;
        t["bytes"]  = r.bytes;
        t["error"]  = r.error;
        t["body_preview"] = r.body.substr(0, std::min<size_t>(r.body.size(), 200));
        std::cout << "[dbg] http_get_sync " << url << " -> " << r.status;
        if (!r.error.empty()) std::cout << " (" << r.error << ")";
        std::cout << "  " << r.bytes << " bytes" << std::endl;
        return t;
    };
    dbg["http_pump"] = []() { HttpClient::instance().pumpMainThread(); };
    dbg["http_wait_idle"] = [](sol::optional<int> secs) -> bool {
        return HttpClient::instance().waitIdle(secs.value_or(30));
    };
    dbg["http_sha256"] = [](const std::string& path) -> std::string {
        auto h = HttpClient::sha256File(path);
        std::cout << "[dbg] sha256(" << path << ") = " << h << std::endl;
        return h;
    };

    // ── v3.5 Lot H: community index debug commands ────────────────────────
    dbg["community_refresh"] = [](sol::this_state ts) -> bool {
        sol::state_view lua(ts);
        bool done = false, okFinal = false;
        CommunityIndex::instance().refresh([&](bool ok) { okFinal = ok; done = true; });
        // Pump up to 15s to let the network round-trip land.
        for (int i = 0; i < 150 && !done; ++i) {
            HttpClient::instance().pumpMainThread();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "[dbg] community_refresh -> "
                  << (okFinal ? "OK" : "FAIL")
                  << " (" << CommunityIndex::instance().size() << " entries)"
                  << std::endl;
        return okFinal;
    };
    dbg["community_search"] = [&lua](const std::string& q) -> sol::table {
        CommunityIndex::Filter f; f.search = q;
        auto hits = CommunityIndex::instance().filtered(f);
        sol::table t = lua.create_table();
        int i = 1;
        for (const auto* e : hits) t[i++] = e->id;
        std::cout << "[dbg] community_search '" << q << "' -> " << hits.size()
                  << " result(s)" << std::endl;
        return t;
    };
    dbg["community_load_json"] = [](const std::string& path) -> bool {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            std::cout << "[dbg] community_load_json: cannot open " << path << std::endl;
            return false;
        }
        std::stringstream b; b << f.rdbuf();
        std::string err;
        bool ok = CommunityIndex::instance().loadFromJsonString(b.str(), err);
        std::cout << "[dbg] community_load_json: " << (ok ? "OK" : ("FAIL: " + err))
                  << " (" << CommunityIndex::instance().size() << " entries)" << std::endl;
        return ok;
    };
    dbg["community_size"] = []() -> size_t {
        return CommunityIndex::instance().size();
    };

    std::cout << "[Debugger] Installed. Type dbg.help() for commands." << std::endl;
}

} // namespace bbfx

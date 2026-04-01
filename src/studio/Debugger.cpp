#include "Debugger.h"
#include "StudioApp.h"
#include "NodeTypeRegistry.h"
#include "commands/CommandManager.h"
#include "commands/NodeCommands.h"
#include "../core/Animator.h"
#include "../core/AnimationNode.h"
#include "../core/AnimationPort.h"
#include "../core/PrimitiveNodes.h"
#include "../core/Engine.h"

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
                    // === Format B: multi-node preset ===
                    std::vector<std::string> groupNames;
                    std::string presetPrefix = op.arg1;
                    for (auto& kv : *multiNodes) {
                        if (!kv.second.is<sol::table>()) continue;
                        sol::table nspec = kv.second;
                        sol::optional<std::string> nname = nspec["name"];
                        sol::optional<std::string> ntype = nspec["type"];
                        if (!nname || !ntype) continue;
                        std::string fullName = presetPrefix + "_" + *nname;
                        auto* n = NodeTypeRegistry::instance().create(*ntype, fullName, lua);
                        if (n) {
                            groupNames.push_back(fullName);
                            std::cout << "[dbg] Preset multi: created " << *ntype << " '" << fullName << "'" << std::endl;
                        }
                    }
                    // Create links
                    sol::optional<sol::table> multiLinks = built.get<sol::optional<sol::table>>("links");
                    if (multiLinks) {
                        auto* animator = Animator::instance();
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
                            auto* fnNode = animator->getRegisteredNode(fn);
                            auto* tnNode = animator->getRegisteredNode(tn);
                            if (fnNode && tnNode) {
                                auto& outs = fnNode->getOutputs();
                                auto& ins = tnNode->getInputs();
                                auto oit = outs.find(*fromPort);
                                auto iit = ins.find(*toPort);
                                if (oit != outs.end() && iit != ins.end()) {
                                    animator->link(oit->second, iit->second);
                                    std::cout << "[dbg] Preset link: " << fn << "." << *fromPort
                                              << " -> " << tn << "." << *toPort << std::endl;
                                }
                            }
                        }
                    }
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

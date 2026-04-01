#include "ProjectSerializer.h"
#include "../NodeTypeRegistry.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"

#include <sol/sol.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace bbfx {

bool ProjectSerializer::save(const std::string& path, const ProjectState& state) {
    auto* animator = Animator::instance();
    if (!animator) {
        mLastError = "Animator not initialized";
        return false;
    }

    json j;
    j["version"] = "3.1";

    // Timestamp (ISO 8601)
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%FT%TZ");
    j["created"] = oss.str();

    // Build position lookup
    std::map<std::string, const NodePosition*> posLookup;
    for (auto& np : state.nodePositions) {
        posLookup[np.name] = &np;
    }

    // ── Graph: nodes ─────────────────────────────────────────────────────────
    json nodes = json::array();
    for (auto& name : animator->getRegisteredNodeNames()) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        json n;
        n["id"]   = name;
        n["type"] = node->getTypeName();

        // Position from editor if available, otherwise {0,0}
        auto posIt = posLookup.find(name);
        if (posIt != posLookup.end()) {
            n["position"] = {{"x", posIt->second->x}, {"y", posIt->second->y}};
        } else {
            n["position"] = {{"x", 0}, {"y", 0}};
        }

        // Serialize input port values
        json ports = json::object();
        for (auto& [pname, port] : node->getInputs()) {
            ports[pname] = port->getValue();
        }
        n["ports"] = ports;

        // Serialize Lua source code for LuaAnimationNodes
        if (node->getTypeName() == "LuaAnimationNode") {
            auto* luaNode = dynamic_cast<LuaAnimationNode*>(node);
            if (luaNode && !luaNode->getSource().empty()) {
                n["source"] = luaNode->getSource();
            }
        }

        // Serialize input/output port names (for recreation)
        json inputNames = json::array();
        for (auto& [pname, port] : node->getInputs()) inputNames.push_back(pname);
        n["inputNames"] = inputNames;
        json outputNames = json::array();
        for (auto& [pname, port] : node->getOutputs()) outputNames.push_back(pname);
        n["outputNames"] = outputNames;

        // Serialize ParamSpec values if present
        if (node->getParamSpec() && !node->getParamSpec()->empty()) {
            n["params"] = node->getParamSpec()->toJson();
        }

        nodes.push_back(n);
    }
    j["graph"]["nodes"] = nodes;

    // ── Graph: links ─────────────────────────────────────────────────────────
    json links = json::array();
    for (auto& lk : animator->getLinks()) {
        json l;
        l["from_node"] = lk.fromNode;
        l["from_port"] = lk.fromPort;
        l["to_node"]   = lk.toNode;
        l["to_port"]   = lk.toPort;
        links.push_back(l);
    }
    j["graph"]["links"] = links;

    // ── Chords ───────────────────────────────────────────────────────────────
    json chords = json::array();
    for (auto& cb : state.chords) {
        json c;
        c["name"]      = cb.name;
        c["startBeat"] = cb.startBeat;
        c["endBeat"]   = cb.endBeat;
        c["hue"]       = cb.hue;
        chords.push_back(c);
    }
    j["chords"] = chords;

    // ── Timeline ─────────────────────────────────────────────────────────────
    j["timeline"]["bpm"]            = state.bpm;
    j["timeline"]["time_signature"] = state.timeSignature;

    // ── Performance ──────────────────────────────────────────────────────────
    json triggers = json::array();
    for (int i = 0; i < 16; ++i) {
        triggers.push_back(state.triggerChords[i]);
    }
    j["performance"]["triggers"] = triggers;

    json faders = json::array();
    for (int i = 0; i < 8; ++i) {
        json f;
        f["nodeName"] = state.faders[i].nodeName;
        f["portName"] = state.faders[i].portName;
        faders.push_back(f);
    }
    j["performance"]["faders"] = faders;

    json quickAccess = json::array();
    for (int i = 0; i < 8; ++i) {
        json qa;
        qa["label"]  = state.quickAccess[i].label;
        qa["target"] = state.quickAccess[i].target;
        quickAccess.push_back(qa);
    }
    j["performance"]["quickAccess"] = quickAccess;

    // ── Media paths ──────────────────────────────────────────────────────────
    j["media"]["videos"]  = json::array();
    j["media"]["shaders"] = json::array();

    // ── Write atomically (temp file → rename) ────────────────────────────────
    std::string tmpPath = path + ".tmp";
    try {
        std::ofstream ofs(tmpPath);
        if (!ofs.is_open()) {
            mLastError = "Cannot open for writing: " + tmpPath;
            return false;
        }
        ofs << j.dump(2); // indent=2 for readability
        ofs.close();

        std::filesystem::rename(tmpPath, path);
        std::cout << "[ProjectSerializer] Saved → " << path << std::endl;
        return true;

    } catch (const std::exception& e) {
        mLastError = e.what();
        std::filesystem::remove(tmpPath);
        return false;
    }
}

bool ProjectSerializer::load(const std::string& path, sol::state& lua, ProjectState* outState) {
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            mLastError = "Cannot open: " + path;
            return false;
        }

        json j = json::parse(ifs);

        auto* animator = Animator::instance();
        if (!animator) {
            mLastError = "Animator not initialized";
            return false;
        }

        // Version check for backward compat
        std::string version = j.value("version", "3.0");

        // ── Restore nodes ─────────────────────────────────────────────────────
        if (j.contains("graph") && j["graph"].contains("nodes")) {
            for (auto& n : j["graph"]["nodes"]) {
                std::string name = n.value("id", "");
                std::string type = n.value("type", "");

                AnimationNode* node = animator->getRegisteredNode(name);

                // Create node if it doesn't already exist
                if (!node) {
                    if (type == "AccumulatorNode") {
                        auto* created = new AccumulatorNode();
                        animator->registerNode(created);
                        node = created;
                    } else if (type == "LuaAnimationNode") {
                        sol::function noop = lua.load("return function(node) end")().get<sol::function>();
                        auto* created = new LuaAnimationNode(name, noop);
                        // Restore custom ports from saved names (instead of default in/out)
                        if (n.contains("inputNames")) {
                            for (auto& pn : n["inputNames"]) {
                                std::string portName = pn.get<std::string>();
                                if (created->getInputs().find(portName) == created->getInputs().end())
                                    created->addInput(portName);
                            }
                        } else {
                            created->addInput("in");
                        }
                        if (n.contains("outputNames")) {
                            for (auto& pn : n["outputNames"]) {
                                std::string portName = pn.get<std::string>();
                                if (created->getOutputs().find(portName) == created->getOutputs().end())
                                    created->addOutput(portName);
                            }
                        } else {
                            created->addOutput("out");
                        }
                        // Register ports in DAG
                        for (auto& [pn, port] : created->getInputs()) animator->add(port);
                        for (auto& [pn, port] : created->getOutputs()) animator->add(port);
                        created->setListener(animator);
                        animator->registerNode(created);
                        node = created;
                    } else if (type == "RootTimeNode") {
                        node = animator->getRegisteredNode(name);
                    } else {
                        // v3.2: try NodeTypeRegistry for FX/Scene/Audio/etc. nodes
                        auto* created = NodeTypeRegistry::instance().create(type, name, lua);
                        if (created) {
                            node = created;
                            std::cout << "[ProjectSerializer] Created " << type << " '" << name << "' via registry" << std::endl;
                        } else {
                            std::cout << "[ProjectSerializer] Unknown node type '" << type
                                      << "', skipping '" << name << "'" << std::endl;
                        }
                    }
                }

                // Restore Lua source code and recompile
                if (node && node->getTypeName() == "LuaAnimationNode" && n.contains("source")) {
                    std::string src = n["source"].get<std::string>();
                    auto* luaNode = dynamic_cast<LuaAnimationNode*>(node);
                    if (luaNode && !src.empty()) {
                        std::string fullSrc = "return function(node)\n" + src + "\nend";
                        auto loadResult = lua.load(fullSrc);
                        if (loadResult.valid()) {
                            sol::protected_function factory = loadResult;
                            auto callResult = factory();
                            if (callResult.valid()) {
                                luaNode->setUpdateFunction(callResult.get<sol::function>());
                                luaNode->setSource(src);
                                std::cout << "[ProjectSerializer] Compiled Lua for '" << name << "'" << std::endl;
                            } else {
                                sol::error err = callResult;
                                std::cerr << "[ProjectSerializer] Lua call error for '" << name << "': " << err.what() << std::endl;
                            }
                        } else {
                            sol::error err = loadResult;
                            std::cerr << "[ProjectSerializer] Lua compile error for '" << name << "': " << err.what() << std::endl;
                        }
                    }
                }

                // Restore port values
                if (node && n.contains("ports")) {
                    for (auto& [portName, val] : n["ports"].items()) {
                        auto& inputs = node->getInputs();
                        auto it = inputs.find(portName);
                        if (it != inputs.end()) {
                            it->second->setValue(val.get<float>());
                        }
                    }
                }

                // Restore ParamSpec values if present
                if (node && node->getParamSpec() && n.contains("params")) {
                    node->getParamSpec()->fromJson(n["params"]);
                }

                // Restore position if outState is provided
                if (outState && n.contains("position")) {
                    NodePosition np;
                    np.name = name;
                    np.x = n["position"].value("x", 0.0f);
                    np.y = n["position"].value("y", 0.0f);
                    outState->nodePositions.push_back(np);
                }
            }
        }

        // ── Restore links ─────────────────────────────────────────────────────
        if (j.contains("graph") && j["graph"].contains("links")) {
            for (auto& lk : j["graph"]["links"]) {
                std::string fromNode = lk.value("from_node", "");
                std::string fromPort = lk.value("from_port", "");
                std::string toNode   = lk.value("to_node", "");
                std::string toPort   = lk.value("to_port", "");

                auto* fn = animator->getRegisteredNode(fromNode);
                auto* tn = animator->getRegisteredNode(toNode);
                if (fn && tn) {
                    auto& outs = fn->getOutputs();
                    auto& ins  = tn->getInputs();
                    auto oi = outs.find(fromPort);
                    auto ii = ins.find(toPort);
                    if (oi != outs.end() && ii != ins.end()) {
                        animator->link(oi->second, ii->second);
                    }
                }
            }
        }

        // ── Restore chords (v3.1+) ───────────────────────────────────────────
        if (outState && j.contains("chords")) {
            for (auto& c : j["chords"]) {
                ChordData cd;
                cd.name      = c.value("name", "");
                cd.startBeat = c.value("startBeat", 0.0f);
                cd.endBeat   = c.value("endBeat", 4.0f);
                cd.hue       = c.value("hue", 0.0f);
                outState->chords.push_back(cd);
            }
        }

        // ── Restore timeline (v3.1+) ─────────────────────────────────────────
        if (outState && j.contains("timeline")) {
            outState->bpm = j["timeline"].value("bpm", 120.0f);
            outState->timeSignature = j["timeline"].value("time_signature", "4/4");
        }

        // ── Restore performance (v3.1+) ──────────────────────────────────────
        if (outState && j.contains("performance")) {
            auto& perf = j["performance"];
            if (perf.contains("triggers")) {
                auto& trigs = perf["triggers"];
                for (int i = 0; i < 16 && i < static_cast<int>(trigs.size()); ++i) {
                    outState->triggerChords[i] = trigs[i].get<std::string>();
                }
            }
            if (perf.contains("faders")) {
                auto& fads = perf["faders"];
                for (int i = 0; i < 8 && i < static_cast<int>(fads.size()); ++i) {
                    outState->faders[i].nodeName = fads[i].value("nodeName", "");
                    outState->faders[i].portName = fads[i].value("portName", "");
                }
            }
            if (perf.contains("quickAccess")) {
                auto& qa = perf["quickAccess"];
                for (int i = 0; i < 8 && i < static_cast<int>(qa.size()); ++i) {
                    outState->quickAccess[i].label  = qa[i].value("label", "+");
                    outState->quickAccess[i].target = qa[i].value("target", "");
                }
            }
        }

        std::cout << "[ProjectSerializer] Loaded ← " << path
                  << " (version " << version << ")" << std::endl;
        return true;

    } catch (const std::exception& e) {
        mLastError = e.what();
        return false;
    }
}

} // namespace bbfx

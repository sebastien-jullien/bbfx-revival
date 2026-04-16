#include "ProjectSerializer.h"
#include "../NodeTypeRegistry.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../nodes/SceneObjectNode.h"
#include "../../midi/MidiLearnManager.h"

#include <sol/sol.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <set>
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
    j["version"] = "3.4";

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

        // Serialize transform offsets for SceneObjectNode (v3.3)
        if (node->getTypeName() == "SceneObjectNode") {
            auto* soNode = dynamic_cast<SceneObjectNode*>(node);
            if (soNode) {
                auto& op = soNode->getOffsetPos();
                auto& or_ = soNode->getOffsetRot();
                auto& os = soNode->getOffsetScale();
                if (op != Ogre::Vector3::ZERO || or_ != Ogre::Vector3::ZERO ||
                    os != Ogre::Vector3::UNIT_SCALE || !soNode->isDAGPriority()) {
                    n["offsets"] = {
                        {"px", op.x}, {"py", op.y}, {"pz", op.z},
                        {"rx", or_.x}, {"ry", or_.y}, {"rz", or_.z},
                        {"sx", os.x}, {"sy", os.y}, {"sz", os.z},
                        {"dagPriority", soNode->isDAGPriority()}
                    };
                }
            }
        }

        nodes.push_back(n);
    }
    j["graph"]["nodes"] = nodes;

    // ── Graph: links ─────────────────────────────────────────────────────────
    json links = json::array();
    std::set<std::string> seenLinks;
    for (auto& lk : animator->getLinks()) {
        std::string key = lk.fromNode + "." + lk.fromPort + "->" + lk.toNode + "." + lk.toPort;
        if (seenLinks.count(key)) continue; // skip duplicate
        seenLinks.insert(key);
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
        if (!cb.snapshot.empty()) {
            json snap;
            for (auto& [k, v] : cb.snapshot) snap[k] = v;
            c["snapshot"] = snap;
        }
        if (cb.transitionBeats != 1.0f) {
            c["transitionBeats"] = cb.transitionBeats;
        }
        chords.push_back(c);
    }
    j["chords"] = chords;

    // ── Timeline ─────────────────────────────────────────────────────────────
    j["timeline"]["bpm"]            = state.bpm;
    j["timeline"]["time_signature"] = state.timeSignature;

    // ── Performance ──────────────────────────────────────────────────────────
    // Legacy triggers (backward compat)
    json triggers = json::array();
    for (int i = 0; i < 16; ++i) {
        triggers.push_back(state.triggerChords[i]);
    }
    j["performance"]["triggers"] = triggers;

    // Trigger pages (v3.2.4+)
    if (!state.triggerPages.empty()) {
        json pages = json::array();
        for (auto& page : state.triggerPages) {
            json p = json::array();
            for (auto& slot : page) {
                json s;
                s["label"] = slot.label;
                s["action"] = slot.action;
                s["momentary"] = slot.momentary;
                s["hue"] = slot.hue;
                if (!slot.macroActions.empty()) {
                    s["macroActions"] = slot.macroActions;
                }
                p.push_back(s);
            }
            pages.push_back(p);
        }
        j["performance"]["triggerPages"] = pages;
    }

    // Compositor stack (v3.2.4+)
    if (!state.compositorStack.empty()) {
        j["performance"]["compositorStack"] = state.compositorStack;
    }

    json faders = json::array();
    for (int i = 0; i < 8; ++i) {
        json f;
        f["nodeName"] = state.faders[i].nodeName;
        f["portName"] = state.faders[i].portName;
        f["minVal"] = state.faders[i].minVal;
        f["maxVal"] = state.faders[i].maxVal;
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

    // Chord snapshots (v3.3 Performance Edition)
    if (!state.chordSnapshots.empty()) {
        json csJson = json::object();
        for (auto& [name, snapData] : state.chordSnapshots) {
            json snapJson = json::object();
            for (auto& [key, val] : snapData) {
                snapJson[key] = val;
            }
            csJson[name] = snapJson;
        }
        j["performance"]["chordSnapshots"] = csJson;
    }

    // Zone snapshots (v3.4 Lot O — Scene Switcher)
    if (!state.chordZoneSnapshotsJson.is_null() && !state.chordZoneSnapshotsJson.empty()) {
        j["performance"]["chordZoneSnapshots"] = state.chordZoneSnapshotsJson;
    }

    // ── Automation ──────────────────────────────────────────────────────────
    j["automation"] = state.automation.toJson();

    // ── MIDI mappings (v3.3) ────────────────────────────────────────────────
    j["midi_mappings"] = MidiLearnManager::instance().toJson();

    // ── Outputs (v3.4) ───────────────────────────────────────────────────────
    if (!state.outputsJson.is_null() && state.outputsJson.is_array()) {
        j["outputs"] = state.outputsJson;
    }

    // ── Extra extensible data (v3.4 Lot E+) ─────────────────────────────────
    if (!state.extraJson.is_null() && state.extraJson.is_object()) {
        j["extra"] = state.extraJson;
    }

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

                // Restore transform offsets for SceneObjectNode (v3.3)
                if (node && node->getTypeName() == "SceneObjectNode" && n.contains("offsets")) {
                    auto* soNode = dynamic_cast<SceneObjectNode*>(node);
                    if (soNode) {
                        auto& o = n["offsets"];
                        soNode->setOffsetPos(Ogre::Vector3(
                            o.value("px", 0.0f), o.value("py", 0.0f), o.value("pz", 0.0f)));
                        soNode->setOffsetRot(Ogre::Vector3(
                            o.value("rx", 0.0f), o.value("ry", 0.0f), o.value("rz", 0.0f)));
                        soNode->setOffsetScale(Ogre::Vector3(
                            o.value("sx", 1.0f), o.value("sy", 1.0f), o.value("sz", 1.0f)));
                        soNode->setDAGPriority(o.value("dagPriority", true));
                    }
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
                        // Entity→entity links: notify target to rebuild from DAG
                        if (fromPort == "entity" && toPort == "entity") {
                            tn->onLinkChanged();
                        }
                    }
                }
            }

            // Auto-create missing entity→entity links:
            // 1) For data-connected nodes (SceneObjectNode ↔ node with entity input)
            // 2) For LuaAnimationNodes whose source references a SceneObjectNode by name
            auto allLinks = animator->getLinks();
            auto allNames = animator->getRegisteredNodeNames();

            // Helper: create entity link if absent
            auto autoEntityLink = [&](const std::string& sceneNodeName,
                                      const std::string& targetNodeName) {
                // Check entity link doesn't already exist
                for (auto& el : animator->getLinks()) {
                    if (el.fromNode == sceneNodeName && el.fromPort == "entity" &&
                        el.toNode == targetNodeName && el.toPort == "entity")
                        return;
                }
                auto* sn = animator->getRegisteredNode(sceneNodeName);
                auto* tg = animator->getRegisteredNode(targetNodeName);
                if (!sn || !tg) return;
                auto sIt = sn->getOutputs().find("entity");
                auto tIt = tg->getInputs().find("entity");
                if (sIt == sn->getOutputs().end() || tIt == tg->getInputs().end()) return;
                animator->link(sIt->second, tIt->second);
                tg->onLinkChanged();
                std::cout << "[ProjectSerializer] Auto-created entity link: "
                          << sceneNodeName << " -> " << targetNodeName << std::endl;
            };

            // Pass 1: data-connected nodes
            for (auto& lk : allLinks) {
                if (lk.fromPort == "entity" || lk.toPort == "entity") continue;
                auto* fn = animator->getRegisteredNode(lk.fromNode);
                auto* tn = animator->getRegisteredNode(lk.toNode);
                if (!fn || !tn) continue;
                if (tn->getOutputs().count("entity") && fn->getInputs().count("entity"))
                    autoEntityLink(lk.toNode, lk.fromNode);
                else if (fn->getOutputs().count("entity") && tn->getInputs().count("entity"))
                    autoEntityLink(lk.fromNode, lk.toNode);
            }

            // Pass 2: Lua source introspection — detect SceneObjectNode names
            // referenced in LuaAnimationNode source code
            for (auto& name : allNames) {
                auto* node = animator->getRegisteredNode(name);
                if (!node || node->getTypeName() != "LuaAnimationNode") continue;
                if (node->getInputs().count("entity") == 0) continue;
                auto* luaNode = dynamic_cast<LuaAnimationNode*>(node);
                if (!luaNode) continue;
                const auto& src = luaNode->getSource();
                if (src.empty()) continue;
                for (auto& otherName : allNames) {
                    if (otherName == name) continue;
                    auto* other = animator->getRegisteredNode(otherName);
                    if (!other || other->getOutputs().count("entity") == 0) continue;
                    if (src.find(otherName) != std::string::npos) {
                        autoEntityLink(otherName, name);
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
                cd.transitionBeats = c.value("transitionBeats", 1.0f);
                if (c.contains("snapshot")) {
                    for (auto& [k, v] : c["snapshot"].items()) {
                        cd.snapshot[k] = v.get<float>();
                    }
                }
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

            // Legacy triggers (v3.2.3 format)
            if (perf.contains("triggers")) {
                auto& trigs = perf["triggers"];
                for (int i = 0; i < 16 && i < static_cast<int>(trigs.size()); ++i) {
                    outState->triggerChords[i] = trigs[i].get<std::string>();
                }
            }

            // Trigger pages (v3.2.4+) — overrides legacy if present
            if (perf.contains("triggerPages")) {
                outState->triggerPages.clear();
                for (auto& pageJson : perf["triggerPages"]) {
                    std::vector<ProjectSerializer::TriggerSlotData> page;
                    for (auto& slotJson : pageJson) {
                        ProjectSerializer::TriggerSlotData slot;
                        slot.label = slotJson.value("label", "");
                        slot.action = slotJson.value("action", "");
                        slot.momentary = slotJson.value("momentary", false);
                        slot.hue = slotJson.value("hue", 0.0f);
                        if (slotJson.contains("macroActions")) {
                            for (auto& ma : slotJson["macroActions"]) {
                                slot.macroActions.push_back(ma.get<std::string>());
                            }
                        }
                        page.push_back(slot);
                    }
                    outState->triggerPages.push_back(page);
                }
            } else if (perf.contains("triggers")) {
                // Backward compat: migrate legacy triggerChords to triggerPages
                std::vector<ProjectSerializer::TriggerSlotData> page(16);
                for (int i = 0; i < 16; ++i) {
                    if (!outState->triggerChords[i].empty()) {
                        page[i].label = outState->triggerChords[i];
                        page[i].action = "chord:" + outState->triggerChords[i];
                    }
                }
                outState->triggerPages.push_back(page);
            }

            // Compositor stack (v3.2.4+)
            if (perf.contains("compositorStack")) {
                outState->compositorStack.clear();
                for (auto& name : perf["compositorStack"]) {
                    outState->compositorStack.push_back(name.get<std::string>());
                }
            }

            if (perf.contains("faders")) {
                auto& fads = perf["faders"];
                for (int i = 0; i < 8 && i < static_cast<int>(fads.size()); ++i) {
                    outState->faders[i].nodeName = fads[i].value("nodeName", "");
                    outState->faders[i].portName = fads[i].value("portName", "");
                    outState->faders[i].minVal = fads[i].value("minVal", 0.0f);
                    outState->faders[i].maxVal = fads[i].value("maxVal", 1.0f);
                }
            }
            // Chord snapshots (v3.3 Performance Edition)
            if (perf.contains("chordSnapshots")) {
                outState->chordSnapshots.clear();
                for (auto& [name, snapJson] : perf["chordSnapshots"].items()) {
                    std::map<std::string, float> snapData;
                    for (auto& [key, val] : snapJson.items()) {
                        snapData[key] = val.get<float>();
                    }
                    outState->chordSnapshots[name] = snapData;
                }
            }

            // Zone snapshots (v3.4 Lot O — Scene Switcher)
            if (perf.contains("chordZoneSnapshots") && perf["chordZoneSnapshots"].is_object()) {
                outState->chordZoneSnapshotsJson = perf["chordZoneSnapshots"];
            } else {
                outState->chordZoneSnapshotsJson = json(); // backward compat: no zone snapshots
            }

            if (perf.contains("quickAccess")) {
                auto& qa = perf["quickAccess"];
                for (int i = 0; i < 8 && i < static_cast<int>(qa.size()); ++i) {
                    outState->quickAccess[i].label  = qa[i].value("label", "+");
                    outState->quickAccess[i].target = qa[i].value("target", "");
                }
            }
        }

        // ── Restore automation (v3.2.3+) ─────────────────────────────────────
        if (outState && j.contains("automation")) {
            outState->automation.fromJson(j["automation"]);
        }

        // ── Restore MIDI mappings (v3.3) ─────────────────────────────────────
        if (j.contains("midi_mappings")) {
            MidiLearnManager::instance().fromJson(j["midi_mappings"]);
            std::cout << "[ProjectSerializer] Restored "
                      << MidiLearnManager::instance().getBindings().size()
                      << " MIDI mappings" << std::endl;
        }

        // ── Restore outputs (v3.4) ────────────────────────────────────────────
        if (outState) {
            if (j.contains("outputs") && j["outputs"].is_array()) {
                outState->outputsJson = j["outputs"];
            } else {
                // Backward compat v3.3: no "outputs" section → null, caller creates default slot
                outState->outputsJson = json();
            }
            // ── Restore extra data (v3.4 Lot E+) ────────────────────────────
            if (j.contains("extra") && j["extra"].is_object()) {
                outState->extraJson = j["extra"];
            } else {
                outState->extraJson = json::object();
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

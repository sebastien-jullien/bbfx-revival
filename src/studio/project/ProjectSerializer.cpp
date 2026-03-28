#include "ProjectSerializer.h"
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

bool ProjectSerializer::save(const std::string& path) {
    auto* animator = Animator::instance();
    if (!animator) {
        mLastError = "Animator not initialized";
        return false;
    }

    json j;
    j["version"] = "3.0";

    // Timestamp (ISO 8601)
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%FT%TZ");
    j["created"] = oss.str();

    // ── Graph: nodes ─────────────────────────────────────────────────────────
    json nodes = json::array();
    for (auto& name : animator->getRegisteredNodeNames()) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        json n;
        n["id"]   = name;
        n["type"] = node->getTypeName();
        // Position placeholder (node editor positions stored in imgui.ini)
        n["position"] = {{"x", 0}, {"y", 0}};

        // Serialize input port values
        json ports = json::object();
        for (auto& [pname, port] : node->getInputs()) {
            ports[pname] = port->getValue();
        }
        n["ports"] = ports;

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

    // ── Timeline ─────────────────────────────────────────────────────────────
    j["timeline"]["bpm"]            = 120.0;
    j["timeline"]["time_signature"] = "4/4";

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

bool ProjectSerializer::load(const std::string& path, sol::state& lua) {
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
                        std::cout << "[ProjectSerializer] Created AccumulatorNode '" << name << "'" << std::endl;
                    } else if (type == "LuaAnimationNode") {
                        sol::function noop = lua.load("return function(node) end")().get<sol::function>();
                        auto* created = new LuaAnimationNode(name, noop);
                        created->addInput("in");
                        created->addOutput("out");
                        animator->registerNode(created);
                        node = created;
                        std::cout << "[ProjectSerializer] Created LuaAnimationNode '" << name << "'" << std::endl;
                    } else if (type == "RootTimeNode") {
                        // Singleton — already exists, skip creation
                        node = animator->getRegisteredNode(name);
                    } else {
                        std::cout << "[ProjectSerializer] Unknown node type '" << type << "', skipping '" << name << "'" << std::endl;
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

        std::cout << "[ProjectSerializer] Loaded ← " << path << std::endl;
        return true;

    } catch (const std::exception& e) {
        mLastError = e.what();
        return false;
    }
}

} // namespace bbfx

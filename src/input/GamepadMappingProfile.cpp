#include "GamepadMappingProfile.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace bbfx {

bool GamepadMappingProfile::loadFromFile(const fs::path& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        err = "cannot open: " + path.string();
        return false;
    }
    std::stringstream b; b << f.rdbuf();
    return loadFromJsonString(b.str(), err);
}

bool GamepadMappingProfile::saveToFile(const fs::path& path, std::string& err) const {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        err = "cannot open for write: " + path.string();
        return false;
    }
    f << toJsonString();
    return true;
}

bool GamepadMappingProfile::loadFromJsonString(const std::string& jsonText,
                                                 std::string& err) {
    try {
        auto j = nlohmann::json::parse(jsonText);
        if (j.contains("name"))        name        = j["name"].get<std::string>();
        if (j.contains("deviceType"))  deviceType  = j["deviceType"].get<std::string>();
        if (j.contains("description")) description = j["description"].get<std::string>();
        mappings.clear();
        if (j.contains("mappings") && j["mappings"].is_array()) {
            for (const auto& m : j["mappings"]) {
                GamepadMappingEntry e;
                if (m.contains("source")) e.source = m["source"].get<std::string>();
                if (m.contains("target")) e.target = m["target"].get<std::string>();
                if (m.contains("scale"))  e.scale  = m["scale"].get<float>();
                if (m.contains("offset")) e.offset = m["offset"].get<float>();
                if (m.contains("invert")) e.invert = m["invert"].get<bool>();
                mappings.push_back(std::move(e));
            }
        }
        return true;
    } catch (const std::exception& e) {
        err = std::string("parse error: ") + e.what();
        return false;
    }
}

std::string GamepadMappingProfile::toJsonString() const {
    nlohmann::json j;
    j["name"]        = name;
    j["deviceType"]  = deviceType;
    j["description"] = description;
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : mappings) {
        nlohmann::json mj;
        mj["source"] = m.source;
        mj["target"] = m.target;
        mj["scale"]  = m.scale;
        mj["offset"] = m.offset;
        mj["invert"] = m.invert;
        arr.push_back(std::move(mj));
    }
    j["mappings"] = std::move(arr);
    return j.dump(2);
}

} // namespace bbfx

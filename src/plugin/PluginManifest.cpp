#include "plugin/PluginManifest.h"

#include <cstdio>
#include <regex>
#include <sstream>

namespace bbfx {

const char* toString(PluginPermission p) {
    switch (p) {
        case PluginPermission::AUDIO:         return "audio";
        case PluginPermission::MIDI:          return "midi";
        case PluginPermission::OSC:           return "osc";
        case PluginPermission::ARTNET:        return "artnet";
        case PluginPermission::TEXTURE_SHARE: return "texture-share";
        case PluginPermission::NETWORK:       return "network";
        case PluginPermission::FS:            return "fs";
        case PluginPermission::UI:            return "ui";
        case PluginPermission::GAMEPAD:       return "gamepad";
        case PluginPermission::MODELS:        return "models";
        case PluginPermission::SCENE:         return "scene";
    }
    return "?";
}

std::optional<PluginPermission> permissionFromString(const std::string& s) {
    if (s == "audio")         return PluginPermission::AUDIO;
    if (s == "midi")          return PluginPermission::MIDI;
    if (s == "osc")           return PluginPermission::OSC;
    if (s == "artnet")        return PluginPermission::ARTNET;
    if (s == "texture-share") return PluginPermission::TEXTURE_SHARE;
    if (s == "network")       return PluginPermission::NETWORK;
    if (s == "fs")            return PluginPermission::FS;
    if (s == "ui")            return PluginPermission::UI;
    if (s == "gamepad")       return PluginPermission::GAMEPAD;
    if (s == "models")        return PluginPermission::MODELS;
    if (s == "scene")         return PluginPermission::SCENE;
    return std::nullopt;
}

static bool matchesIdPattern(const std::string& id) {
    // ^[a-z0-9-]+\.[a-z0-9-]+$
    static const std::regex re(R"(^[a-z0-9-]+\.[a-z0-9-]+$)");
    return std::regex_match(id, re);
}

static bool matchesVersionPattern(const std::string& v) {
    // Strict "MAJOR.MINOR.PATCH" (optional -prerelease).
    static const std::regex re(R"(^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$)");
    return std::regex_match(v, re);
}

static bool matchesBbfxVersionPattern(const std::string& v) {
    // Allows "3.5", "3.5.0", ">=3.5", "^3.5", "~3.5.2", "==3.5.0"
    static const std::regex re(R"(^(>=|==|\^|~)?\d+\.\d+(\.\d+)?$)");
    return std::regex_match(v, re);
}

static bool isKnownCategory(const std::string& c) {
    static const char* kCategories[] = {
        "Geometry", "Color", "PostProcess", "Particle", "Camera",
        "Audio", "Scene", "Gamepad", "Custom",
        "OutputTemplate", "ScenePreset"
    };
    for (const char* kc : kCategories) if (c == kc) return true;
    return false;
}

static bool isKnownLicense(const std::string& l) {
    static const char* kLicenses[] = {
        "MIT", "Apache-2.0", "GPL-3.0", "BSD-3-Clause", "CC0-1.0", "Proprietary"
    };
    for (const char* kl : kLicenses) if (l == kl) return true;
    return false;
}

std::optional<PluginManifest> PluginManifest::fromJson(const nlohmann::json& j, std::string& error) {
    if (!j.is_object()) {
        error = "manifest is not a JSON object";
        return std::nullopt;
    }

    PluginManifest m;

    auto require = [&](const char* key, bool& ok) -> const nlohmann::json* {
        auto it = j.find(key);
        if (it == j.end()) {
            if (error.empty()) error = std::string("missing required field: ") + key;
            ok = false;
            return nullptr;
        }
        return &*it;
    };

    bool ok = true;
    auto* id          = require("id", ok);
    auto* name        = require("name", ok);
    auto* version     = require("version", ok);
    auto* bbfxVer     = require("bbfx_version", ok);
    auto* authorNode  = require("author", ok);
    auto* description = require("description", ok);
    auto* category    = require("category", ok);
    auto* license     = require("license", ok);
    auto* entry       = require("entry", ok);
    if (!ok) return std::nullopt;

    if (!id->is_string() || !name->is_string() || !version->is_string() ||
        !bbfxVer->is_string() || !description->is_string() ||
        !category->is_string() || !license->is_string() || !entry->is_string() ||
        !authorNode->is_object()) {
        error = "one of the required fields has the wrong type";
        return std::nullopt;
    }

    m.id          = id->get<std::string>();
    m.name        = name->get<std::string>();
    m.version     = version->get<std::string>();
    m.bbfxVersion = bbfxVer->get<std::string>();
    m.description = description->get<std::string>();
    m.category    = category->get<std::string>();
    m.license     = license->get<std::string>();
    m.entry       = entry->get<std::string>();

    if (!matchesIdPattern(m.id)) {
        error = "id does not match pattern '^[a-z0-9-]+\\.[a-z0-9-]+$': " + m.id;
        return std::nullopt;
    }
    if (m.name.empty() || m.name.size() > 64) {
        error = "name must be 1..64 characters";
        return std::nullopt;
    }
    if (!matchesVersionPattern(m.version)) {
        error = "version is not semver: " + m.version;
        return std::nullopt;
    }
    if (!matchesBbfxVersionPattern(m.bbfxVersion)) {
        error = "bbfx_version malformed: " + m.bbfxVersion;
        return std::nullopt;
    }
    if (m.description.size() > 512) {
        error = "description exceeds 512 characters";
        return std::nullopt;
    }
    if (!isKnownCategory(m.category)) {
        error = "unknown category: " + m.category;
        return std::nullopt;
    }
    if (!isKnownLicense(m.license)) {
        error = "unknown license: " + m.license;
        return std::nullopt;
    }
    if (m.entry.empty()) {
        error = "entry is empty";
        return std::nullopt;
    }

    // Author
    auto itAuthorName = authorNode->find("name");
    if (itAuthorName == authorNode->end() || !itAuthorName->is_string()) {
        error = "author.name missing or not a string";
        return std::nullopt;
    }
    m.author.name = itAuthorName->get<std::string>();
    auto itAuthorUrl = authorNode->find("url");
    if (itAuthorUrl != authorNode->end() && itAuthorUrl->is_string()) {
        m.author.url = itAuthorUrl->get<std::string>();
    }

    // Optional: homepage
    auto itHome = j.find("homepage");
    if (itHome != j.end() && itHome->is_string()) m.homepage = itHome->get<std::string>();

    // Optional: tags
    auto itTags = j.find("tags");
    if (itTags != j.end() && itTags->is_array()) {
        for (const auto& t : *itTags) {
            if (t.is_string()) m.tags.push_back(t.get<std::string>());
        }
    }

    // Optional: nodes, presets
    auto readStringArray = [](const nlohmann::json* arr, std::vector<std::string>& out) {
        if (!arr || !arr->is_array()) return;
        for (const auto& v : *arr) {
            if (v.is_string()) out.push_back(v.get<std::string>());
        }
    };
    if (auto it = j.find("nodes"); it != j.end())    readStringArray(&*it, m.nodeFiles);
    if (auto it = j.find("presets"); it != j.end())  readStringArray(&*it, m.presetFiles);

    // Optional: resources
    if (auto itRes = j.find("resources"); itRes != j.end() && itRes->is_object()) {
        if (auto it = itRes->find("shaders");   it != itRes->end()) readStringArray(&*it, m.resources.shaders);
        if (auto it = itRes->find("textures");  it != itRes->end()) readStringArray(&*it, m.resources.textures);
        if (auto it = itRes->find("materials"); it != itRes->end()) readStringArray(&*it, m.resources.materials);
        if (auto it = itRes->find("particles"); it != itRes->end()) readStringArray(&*it, m.resources.particles);
    }

    // Optional: dependencies
    if (auto it = j.find("dependencies"); it != j.end()) readStringArray(&*it, m.dependencies);

    // Optional: permissions
    if (auto itPerm = j.find("permissions"); itPerm != j.end() && itPerm->is_array()) {
        for (const auto& p : *itPerm) {
            if (!p.is_string()) continue;
            auto parsed = permissionFromString(p.get<std::string>());
            if (!parsed) {
                error = "unknown permission: " + p.get<std::string>();
                return std::nullopt;
            }
            m.permissions.push_back(*parsed);
        }
    }

    return m;
}

nlohmann::json PluginManifest::toJson() const {
    nlohmann::json j;
    j["id"]           = id;
    j["name"]         = name;
    j["version"]      = version;
    j["bbfx_version"] = bbfxVersion;
    j["author"]       = { {"name", author.name} };
    if (!author.url.empty()) j["author"]["url"] = author.url;
    j["description"]  = description;
    j["category"]     = category;
    if (!tags.empty())        j["tags"]         = tags;
    j["license"]      = license;
    if (!homepage.empty())    j["homepage"]     = homepage;
    j["entry"]        = entry;
    if (!nodeFiles.empty())   j["nodes"]        = nodeFiles;
    if (!presetFiles.empty()) j["presets"]      = presetFiles;

    nlohmann::json res = nlohmann::json::object();
    if (!resources.shaders.empty())   res["shaders"]   = resources.shaders;
    if (!resources.textures.empty())  res["textures"]  = resources.textures;
    if (!resources.materials.empty()) res["materials"] = resources.materials;
    if (!resources.particles.empty()) res["particles"] = resources.particles;
    if (!res.empty()) j["resources"] = res;

    if (!dependencies.empty()) j["dependencies"] = dependencies;

    if (!permissions.empty()) {
        nlohmann::json perms = nlohmann::json::array();
        for (auto p : permissions) perms.push_back(toString(p));
        j["permissions"] = perms;
    }
    return j;
}

std::optional<std::tuple<int,int,int>> PluginManifest::parseSemver(const std::string& v) {
    int major = 0, minor = 0, patch = 0;
    // Strip optional "-prerelease" tail.
    std::string core = v;
    if (auto dash = core.find('-'); dash != std::string::npos) core = core.substr(0, dash);
    // Sscanf is fine for three ints.
    int matched = std::sscanf(core.c_str(), "%d.%d.%d", &major, &minor, &patch);
    if (matched < 2) return std::nullopt;
    if (matched == 2) patch = 0;
    if (major < 0 || minor < 0 || patch < 0) return std::nullopt;
    return std::make_tuple(major, minor, patch);
}

bool PluginManifest::semverSatisfies(const std::string& currentVersion) const {
    auto curOpt = parseSemver(currentVersion);
    if (!curOpt) return false;
    auto [curMaj, curMin, curPat] = *curOpt;

    std::string constraint = bbfxVersion;
    std::string op;
    // Detect operator prefix.
    if (constraint.rfind(">=", 0) == 0) { op = ">="; constraint.erase(0, 2); }
    else if (constraint.rfind("==", 0) == 0) { op = "=="; constraint.erase(0, 2); }
    else if (!constraint.empty() && constraint[0] == '^') { op = "^"; constraint.erase(0, 1); }
    else if (!constraint.empty() && constraint[0] == '~') { op = "~"; constraint.erase(0, 1); }
    else { op = "=="; }

    auto reqOpt = parseSemver(constraint);
    if (!reqOpt) return false;
    auto [reqMaj, reqMin, reqPat] = *reqOpt;

    auto cmp = [&](int cM, int cm, int cp) {
        if (cM != reqMaj) return cM < reqMaj ? -1 : 1;
        if (cm != reqMin) return cm < reqMin ? -1 : 1;
        if (cp != reqPat) return cp < reqPat ? -1 : 1;
        return 0;
    };

    if (op == "==") return cmp(curMaj, curMin, curPat) == 0;
    if (op == ">=") return cmp(curMaj, curMin, curPat) >= 0;
    if (op == "^") {
        // ^MAJOR.MINOR.PATCH : same major, >= required
        if (curMaj != reqMaj) return false;
        return cmp(curMaj, curMin, curPat) >= 0;
    }
    if (op == "~") {
        // ~MAJOR.MINOR.PATCH : same major.minor, patch >= required
        if (curMaj != reqMaj || curMin != reqMin) return false;
        return curPat >= reqPat;
    }
    return false;
}

} // namespace bbfx

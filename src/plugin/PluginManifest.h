#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace bbfx {

enum class PluginPermission {
    AUDIO,
    MIDI,
    OSC,
    ARTNET,
    TEXTURE_SHARE,
    NETWORK,
    FS,
    UI,
    GAMEPAD,
    MODELS,
    SCENE,
    // v3.5 Lot M — sentinel used by the sandbox to mean "no permission
    // required". Never written to a manifest.
    UNKNOWN
};

const char* toString(PluginPermission p);
std::optional<PluginPermission> permissionFromString(const std::string& s);

struct PluginAuthor {
    std::string name;
    std::string url;
};

struct PluginResources {
    std::vector<std::string> shaders;
    std::vector<std::string> textures;
    std::vector<std::string> materials;
    std::vector<std::string> particles;
};

struct PluginManifest {
    std::string id;
    std::string name;
    std::string version;       // semver "1.0.0"
    std::string bbfxVersion;   // ">=3.5"
    PluginAuthor author;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    std::string license;
    std::string homepage;
    std::string entry;         // "init.lua"
    std::vector<std::string> nodeFiles;
    std::vector<std::string> presetFiles;
    PluginResources resources;
    std::vector<std::string> dependencies;
    std::vector<PluginPermission> permissions;

    static std::optional<PluginManifest> fromJson(const nlohmann::json& j, std::string& error);
    nlohmann::json toJson() const;

    // Returns true if this manifest's `bbfx_version` constraint is satisfied
    // by the given current BBFx version ("3.5.0", "3.6.2", ...).
    // Supports operators: `>=`, `==`, `^`, `~`, or bare version (== exact).
    bool semverSatisfies(const std::string& currentVersion) const;

    // Parse a semver "MAJOR.MINOR.PATCH" into three integers.
    // Accepts optional pre-release suffix "-alpha" etc. (ignored).
    // Returns nullopt if malformed.
    static std::optional<std::tuple<int,int,int>> parseSemver(const std::string& v);
};

} // namespace bbfx

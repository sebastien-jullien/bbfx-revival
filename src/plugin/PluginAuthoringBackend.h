#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace bbfx {

/// v3.5 Lot S — programmatic plugin authoring.
///
/// Generates a complete plugin directory from in-memory data :
/// manifest.json, init.lua, README.md (stub), optional thumbnail and
/// copied resources. The caller supplies the plugin id, metadata, a
/// Lua `init.lua` body, and an optional list of resource files to
/// copy.
///
/// The backend is pure filesystem + JSON — no ImGui dependency — so it
/// lives in bbfx-core and is callable from both the Studio
/// PluginAuthoringDialog (Lot S UI) and `dbg.plugin_export_*` /
/// `bbfx.authoring.*` automations.
class PluginAuthoringBackend {
public:
    struct Metadata {
        std::string id;            // kebab-case; validated against [a-z0-9.-]
        std::string name;
        std::string version  = "1.0.0";
        std::string author;
        std::string description;
        std::string license  = "MIT";
        std::string category = "Custom";       // Node, Preset, Scene, OutputTemplate, Custom
        std::string bbfxVersion = ">=3.5";
        std::vector<std::string> tags;
        std::vector<std::string> permissions;  // "midi" / "fs" / ...
    };

    /// Write a full plugin directory under `destRoot/<id>/`. Returns the
    /// absolute path of the generated plugin directory on success, or
    /// an empty path on failure. Safe to call repeatedly — existing
    /// content is overwritten.
    ///
    /// `initLuaBody` is the Lua source to write as the plugin's init.lua
    /// entry point. `extraFiles` is a map of `relative_path_in_plugin ->
    /// absolute_source_path` : every file is copied into the plugin dir.
    static std::filesystem::path writePlugin(
        const std::filesystem::path& destRoot,
        const Metadata& meta,
        const std::string& initLuaBody,
        const std::vector<std::pair<std::string, std::string>>& extraFiles);

    /// Export the current Studio scene as a Scene-Preset plugin.
    /// `sceneJson` is the captured scene config (produced by
    /// StudioApp's save path). Returns the generated plugin dir.
    static std::filesystem::path exportScenePreset(
        const Metadata& meta, const nlohmann::json& sceneJson);

    /// Export the current OutputManager config as an Output-Template
    /// plugin. `outputJson` comes from OutputManager::toJson().
    static std::filesystem::path exportOutputTemplate(
        const Metadata& meta, const nlohmann::json& outputJson);

    /// Export a node subgraph. `subgraphJson` is the serialized
    /// subgraph (node specs + links). The init.lua instantiates the
    /// graph at load time.
    static std::filesystem::path exportSubgraph(
        const Metadata& meta, const nlohmann::json& subgraphJson);

    /// Slugify a human name into a plugin id-compatible token
    /// (lowercase, digits, '-', '.' only).
    static std::string slugify(const std::string& name);

    /// Validate that `id` matches the plugin-id pattern.
    static bool isValidId(const std::string& id);

    /// Heuristic scan of a Lua source body — returns the list of bbfx
    /// namespaces referenced (used to auto-fill permissions).
    static std::vector<std::string> detectPermissions(const std::string& luaSource);
};

} // namespace bbfx

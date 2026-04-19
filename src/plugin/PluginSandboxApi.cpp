#include "plugin/PluginSandboxApi.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <sol/sol.hpp>

#include <OgreMaterialManager.h>
#include <OgreParticleSystemManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreTextureManager.h>

#include "core/Animator.h"
#include "core/PrimitiveNodes.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginRegistry.h"
#include "plugin/PluginSandbox.h"
#include "studio/NodeTypeRegistry.h"

namespace fs = std::filesystem;

namespace bbfx {

namespace {

// Resolve a user-supplied relative path (from Lua) against the plugin dir, and
// refuse anything that escapes it. Returns the canonical absolute path on
// success, or an empty path on failure (caller reports the violation).
fs::path resolveInsidePlugin(const std::string& rel, const fs::path& pluginDir,
                             const std::string& pluginId, const char* what) {
    if (rel.empty()) {
        PluginManager::instance().onSandboxViolation(
            pluginId, std::string(what) + ": empty path");
        return {};
    }
    fs::path p(rel);
    fs::path resolved = p.is_absolute() ? p : (pluginDir / p);
    if (!PluginSandbox::canAccess(resolved, pluginDir)) {
        PluginManager::instance().onSandboxViolation(
            pluginId, std::string(what) + " denied (outside plugin dir): " + rel);
        return {};
    }
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(resolved, ec);
    if (ec) {
        PluginManager::instance().onSandboxViolation(
            pluginId, std::string(what) + ": cannot canonicalize " + rel);
        return {};
    }
    if (!fs::exists(canon, ec) || !fs::is_regular_file(canon, ec)) {
        PluginManager::instance().onSandboxViolation(
            pluginId, std::string(what) + ": file not found — " + rel);
        return {};
    }
    return canon;
}

// Build an OGRE resource name from a plugin-relative path.
// Format: "<pluginId>/<relPath>" — stable across sessions, unique per file.
std::string ogreName(const std::string& pluginId, const std::string& rel) {
    return pluginId + "/" + rel;
}

// Ensure the plugin's OGRE resource group exists and has the plugin dir
// registered as a filesystem location. Idempotent.
void ensureResourceGroup(const PluginInfo& info) {
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    const std::string& group = info.resourceGroupName;
    if (!rgm.resourceGroupExists(group)) {
        rgm.createResourceGroup(group, /*inGlobalPool=*/false);
    }
    // addResourceLocation is idempotent in practice (OGRE will no-op if the
    // same path is added twice) but we guard anyway by checking locations.
    bool already = false;
    try {
        const auto locs = rgm.listResourceLocations(group);
        for (const auto& l : *locs) {
            if (l == info.directoryPath.string()) { already = true; break; }
        }
    } catch (const std::exception&) {
        // Some OGRE builds throw if the group is fresh and never initialised.
    }
    if (!already) {
        try {
            rgm.addResourceLocation(info.directoryPath.string(), "FileSystem",
                                    group, /*recursive=*/true);
        } catch (const std::exception& e) {
            std::cerr << "[PluginSandboxApi] addResourceLocation failed for "
                      << info.id << ": " << e.what() << std::endl;
        }
    }
    if (!rgm.isResourceGroupInitialised(group)) {
        try {
            rgm.initialiseResourceGroup(group);
        } catch (const std::exception& e) {
            std::cerr << "[PluginSandboxApi] initialiseResourceGroup failed for "
                      << info.id << ": " << e.what() << std::endl;
        }
    }
}

// --- registerNodeType implementation ---------------------------------------
//
// The plugin declares a spec table:
//   {
//     inputs  = { "a", "b", ... },
//     outputs = { "out", ... },
//     process = function(node) ... end,
//     category = "Custom",
//     color   = {r, g, b, a},   -- optional
//   }
//
// We synthesize a NodeTypeRegistry entry whose factory produces a
// LuaAnimationNode wired to `process`. The full id is `<pluginId>.<typeId>`
// so plugin-provided node types never clash with builtins.
void registerNodeTypeImpl(sol::state& lua, const PluginInfo& info,
                          const std::string& typeId, sol::table spec) {
    std::string fullId = info.id + "." + typeId;

    if (NodeTypeRegistry::instance().getType(fullId) != nullptr) {
        PluginManager::instance().onSandboxViolation(
            info.id, "registerNodeType: type already registered — " + fullId);
        return;
    }

    // Extract inputs / outputs names to capture (so the factory doesn't need
    // to access the sol::table later from an arbitrary thread).
    std::vector<std::string> inputs, outputs;
    sol::optional<sol::table> inTbl  = spec["inputs"];
    sol::optional<sol::table> outTbl = spec["outputs"];
    if (inTbl) {
        for (auto& kv : *inTbl) {
            if (kv.second.is<std::string>()) inputs.push_back(kv.second.as<std::string>());
        }
    }
    if (outTbl) {
        for (auto& kv : *outTbl) {
            if (kv.second.is<std::string>()) outputs.push_back(kv.second.as<std::string>());
        }
    }

    sol::function process;
    sol::object processObj = spec["process"];
    if (processObj.is<sol::function>()) process = processObj.as<sol::function>();

    std::string category = "Community/" + info.manifest.name;
    sol::optional<std::string> catOpt = spec["category"];
    if (catOpt) category = "Community/" + *catOpt;

    ImVec4 color(0.5f, 0.4f, 0.9f, 1.0f); // default lilac for plugin types
    sol::optional<sol::table> colorTbl = spec["color"];
    if (colorTbl) {
        color.x = (*colorTbl)[1].get_or(color.x);
        color.y = (*colorTbl)[2].get_or(color.y);
        color.z = (*colorTbl)[3].get_or(color.z);
        color.w = (*colorTbl)[4].get_or(color.w);
    }

    NodeTypeInfo ti;
    ti.typeName = fullId;
    ti.category = category;
    ti.color    = color;
    ti.factory  = [process, inputs, outputs](const std::string& name,
                                             sol::state& st) -> AnimationNode* {
        sol::function update = process;
        if (!update.valid()) {
            st.script("function _bbfx_noop(self) end", sol::script_pass_on_error);
            update = st["_bbfx_noop"];
        }
        auto* node = new LuaAnimationNode(name, update);
        for (const auto& in  : inputs)  node->addInput(in);
        for (const auto& out : outputs) node->addOutput(out);
        if (auto* animator = Animator::instance()) {
            for (auto& [pname, port] : node->getInputs())  animator->add(port);
            for (auto& [pname, port] : node->getOutputs()) animator->add(port);
            node->setListener(animator);
            animator->registerNode(node);
        }
        return node;
    };

    NodeTypeRegistry::instance().registerTypeFromPlugin(info.id, ti);

    auto* mut = PluginManager::instance().getPluginMutable(info.id);
    if (mut) mut->registeredNodeTypes.push_back(fullId);

    std::cout << "[plugin] " << info.id << " registered node type '" << fullId
              << "' (" << inputs.size() << " inputs, " << outputs.size()
              << " outputs)" << std::endl;
}

} // anonymous namespace

void installSandboxPluginApi(sol::state& lua, sol::environment& env,
                             const PluginInfo& info) {
    // `bbfx` is itself a table inside the sandbox; start from the global one
    // (which already has the user-facing `bbfx.*` bindings) but shadow the
    // `plugin` sub-table with the authoring API so registerNodeType etc. live
    // only inside the sandbox.
    sol::table bbfxTbl = lua["bbfx"];
    env["bbfx"] = bbfxTbl;

    sol::table pluginTbl = lua.create_table();

    // Copy the user-facing introspection calls (list/info/...) so plugins can
    // discover their siblings without duplicate bindings.
    pluginTbl["list"]                = bbfxTbl["plugin"]["list"];
    pluginTbl["info"]                = bbfxTbl["plugin"]["info"];
    pluginTbl["currentBBFxVersion"]  = bbfxTbl["plugin"]["currentBBFxVersion"];

    // --- authoring ---------------------------------------------------------
    const std::string pluginId        = info.id;
    const fs::path    pluginDir       = info.directoryPath;
    const std::string pluginVersion   = info.manifest.version;
    const std::string resourceGroup   = info.resourceGroupName;

    pluginTbl["getId"]             = [pluginId]()        -> std::string { return pluginId; };
    pluginTbl["getVersion"]        = [pluginVersion]()   -> std::string { return pluginVersion; };
    pluginTbl["getDir"]            = [pluginDir]()       -> std::string { return pluginDir.string(); };
    pluginTbl["getResourceGroup"]  = [resourceGroup]()   -> std::string { return resourceGroup; };

    pluginTbl["registerNodeType"] = [&lua, info](const std::string& typeId, sol::table spec) {
        registerNodeTypeImpl(lua, info, typeId, spec);
    };

    pluginTbl["registerPreset"] = [&lua, info](const std::string& presetId, sol::table preset) {
        std::string fullId = info.id + "." + presetId;
        // For Lot B we track the preset in the registry so Lot C can wire
        // PresetBrowser integration. The preset table itself is kept in the
        // plugin's Lua state (it's a capture on the closure).
        PluginRegistry::instance().trackPreset(info.id, fullId);
        auto* mut = PluginManager::instance().getPluginMutable(info.id);
        if (mut) mut->registeredPresets.push_back(fullId);
        std::cout << "[plugin] " << info.id << " registered preset '" << fullId << "'"
                  << std::endl;
        return preset;  // returned verbatim so the plugin can keep a reference
    };

    pluginTbl["loadShader"] = [info](const std::string& rel) -> sol::optional<std::string> {
        fs::path canon = resolveInsidePlugin(rel, info.directoryPath, info.id, "loadShader");
        if (canon.empty()) return sol::nullopt;
        ensureResourceGroup(info);
        // Shaders are loaded lazily by OGRE when a material references them;
        // we merely register the file as part of the group via the resource
        // location already added. Return the filename so the plugin can
        // reference it inside a material script or ShaderFxNode.
        return canon.filename().string();
    };

    pluginTbl["loadTexture"] = [info](const std::string& rel) -> sol::optional<std::string> {
        fs::path canon = resolveInsidePlugin(rel, info.directoryPath, info.id, "loadTexture");
        if (canon.empty()) return sol::nullopt;
        ensureResourceGroup(info);
        std::string name = ogreName(info.id, rel);
        try {
            auto& tm = Ogre::TextureManager::getSingleton();
            if (tm.getByName(name, info.resourceGroupName).get() == nullptr) {
                tm.load(canon.filename().string(), info.resourceGroupName);
            }
        } catch (const std::exception& e) {
            PluginManager::instance().onSandboxViolation(
                info.id, std::string("loadTexture failed: ") + e.what());
            return sol::nullopt;
        }
        return canon.filename().string();
    };

    pluginTbl["loadMaterial"] = [info](const std::string& rel) -> sol::optional<std::string> {
        fs::path canon = resolveInsidePlugin(rel, info.directoryPath, info.id, "loadMaterial");
        if (canon.empty()) return sol::nullopt;
        ensureResourceGroup(info);
        try {
            // OGRE auto-parses .material files at initialiseResourceGroup
            // time (triggered by ensureResourceGroup). For plugins loaded
            // after the group was first initialised we'd need a reparse;
            // that refinement lands with the full Lot C integration.
        } catch (const std::exception& e) {
            PluginManager::instance().onSandboxViolation(
                info.id, std::string("loadMaterial failed: ") + e.what());
            return sol::nullopt;
        }
        return canon.filename().string();
    };

    pluginTbl["loadParticleTemplate"] = [info](const std::string& rel) -> sol::optional<std::string> {
        fs::path canon = resolveInsidePlugin(rel, info.directoryPath, info.id, "loadParticleTemplate");
        if (canon.empty()) return sol::nullopt;
        ensureResourceGroup(info);
        return canon.filename().string();
    };

    // v3.5 Lot M — permission-gated namespaces. If the plugin's manifest
    // didn't grant the corresponding permission, we skip copying that
    // namespace into the sandbox env — the plugin's `bbfx.midi` etc. will
    // be sol::nil. Namespaces not listed here are considered safe and are
    // always copied (math, table, bbfx.noise, bbfx.plugin introspection...).
    auto needsPermission = [](const std::string& key) -> PluginPermission {
        if (key == "midi")           return PluginPermission::MIDI;
        if (key == "osc")            return PluginPermission::OSC;
        if (key == "artnet")         return PluginPermission::ARTNET;
        if (key == "textureShare")   return PluginPermission::TEXTURE_SHARE;
        if (key == "gamepad")        return PluginPermission::GAMEPAD;
        if (key == "joystick")       return PluginPermission::GAMEPAD;
        if (key == "gamepadMapping") return PluginPermission::GAMEPAD;
        if (key == "http")           return PluginPermission::NETWORK;
        if (key == "websocket")      return PluginPermission::NETWORK;
        if (key == "audio")          return PluginPermission::AUDIO;
        if (key == "fs")             return PluginPermission::FS;
        if (key == "ui")             return PluginPermission::UI;
        if (key == "scene")          return PluginPermission::SCENE;
        if (key == "models")         return PluginPermission::MODELS;
        // v3.5 Lot Q — bbfx.media / bbfx.images / bbfx.sequences are
        // explicitly "no permission required" per the iteration spec:
        // they operate on paths already under the user's control (the
        // plugin author passes them in), and the underlying OGRE
        // TextureManager is already sandboxed by OGRE's resource group.
        return PluginPermission::UNKNOWN;  // no gating needed
    };
    auto hasPermission = [&info](PluginPermission p) -> bool {
        for (auto have : info.manifest.permissions) {
            if (have == p) return true;
        }
        return false;
    };

    // Stash the sandbox-specific plugin table under `bbfx.plugin` in the env.
    // We deliberately do NOT mutate the global `bbfx.plugin` table — the
    // authoring API must only be visible inside the sandbox.
    sol::table bbfxInEnv = env["bbfx"];
    // Shadow: create a per-env copy that mirrors the global table plus our
    // sandbox plugin table.
    sol::table bbfxShadow = lua.create_table();
    bbfxTbl.for_each([&](sol::object k, sol::object v) {
        if (!k.is<std::string>()) {
            bbfxShadow[k] = v;
            return;
        }
        std::string key = k.as<std::string>();
        if (key == "plugin") return;  // shadowed below
        auto needed = needsPermission(key);
        if (needed != PluginPermission::UNKNOWN && !hasPermission(needed)) {
            // Permission not granted — omit this namespace entirely from
            // the plugin's view of bbfx. The plugin will see nil on
            // access and can handle gracefully.
            return;
        }
        bbfxShadow[key] = v;
    });
    bbfxShadow["plugin"] = pluginTbl;

    // ── v3.5 Lot O — sandbox-scoped bbfx.fs + bbfx.http.download ─────────
    // When the plugin has the `fs` permission, replace bbfx.fs with a
    // plugin-dir-confined wrapper. All paths are canonicalized and checked
    // against info.directoryPath; anything escaping = sandbox violation.
    if (hasPermission(PluginPermission::FS)) {
        sol::table fsWrap = lua.create_table();
        const std::string pid = info.id;
        const fs::path    pdir = info.directoryPath;

        fsWrap["readFile"] = [pid, pdir, &lua](const std::string& rel) -> sol::object {
            fs::path canon = resolveInsidePlugin(rel, pdir, pid, "fs.readFile");
            if (canon.empty()) return sol::make_object(lua, sol::nil);
            std::ifstream f(canon, std::ios::binary);
            if (!f) return sol::make_object(lua, sol::nil);
            std::string body((std::istreambuf_iterator<char>(f)), {});
            return sol::make_object(lua, body);
        };
        fsWrap["writeFile"] = [pid, pdir](const std::string& rel,
                                             const std::string& content) -> bool {
            // writeFile can create new files — skip the strict exists check
            // that resolveInsidePlugin enforces. Just ensure path stays
            // under the plugin directory.
            if (rel.empty()) {
                PluginManager::instance().onSandboxViolation(pid, "fs.writeFile: empty path");
                return false;
            }
            fs::path p(rel);
            fs::path resolved = p.is_absolute() ? p : (pdir / p);
            if (!PluginSandbox::canAccess(resolved, pdir)) {
                PluginManager::instance().onSandboxViolation(
                    pid, "fs.writeFile denied (outside plugin dir): " + rel);
                return false;
            }
            std::error_code ec;
            fs::create_directories(resolved.parent_path(), ec);
            std::ofstream f(resolved, std::ios::binary);
            if (!f) return false;
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
            return static_cast<bool>(f);
        };
        fsWrap["readLines"] = [pid, pdir, &lua](const std::string& rel) -> sol::table {
            sol::table out = lua.create_table();
            fs::path canon = resolveInsidePlugin(rel, pdir, pid, "fs.readLines");
            if (canon.empty()) return out;
            std::ifstream f(canon);
            std::string line;
            int i = 1;
            while (std::getline(f, line)) out[i++] = line;
            return out;
        };
        fsWrap["exists"] = [pid, pdir](const std::string& rel) -> bool {
            if (rel.empty()) return false;
            fs::path p(rel);
            fs::path resolved = p.is_absolute() ? p : (pdir / p);
            if (!PluginSandbox::canAccess(resolved, pdir)) return false;
            std::error_code ec;
            return fs::exists(resolved, ec);
        };
        fsWrap["listDir"] = [pid, pdir, &lua](const std::string& rel) -> sol::table {
            sol::table out = lua.create_table();
            if (rel.empty()) return out;
            fs::path p(rel);
            fs::path resolved = p.is_absolute() ? p : (pdir / p);
            if (!PluginSandbox::canAccess(resolved, pdir)) {
                PluginManager::instance().onSandboxViolation(
                    pid, "fs.listDir denied (outside plugin dir): " + rel);
                return out;
            }
            std::error_code ec;
            if (!fs::is_directory(resolved, ec)) return out;
            int i = 1;
            for (auto& e : fs::directory_iterator(resolved, ec)) {
                out[i++] = e.path().filename().string();
            }
            return out;
        };
        bbfxShadow["fs"] = fsWrap;
    }

    // bbfx.http.download — when accessible (plugin has `network`), force
    // destPath under the plugin directory. We wrap the one call that
    // writes to disk; get/getSync/post stay as-is since they only hit
    // the network.
    if (hasPermission(PluginPermission::NETWORK)) {
        sol::table httpOrig = bbfxTbl["http"];
        sol::table httpWrap = lua.create_table();
        httpWrap["get"]        = httpOrig["get"];
        httpWrap["getSync"]    = httpOrig["getSync"];
        httpWrap["post"]       = httpOrig["post"];
        httpWrap["sha256File"] = httpOrig["sha256File"];
        httpWrap["pump"]       = httpOrig["pump"];
        httpWrap["waitIdle"]   = httpOrig["waitIdle"];
        const std::string pid = info.id;
        const fs::path    pdir = info.directoryPath;
        sol::function origDownload = httpOrig["download"];
        httpWrap["download"] = [pid, pdir, origDownload](
                                    const std::string& url,
                                    const std::string& destPath,
                                    sol::table opts) mutable {
            if (destPath.empty()) {
                PluginManager::instance().onSandboxViolation(
                    pid, "http.download: empty destPath");
                return;
            }
            fs::path p(destPath);
            fs::path resolved = p.is_absolute() ? p : (pdir / p);
            if (!PluginSandbox::canAccess(resolved, pdir)) {
                PluginManager::instance().onSandboxViolation(
                    pid, "http.download denied (outside plugin dir): " + destPath);
                return;
            }
            std::error_code ec;
            fs::create_directories(resolved.parent_path(), ec);
            origDownload(url, resolved.string(), opts);
        };
        bbfxShadow["http"] = httpWrap;
    }

    env["bbfx"] = bbfxShadow;
}

} // namespace bbfx

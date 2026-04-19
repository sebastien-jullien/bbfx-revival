#pragma once

#include <filesystem>
#include <string>

#include <sol/forward.hpp>

namespace bbfx {

struct PluginInfo;

/// Builds a restricted `sol::environment` for a plugin.
///
/// The sandbox:
///  - Whitelists `math`, `string` (minus `string.dump`), `table`, `coroutine`
///  - Exposes a minimal `os` stub with only `clock`, `time`, `date(fmt, t)`,
///    `difftime`, and `getenv` mapped to `nil` — no `execute/remove/rename/exit`
///  - Does NOT expose `io`, `debug`, or `package`
///  - Replaces `require` with a whitelist-only loader (no native .so/.dll)
///  - Replaces `loadfile` and `dofile` with variants that refuse paths outside
///    the plugin directory (canonical-path check)
///  - Replaces `load` / `loadstring` with variants that always execute in the
///    plugin environment (cannot escape to globals)
///  - Pre-installs `bbfx.plugin.*` as the sandbox-facing plugin API
///    (registerNodeType, registerPreset, loadShader, loadTexture,
///     loadMaterial, loadParticleTemplate, getId, getVersion, getDir,
///     getResourceGroup)
///
/// Every call that violates a rule funnels through
/// `PluginManager::onSandboxViolation(pluginId, detail)`.
class PluginSandbox {
public:
    /// Build a fresh sandbox environment for `info`. The returned environment
    /// is typically held inside the caller's PluginInfo. Calling this again
    /// produces a new, independent environment.
    static sol::environment create(sol::state& lua, const PluginInfo& info);

    /// Returns true iff `requestedPath` resolves to a file that lives under
    /// `pluginDir` (both paths are canonicalized with `weakly_canonical`).
    /// Used by the bbfx.fs.* bindings in Lot O and by loadfile/dofile here.
    static bool canAccess(const std::filesystem::path& requestedPath,
                          const std::filesystem::path& pluginDir);

    /// The (hard-coded) list of stdlib module names that a plugin may
    /// `require` from inside the sandbox. Exposed for tests.
    static bool isRequireAllowed(const std::string& moduleName);
};

} // namespace bbfx

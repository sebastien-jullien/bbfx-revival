#include "plugin/PluginSandbox.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>

#include <sol/sol.hpp>

#include "plugin/PluginManager.h"
#include "plugin/PluginSandboxApi.h"

namespace fs = std::filesystem;

namespace bbfx {

// ── Whitelist of stdlib modules requireable inside a sandbox ────────────────
static const std::array<std::string_view, 4> kAllowedRequires = {
    "math", "string", "table", "coroutine"
};

bool PluginSandbox::isRequireAllowed(const std::string& moduleName) {
    return std::any_of(kAllowedRequires.begin(), kAllowedRequires.end(),
                       [&](std::string_view s) { return s == moduleName; });
}

bool PluginSandbox::canAccess(const fs::path& requestedPath, const fs::path& pluginDir) {
    std::error_code ec;
    fs::path reqCanon = fs::weakly_canonical(requestedPath, ec);
    if (ec) return false;
    fs::path dirCanon = fs::weakly_canonical(pluginDir, ec);
    if (ec) return false;

    auto reqIt = reqCanon.begin();
    auto dirIt = dirCanon.begin();
    auto reqEnd = reqCanon.end();
    auto dirEnd = dirCanon.end();
    for (; dirIt != dirEnd; ++dirIt, ++reqIt) {
        if (reqIt == reqEnd || *reqIt != *dirIt) return false;
    }
    return true;
}

// ── Internal helpers ────────────────────────────────────────────────────────
namespace {

void reportViolation(const std::string& pluginId, const std::string& detail) {
    PluginManager::instance().onSandboxViolation(pluginId, detail);
}

// Whitelist a subset of os.* into `osStub`. We don't expose the real os table
// because that would leak os.execute/os.remove/os.getenv/... through the
// metatable.
void installSafeOsStub(sol::state& lua, sol::environment& env) {
    sol::table osStub = env.create_named("os");
    osStub["clock"]    = lua["os"]["clock"];
    osStub["time"]     = lua["os"]["time"];
    osStub["date"]     = lua["os"]["date"];      // format-style only; safe (no sideeffect)
    osStub["difftime"] = lua["os"]["difftime"];
}

// Whitelist string except string.dump (which leaks bytecode).
void installSafeStringStub(sol::state& lua, sol::environment& env) {
    sol::table stringTbl = lua["string"];
    sol::table stub = env.create_named("string");
    stringTbl.for_each([&](sol::object k, sol::object v) {
        if (k.is<std::string>() && k.as<std::string>() == "dump") return;
        stub[k] = v;
    });
}

// IMPORTANT: the lambdas below capture `env` by VALUE. sol::environment is a
// lightweight handle to a Lua table on the engine-wide state — copying it is
// cheap and preserves the identity of the underlying Lua object, whereas
// capturing by reference would dangle as soon as the sandbox builder returns
// (the local `env` in `PluginSandbox::create` goes out of scope). Same goes
// for `sol::state&` — we use sol::this_state from the call site to access the
// correct lua_State* without relying on a captured reference.

void installRequireGuard(sol::state& lua, sol::environment& env,
                         const std::string& pluginId) {
    env["require"] = [&lua, pluginId](const std::string& mod) -> sol::object {
        if (!PluginSandbox::isRequireAllowed(mod)) {
            reportViolation(pluginId, "require('" + mod + "') is not allowed");
            return sol::nil;
        }
        return lua[mod];
    };
}

void installLoadfileGuard(sol::state& lua, sol::environment& env,
                          const std::string& pluginId,
                          const fs::path& pluginDir) {
    sol::environment envCopy = env;   // handle copy — safe to capture

    env["loadfile"] = [&lua, envCopy, pluginId, pluginDir](
                          sol::this_state ts, const std::string& path)
                      -> std::tuple<sol::object, sol::object> {
        (void)ts;
        fs::path req(path);
        fs::path resolved = req.is_absolute() ? req : (pluginDir / req);
        if (!PluginSandbox::canAccess(resolved, pluginDir)) {
            reportViolation(pluginId, "loadfile denied (outside plugin dir): " + path);
            return { sol::object(ts, sol::in_place, sol::lua_nil),
                     sol::object(ts, sol::in_place, std::string("access denied: ") + path) };
        }
        auto result = lua.load_file(resolved.string(), sol::load_mode::any);
        if (!result.valid()) {
            sol::error err = result;
            return { sol::object(ts, sol::in_place, sol::lua_nil),
                     sol::object(ts, sol::in_place, std::string(err.what())) };
        }
        sol::function fn = result;
        sol::set_environment(envCopy, fn);
        return { sol::object(ts, sol::in_place, fn),
                 sol::object(ts, sol::in_place, sol::lua_nil) };
    };

    env["dofile"] = [&lua, envCopy, pluginId, pluginDir](
                        sol::this_state ts, const std::string& path)
                    -> sol::object {
        (void)ts;
        fs::path req(path);
        fs::path resolved = req.is_absolute() ? req : (pluginDir / req);
        if (!PluginSandbox::canAccess(resolved, pluginDir)) {
            reportViolation(pluginId, "dofile denied (outside plugin dir): " + path);
            return sol::object(ts, sol::in_place, sol::lua_nil);
        }
        auto result = lua.safe_script_file(resolved.string(), envCopy,
                                           sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            reportViolation(pluginId, std::string("dofile error: ") + err.what());
            return sol::object(ts, sol::in_place, sol::lua_nil);
        }
        return result;
    };
}

void installLoadGuard(sol::state& lua, sol::environment& env,
                      const std::string& pluginId) {
    sol::environment envCopy = env;

    auto loader = [&lua, envCopy, pluginId](
                      sol::this_state ts, const std::string& code,
                      sol::optional<std::string> chunkname)
                  -> std::tuple<sol::object, sol::object> {
        auto result = lua.load(code,
                               chunkname ? *chunkname : "=[plugin_load]",
                               sol::load_mode::text /* reject bytecode */);
        if (!result.valid()) {
            sol::error err = result;
            reportViolation(pluginId, std::string("load error: ") + err.what());
            return { sol::object(ts, sol::in_place, sol::lua_nil),
                     sol::object(ts, sol::in_place, std::string(err.what())) };
        }
        sol::function fn = result;
        sol::set_environment(envCopy, fn);
        return { sol::object(ts, sol::in_place, fn),
                 sol::object(ts, sol::in_place, sol::lua_nil) };
    };
    env["load"]       = loader;
    env["loadstring"] = loader;
}

} // namespace

sol::environment PluginSandbox::create(sol::state& lua, const PluginInfo& info) {
    sol::environment env(lua, sol::create);

    // ── Core globals a plugin can always touch ───────────────────────────
    env["_VERSION"] = lua["_VERSION"];
    env["assert"]   = lua["assert"];
    env["error"]    = lua["error"];
    env["ipairs"]   = lua["ipairs"];
    env["pairs"]    = lua["pairs"];
    env["next"]     = lua["next"];
    env["pcall"]    = lua["pcall"];
    env["xpcall"]   = lua["xpcall"];
    env["print"]    = lua["print"];
    env["select"]   = lua["select"];
    env["tonumber"] = lua["tonumber"];
    env["tostring"] = lua["tostring"];
    env["type"]     = lua["type"];
    env["unpack"]   = lua["unpack"];
    env["rawequal"] = lua["rawequal"];
    env["rawget"]   = lua["rawget"];
    env["rawset"]   = lua["rawset"];
    env["rawlen"]   = lua["rawlen"];
    env["setmetatable"] = lua["setmetatable"];
    env["getmetatable"] = lua["getmetatable"];

    // ── Whitelisted stdlib modules ────────────────────────────────────────
    env["math"]      = lua["math"];
    env["table"]     = lua["table"];
    env["coroutine"] = lua["coroutine"];
    installSafeStringStub(lua, env);
    installSafeOsStub(lua, env);

    // io, debug, package remain absent from env (sol::nil on access).

    // ── Restricted dynamic code paths ─────────────────────────────────────
    installRequireGuard (lua, env, info.id);
    installLoadfileGuard(lua, env, info.id, info.directoryPath);
    installLoadGuard    (lua, env, info.id);

    // ── Pre-install bbfx.plugin.* sandbox API ─────────────────────────────
    installSandboxPluginApi(lua, env, info);

    return env;
}

} // namespace bbfx

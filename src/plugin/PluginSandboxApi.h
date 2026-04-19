#pragma once

#include <sol/forward.hpp>

namespace bbfx {

struct PluginInfo;

/// Installs the sandbox-facing `bbfx.plugin.*` table into a plugin's
/// environment. This is distinct from the user-facing `bbfx.plugin.*`
/// exposed globally in `bbfx_bindings.cpp` (which lets a REPL or test script
/// list/scan/validate installed plugins). Here we expose the *authoring* API:
///
///   - bbfx.plugin.registerNodeType(typeId, { inputs, outputs, process,
///                                             category?, color? })
///   - bbfx.plugin.registerPreset(presetId, presetTable)
///   - bbfx.plugin.loadShader(relPath)
///   - bbfx.plugin.loadTexture(relPath)
///   - bbfx.plugin.loadMaterial(relPath)
///   - bbfx.plugin.loadParticleTemplate(relPath)
///   - bbfx.plugin.getId()
///   - bbfx.plugin.getVersion()
///   - bbfx.plugin.getDir()
///   - bbfx.plugin.getResourceGroup()
///
/// All `load*` functions refuse paths outside the plugin directory.
/// `registerNodeType` tags the resulting node type in `NodeTypeRegistry`
/// with the plugin id so that it can be cleanly torn down at disable time.
///
/// Called exclusively from `PluginSandbox::create`.
void installSandboxPluginApi(sol::state& lua, sol::environment& env,
                             const PluginInfo& info);

} // namespace bbfx

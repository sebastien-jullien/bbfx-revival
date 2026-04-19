#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "plugin/PluginManifest.h"

namespace bbfx {

class PluginValidator {
public:
    struct Result {
        bool ok = true;
        std::vector<std::string> errors;
    };

    // Validate a plugin directory. The manifest is expected to have been
    // successfully parsed beforehand (use PluginManifest::fromJson first).
    // This checks:
    //   - bbfx_version constraint against BBFX_CURRENT_VERSION
    //   - entry file exists
    //   - declared resource files exist (shaders/textures/materials/particles)
    //   - declared nodes[] / presets[] files exist
    static Result validate(const std::filesystem::path& pluginDir,
                           const PluginManifest& manifest);

    // Validate just from a path on disk: reads manifest.json, parses, then
    // runs the full validation. Convenient for the `--validate-plugin` CLI flag.
    static Result validatePath(const std::filesystem::path& pluginDir);

    // The BBFx version that plugins' bbfx_version constraint is evaluated
    // against. Exposed so tests and the CLI can reuse the same source of truth.
    static const char* currentBBFxVersion();
};

} // namespace bbfx

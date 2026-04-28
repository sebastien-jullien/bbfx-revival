#pragma once

#include <string>
#include <map>
#include <vector>

namespace bbfx {

/// A single post-process effect in the PostProcessStack.
/// Wraps an existing OGRE material (e.g. "BBFx/Vignette") with its runtime parameters.
struct PostProcessEffect {
    std::string name;                       ///< Display name (e.g. "Vignette")
    std::string materialName;               ///< OGRE material name (e.g. "BBFx/Vignette")
    std::string fragShader;                 ///< Fragment shader filename (e.g. "vignette.frag")
    std::map<std::string, float> params;    ///< Uniform name → value overrides
    bool enabled = true;
    bool needsPrevFrame = false;            ///< True if shader uses prevFrame feedback
    int order = 0;                          ///< Sort key for chaining order

    /// Parse uniform float declarations from a fragment shader source string.
    /// Returns parameter names (excluding tex0 and time which are auto-bound).
    static std::vector<std::string> parseUniforms(const std::string& fragSource);

    /// Get sorted list of parameter names for this effect.
    std::vector<std::string> getParamNames() const;
};

/// Catalogue entry describing an available post-process effect.
struct PostProcessCatalogueEntry {
    std::string name;           ///< Display name (e.g. "Bloom")
    std::string materialName;   ///< OGRE material name (e.g. "BBFx/Bloom")
};

/// Returns the complete list of available BBFx post-process effects.
std::vector<PostProcessCatalogueEntry> getAvailableEffects();

} // namespace bbfx

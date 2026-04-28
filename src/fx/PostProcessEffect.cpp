#include "PostProcessEffect.h"
#include <regex>
#include <algorithm>

namespace bbfx {

std::vector<std::string> PostProcessEffect::parseUniforms(const std::string& fragSource) {
    std::vector<std::string> result;
    // Match lines like: uniform float strength;
    // Exclude tex0, time (auto-bound by the stack)
    static const std::regex re(R"(uniform\s+float\s+(\w+)\s*;)");
    auto begin = std::sregex_iterator(fragSource.begin(), fragSource.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string name = (*it)[1].str();
        if (name != "tex0" && name != "time") {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> PostProcessEffect::getParamNames() const {
    std::vector<std::string> names;
    names.reserve(params.size());
    for (auto& [k, v] : params) {
        names.push_back(k);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<PostProcessCatalogueEntry> getAvailableEffects() {
    return {
        {"Vignette",            "BBFx/Vignette"},
        {"FilmGrain",           "BBFx/FilmGrain"},
        {"Invert",              "BBFx/Invert"},
        {"Posterize",           "BBFx/Posterize"},
        {"EdgeDetect",          "BBFx/EdgeDetect"},
        {"Pixelate",            "BBFx/Pixelate"},
        {"Barrel",              "BBFx/Barrel"},
        {"Kaleidoscope",        "BBFx/Kaleidoscope"},
        {"ChromaticAberration", "BBFx/ChromaticAberration"},
        {"VHS",                 "BBFx/VHS"},
        {"HeatDistort",         "BBFx/HeatDistort"},
        {"ASCII",               "BBFx/ASCII"},
        {"MotionTrail",         "BBFx/MotionTrail"},
        {"EdgeBlend",           "BBFx/EdgeBlend"},
        {"QuadWarp",            "BBFx/QuadWarp"},
        {"GridWarp",            "BBFx/GridWarp"},
        {"Bloom",               "BBFx/Bloom"},
        {"DOF",                 "BBFx/DOF"},
        {"BlackAndWhite",       "BBFx/BlackAndWhite"},
        {"Embossed",            "BBFx/Embossed"},
        {"OldTV",               "BBFx/OldTV"},
        {"Glass",               "BBFx/Glass"},
        {"Halftone",            "BBFx/Halftone"},
        {"CrossHatch",          "BBFx/CrossHatch"},
        {"OilPaint",            "BBFx/OilPaint"},
        {"ColorLUT",            "BBFx/ColorLUT"},
        {"RadialBlur",          "BBFx/RadialBlur"},
        {"FeedbackZoom",        "BBFx/FeedbackZoom"},
        {"MotionTrailFB",       "BBFx/MotionTrailFB"},
    };
}

} // namespace bbfx

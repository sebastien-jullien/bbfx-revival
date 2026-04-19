#pragma once

#include <cstdio>
#include <string>

namespace bbfx {

/// v3.5 Lot S — minimal ImGui dialog driving PluginAuthoringBackend.
///
/// Three modes (File > Export > ...) :
///   - Export Subgraph : prefilled with the user's selection
///   - Export Scene Preset : captures current scene config
///   - Export Output Template : captures OutputManager config
class PluginAuthoringDialog {
public:
    enum class Mode { Subgraph, Scene, OutputTemplate };

    PluginAuthoringDialog();

    void open(Mode m);
    void render(bool* keepOpen);

    // Prefill helpers (called by the menu handler before open()).
    void setPrefillName(const std::string& s) {
        std::snprintf(mName, sizeof(mName), "%s", s.c_str());
    }

private:
    Mode mMode = Mode::Subgraph;
    char mId[128]          = {};
    char mName[128]        = {};
    char mAuthor[128]      = {};
    char mVersion[32]      = "1.0.0";
    char mDescription[512] = {};
    char mLicense[32]      = "MIT";
    std::string mLastResult;   // path written, or empty on failure
    std::string mLastError;

    void generate();
};

} // namespace bbfx

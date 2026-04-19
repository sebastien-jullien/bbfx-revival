#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot G — Plugin runtime errors panel.
///
/// Shows the PluginErrorLog contents with filters (severity, plugin id),
/// per-entry re-enable action (when the plugin is currently in FAILED
/// state), and a Clear button. Opened from menu Plugins > Errors...
class PluginErrorsPanel {
public:
    PluginErrorsPanel() = default;

    void render(bool* open);

private:
    int    mSeverityFilter = 0;  // 0=All, 1=Warning+, 2=Error only
    char   mPluginIdFilter[96] = {};
};

} // namespace bbfx

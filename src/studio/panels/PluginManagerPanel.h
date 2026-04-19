#pragma once

#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot G — Plugin Manager UI panel.
///
/// Replaces the inline Lot-D placeholder window inside StudioApp. Two tabs:
///   - "Installed" : full list + per-plugin actions (toggle enable, context
///     menu with Check Update / Uninstall / Open Folder / Fork & Remix),
///     badges, sort, search, storage readout, bulk enable/disable.
///   - "Community" : placeholder until Lot H lands the full VS Code
///     Marketplace-style browser.
class PluginManagerPanel {
public:
    PluginManagerPanel() = default;

    /// Draw the panel. The window is decorated with a close button; caller
    /// owns the visibility bool via `open`.
    void render(bool* open);

    /// Which tab is currently active. 0 = Installed, 1 = Community.
    int  activeTab() const { return mActiveTab; }
    void setActiveTab(int tab) { mActiveTab = tab; }

private:
    void renderInstalledTab();
    void renderCommunityTab();
    void renderUninstallConfirm();

    int    mActiveTab = 0;
    int    mSortMode  = 0;  // 0=name, 1=installed_at, 2=last_used, 3=size
    char   mSearchBuf[128] = {};
    bool   mUseRegex = false;

    // Uninstall confirm modal
    std::string mPendingUninstall;  // non-empty while the modal is open
    bool        mOpenUninstallPopup = false;
};

} // namespace bbfx

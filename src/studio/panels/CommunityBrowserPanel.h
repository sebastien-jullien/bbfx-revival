#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bbfx {

struct CommunityIndexEntry;

/// v3.5 Lot H — Community Browser (VS Code Marketplace style).
///
/// Three-column layout :
///   [  Sidebar filters 240px  ][   Grid of plugin cards   ][ Details 380px ]
/// The sidebar offers search + categories + tags + author + license +
/// rating + bbfx_version filters, plus a sort-mode selector.
/// The grid shows plugin cards with thumbnail, name, author, rating,
/// install count, badges. Selecting one opens the details pane on the
/// right (README via MarkdownRenderer, permissions icons, Install/Report
/// buttons).
///
/// GIF hover + ratings from GitHub Reactions API arrive in Lot I.
/// Install lands via PluginManager::installFromUrl and goes through the
/// existing PermissionPromptDialog (Lot F).
class CommunityBrowserPanel {
public:
    CommunityBrowserPanel();

    void render(bool* open);

    // Trigger a refresh on the next frame (used by the Manage > Community
    // button + by the Ctrl+Shift+P "Refresh community" command).
    void requestRefresh() { mRefreshPending = true; }

    // Focus the grid on a specific entry (e.g. a `bbfx://install/<id>`
    // deep link or a Command Palette selection). If the entry doesn't
    // exist yet, a toast is emitted and the selection clears.
    void focusOnEntry(const std::string& id);

    // Author profile integration (Lot I): the browser holds a weak ref
    // to an AuthorProfilePanel; clicking an author name forwards through
    // this callback so the StudioApp can raise the author window.
    void setOnOpenAuthor(std::function<void(const std::string& author)> cb) {
        mOnOpenAuthor = std::move(cb);
    }

private:
    void ensureLoaded();
    void renderSidebar();
    void renderGrid(float gridWidth);
    void renderDetails(float detailWidth);
    void renderCard(const CommunityIndexEntry& e);
    void startInstall(const CommunityIndexEntry& e);

    // Filter state
    std::string mSearch;
    std::vector<std::string> mCategories;   // active categories
    std::vector<std::string> mLicenses;     // active licenses
    std::string mAuthor;
    float       mMinRating = 0.0f;
    std::string mMinBbfxVersion;
    int         mSortMode = 0;              // 0=Trending 1=Installs 2=Updated 3=Top 4=New 5=Name
    bool        mShowFeaturedOnly = false;

    // Selection
    std::string mSelectedId;
    std::string mSelectedReadmeCache;  // per-session README cache (url->body)

    // Loading state
    bool mRefreshPending = false;
    bool mLoading = false;

    // Known taxonomy — built from the current index on refresh.
    std::vector<std::string> mAllCategories;
    std::vector<std::string> mAllLicenses;
    std::vector<std::string> mAllAuthors;

    // Cached README bodies per entry id (simple LRU-ish capped map).
    std::unordered_map<std::string, std::string> mReadmeByEntry;

    std::function<void(const std::string& author)> mOnOpenAuthor;
};

} // namespace bbfx

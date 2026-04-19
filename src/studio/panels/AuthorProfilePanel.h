#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot I — per-author plugin listing.
///
/// Spawned from the Community Browser details pane (click the author
/// name) or the Command Palette. Shows:
///   - Author display name + any declared URL
///   - Every plugin in the community index whose `author` matches
///   - Aggregate stats: plugin count, total installs, avg rating
///
/// Clicking a plugin row selects it in the Community Browser.
class AuthorProfilePanel {
public:
    AuthorProfilePanel() = default;

    void render(bool* open);

    // Pick which author to show. If `name` changes, the panel scrolls to
    // the top. Passing an empty string shows "no author selected".
    void setAuthor(const std::string& name);
    const std::string& author() const { return mAuthor; }

    // Set an optional callback: when the user clicks a plugin id in the
    // list, this fires so the caller can raise the Community Browser
    // and pre-select it.
    void setOnSelectPlugin(void (*cb)(const std::string& id)) { mOnSelect = cb; }

private:
    std::string mAuthor;
    void      (*mOnSelect)(const std::string& id) = nullptr;
};

} // namespace bbfx

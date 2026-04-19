#include "CommunityBrowserPanel.h"

#include <algorithm>
#include <cstdio>
#include <set>

#include <imgui.h>

#include "../../plugin/CommunityIndex.h"
#include "../../plugin/GithubReactionsFetcher.h"
#include "../../plugin/PluginManager.h"
#include "../../plugin/PluginManifest.h"
#include "../../network/HttpClient.h"
#include "../MarkdownRenderer.h"
#include "../ToastSystem.h"
#include "../dialogs/PermissionPromptDialog.h"

namespace bbfx {

namespace {

const char* kSortLabels[] = {
    "Trending", "Most Installed", "Recently Updated",
    "Top Rated", "New", "Name"
};

const char* kLicenseOptions[] = {
    "MIT", "Apache-2.0", "GPL-3.0", "BSD-3-Clause", "CC0-1.0", "Proprietary"
};

const char* kCategoryOptions[] = {
    "Geometry", "Color", "PostProcess", "Particle", "Camera",
    "Audio", "Scene", "Gamepad", "Custom",
    "OutputTemplate", "ScenePreset"
};

void openUrlInBrowser(const std::string& url) {
    if (url.empty()) return;
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + url + "\"";
#else
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    std::system(cmd.c_str());
}

std::string formatInstalls(int n) {
    char buf[32];
    if (n < 1000)           std::snprintf(buf, sizeof(buf), "%d installs", n);
    else if (n < 1'000'000) std::snprintf(buf, sizeof(buf), "%.1fk installs", n / 1000.0);
    else                    std::snprintf(buf, sizeof(buf), "%.1fM installs", n / 1'000'000.0);
    return buf;
}

std::string starsString(float rating) {
    // 0..5 with half-steps. ImGui's default font doesn't carry Unicode
    // stars reliably, so we use ASCII approximations.
    int full = static_cast<int>(rating + 0.25f);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d/5", full);
    return buf;
}

} // anonymous

CommunityBrowserPanel::CommunityBrowserPanel() = default;

void CommunityBrowserPanel::focusOnEntry(const std::string& id) {
    ensureLoaded();
    if (CommunityIndex::instance().findById(id)) {
        mSelectedId = id;
    } else {
        ToastSystem::instance().toast("Plugin '" + id + "' not in community index",
                                      ToastSeverity::Warning, 4.0f);
        mSelectedId.clear();
    }
}

void CommunityBrowserPanel::ensureLoaded() {
    if (!mRefreshPending && CommunityIndex::instance().isLoaded()) return;
    mRefreshPending = false;

    if (mLoading) return;
    mLoading = true;
    ToastSystem::instance().toast("Loading community index...",
                                    ToastSeverity::Info, 2.0f);
    CommunityIndex::instance().loadOrRefresh([this](bool ok) {
        mLoading = false;
        if (!ok) {
            ToastSystem::instance().toast(
                "Community index load failed: " + CommunityIndex::instance().lastError(),
                ToastSeverity::Error, 6.0f);
            return;
        }

        // Rebuild taxonomy caches.
        mAllCategories.clear();
        mAllLicenses.clear();
        mAllAuthors.clear();
        std::set<std::string> cats, lics, auths;
        for (const auto& e : CommunityIndex::instance().entries()) {
            if (!e.category.empty()) cats.insert(e.category);
            if (!e.license.empty())  lics.insert(e.license);
            if (!e.author.empty())   auths.insert(e.author);
        }
        mAllCategories.assign(cats.begin(), cats.end());
        mAllLicenses.assign(lics.begin(), lics.end());
        mAllAuthors.assign(auths.begin(), auths.end());

        ToastSystem::instance().toast(
            "Community index loaded (" +
            std::to_string(CommunityIndex::instance().size()) + " plugins)",
            ToastSeverity::Info, 3.0f);
    });
}

void CommunityBrowserPanel::render(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize({ 1100, 620 }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Community Browser##v35", open)) {
        ImGui::End();
        return;
    }

    ensureLoaded();

    // Layout: sidebar | grid | details (collapsible)
    const float sidebarW = 240.0f;
    const float detailsW = 380.0f;
    const float spacing  = 8.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    const float gridW = std::max(200.0f, available - sidebarW - detailsW - 2 * spacing);

    ImGui::BeginChild("##cb_sidebar", { sidebarW, 0 }, true);
    renderSidebar();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##cb_grid", { gridW, 0 }, true);
    renderGrid(gridW);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##cb_details", { 0, 0 }, true);
    renderDetails(detailsW);
    ImGui::EndChild();

    ImGui::End();
}

void CommunityBrowserPanel::renderSidebar() {
    ImGui::TextDisabled("Search & Filters");
    ImGui::Separator();

    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", mSearch.c_str());
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##cb_search", "Search...", buf, sizeof(buf))) {
        mSearch = buf;
    }

    // Sort
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##cb_sort", &mSortMode, kSortLabels, IM_ARRAYSIZE(kSortLabels));

    ImGui::Checkbox("Featured only", &mShowFeaturedOnly);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Categories", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto* c : kCategoryOptions) {
            bool on = std::find(mCategories.begin(), mCategories.end(), c) != mCategories.end();
            if (ImGui::Checkbox(c, &on)) {
                if (on) mCategories.push_back(c);
                else    mCategories.erase(std::remove(mCategories.begin(),
                                                      mCategories.end(), c),
                                          mCategories.end());
            }
        }
    }

    if (ImGui::CollapsingHeader("License")) {
        for (const auto* l : kLicenseOptions) {
            bool on = std::find(mLicenses.begin(), mLicenses.end(), l) != mLicenses.end();
            if (ImGui::Checkbox(l, &on)) {
                if (on) mLicenses.push_back(l);
                else    mLicenses.erase(std::remove(mLicenses.begin(),
                                                     mLicenses.end(), l),
                                        mLicenses.end());
            }
        }
    }

    if (ImGui::CollapsingHeader("Author / Rating / Version")) {
        char authBuf[128];
        std::snprintf(authBuf, sizeof(authBuf), "%s", mAuthor.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##cb_author", "Author (contains)",
                                     authBuf, sizeof(authBuf))) {
            mAuthor = authBuf;
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##cb_minrating", &mMinRating, 0.0f, 5.0f,
                            "Min rating: %.1f");
        static const char* kVers[] = { "any", "3.5", "3.6", "3.7", "4.0" };
        int vi = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kVers); ++i)
            if (mMinBbfxVersion == kVers[i]) { vi = i; break; }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##cb_minver", &vi, kVers, IM_ARRAYSIZE(kVers))) {
            mMinBbfxVersion = (vi == 0) ? "" : kVers[vi];
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Refresh", {-1, 0})) mRefreshPending = true;
    ImGui::TextDisabled("%zu plugins in index",
                         CommunityIndex::instance().size());
}

void CommunityBrowserPanel::renderGrid(float gridWidth) {
    if (!CommunityIndex::instance().isLoaded()) {
        ImGui::TextDisabled("Community index is loading or could not be fetched.");
        if (!CommunityIndex::instance().lastError().empty()) {
            ImGui::TextColored({1.0f, 0.5f, 0.5f, 1.0f}, "%s",
                                CommunityIndex::instance().lastError().c_str());
        }
        return;
    }

    CommunityIndex::Filter f;
    f.search          = mSearch;
    f.categories      = mCategories;
    f.licenses        = mLicenses;
    f.author          = mAuthor;
    f.minRating       = mMinRating;
    f.minBbfxVersion  = mMinBbfxVersion;
    f.sortMode        = mSortMode;

    auto entries = CommunityIndex::instance().filtered(f);
    if (mShowFeaturedOnly) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [](const CommunityIndexEntry* e) { return !e->featured; }),
            entries.end());
    }

    // Featured section (top) — shown when not already filtered to featured.
    if (!mShowFeaturedOnly) {
        std::vector<const CommunityIndexEntry*> featured;
        for (const auto* e : entries) if (e->featured) featured.push_back(e);
        if (!featured.empty()) {
            ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "* Featured");
            ImGui::Separator();
            for (const auto* e : featured) renderCard(*e);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    if (entries.empty()) {
        ImGui::TextDisabled("(no plugins match the current filters)");
        return;
    }
    for (const auto* e : entries) renderCard(*e);
}

void CommunityBrowserPanel::renderCard(const CommunityIndexEntry& e) {
    ImGui::PushID(e.id.c_str());
    bool selected = (mSelectedId == e.id);

    const ImVec2 padding{8, 6};
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
    if (ImGui::Selectable(("##cb_sel_" + e.id).c_str(), selected,
                           ImGuiSelectableFlags_AllowDoubleClick,
                           {0, 70})) {
        mSelectedId = e.id;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            startInstall(e);
        }
    }
    ImGui::PopStyleVar();

    // Draw the contents overlayed on top of the Selectable.
    auto* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();

    ImGui::SetCursorScreenPos({p0.x + 8, p0.y + 4});
    ImGui::TextUnformatted(e.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", e.version.c_str());
    if (e.featured) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "[Featured]");
    }

    ImGui::SetCursorScreenPos({p0.x + 8, p0.y + 24});
    ImGui::TextDisabled("by %s", e.author.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", e.category.c_str());

    ImGui::SetCursorScreenPos({p0.x + 8, p0.y + 42});
    if (e.rating > 0) {
        ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "%s", starsString(e.rating).c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%d)", e.ratingCount);
        ImGui::SameLine();
    }
    ImGui::TextDisabled("%s", formatInstalls(e.installs).c_str());

    // Quick install icon on the right (small ▼ — plain text "Install")
    float btnW = 80.0f;
    ImGui::SetCursorScreenPos({p1.x - btnW - 6, p0.y + 24});
    if (ImGui::SmallButton(("Install##" + e.id).c_str())) {
        startInstall(e);
    }

    (void)dl;
    ImGui::PopID();
}

void CommunityBrowserPanel::renderDetails(float /*detailWidth*/) {
    if (mSelectedId.empty()) {
        ImGui::TextDisabled("Select a plugin on the left to see its details.");
        ImGui::Spacing();
        ImGui::TextDisabled("Tip: Double-click a card to install it directly.");
        return;
    }
    const auto* e = CommunityIndex::instance().findById(mSelectedId);
    if (!e) {
        ImGui::TextDisabled("(selection out of sync — pick another plugin)");
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%s", e->name.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("v%s", e->version.c_str());
    if (e->featured) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "[Featured]");
    }

    // Author name is clickable (Lot I): forwards to the AuthorProfilePanel
    // via the mOnOpenAuthor hook wired by StudioApp.
    if (mOnOpenAuthor) {
        ImGui::TextDisabled("by");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
        ImGui::TextUnformatted(e->author.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) mOnOpenAuthor(e->author);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show all plugins from %s",
                                                       e->author.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("—  %s  —  %s",
                             e->license.c_str(),
                             e->category.c_str());
    } else {
        ImGui::TextDisabled("by %s  —  %s  —  %s",
                             e->author.c_str(),
                             e->license.c_str(),
                             e->category.c_str());
    }

    // v3.5 Lot I — request the live rating from GitHub Reactions when the
    // details pane opens a plugin that has a `githubIssue` wired. The
    // call is cached (1h TTL) so leaving and returning to the same entry
    // doesn't re-hit the API.
    if (e->githubIssue > 0 && !e->liveRatingValid) {
        int issue = e->githubIssue;
        std::string id = e->id;
        GithubReactionsFetcher::instance().fetch(issue,
            [id](GithubReactionsFetcher::Result r) {
                auto* me = CommunityIndex::instance().findById(id);
                if (!me || !r.valid) return;
                me->liveRating      = r.rating;
                me->liveRatingCount = r.thumbsUp + r.thumbsDown;
                me->liveRatingValid = true;
            });
    }

    ImGui::Spacing();

    float displayRating = e->liveRatingValid ? e->liveRating : e->rating;
    int   displayCount  = e->liveRatingValid ? e->liveRatingCount : e->ratingCount;
    if (displayRating > 0) {
        ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "%s", starsString(displayRating).c_str());
        ImGui::SameLine();
        if (e->liveRatingValid) {
            ImGui::TextDisabled("(%d live)", displayCount);
        } else {
            ImGui::TextDisabled("(%d ratings)", displayCount);
        }
        ImGui::SameLine();
    }
    ImGui::TextDisabled("%s", formatInstalls(e->installs).c_str());

    if (!e->description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", e->description.c_str());
    }

    if (!e->permissions.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Permissions:");
        for (auto p : e->permissions) {
            const auto& d = describePermission(p);
            ImGui::BulletText("%s  %s", d.icon, d.label);
            ImGui::Indent();
            ImGui::TextDisabled("%s", d.description);
            ImGui::Unindent();
        }
    } else {
        ImGui::Spacing();
        ImGui::TextDisabled("No special permissions requested.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Action buttons
    if (ImGui::Button("Install", {140, 0})) startInstall(*e);
    ImGui::SameLine();
    if (ImGui::Button("Homepage", {120, 0})) openUrlInBrowser(e->homepage);
    ImGui::SameLine();
    if (ImGui::Button("Report", {80, 0})) {
        std::string reportUrl = e->homepage;
        if (reportUrl.empty()) reportUrl = "https://github.com/bonneballefx/bbfx-community-plugins/issues";
        openUrlInBrowser(reportUrl);
    }

    ImGui::Spacing();
    if (ImGui::BeginTabBar("##cb_details_tabs")) {
        if (ImGui::BeginTabItem("README")) {
            auto it = mReadmeByEntry.find(e->id);
            if (it == mReadmeByEntry.end()) {
                ImGui::TextDisabled("Loading README...");
                if (!e->readmeUrl.empty()) {
                    std::string id = e->id;
                    std::string url = e->readmeUrl;
                    HttpClient::instance().get(url, [this, id](HttpResponse r) {
                        if (r.status == 200 && r.error.empty()) {
                            mReadmeByEntry[id] = r.body;
                        } else {
                            mReadmeByEntry[id] = "(README fetch failed: " + r.error + ")";
                        }
                    });
                    mReadmeByEntry[e->id] = "";   // placeholder so we don't re-fetch
                } else {
                    mReadmeByEntry[e->id] = "(no README URL provided by index entry)";
                }
            } else {
                if (it->second.empty()) {
                    ImGui::TextDisabled("Loading...");
                } else {
                    MarkdownRenderer::draw(it->second);
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Details")) {
            ImGui::BulletText("id: %s", e->id.c_str());
            ImGui::BulletText("size: %llu bytes", static_cast<unsigned long long>(e->size));
            ImGui::BulletText("sha256: %s", e->sha256.empty() ? "(not set)" : e->sha256.c_str());
            ImGui::BulletText("updated: %s", e->updatedAt.c_str());
            ImGui::BulletText("bbfx_version: %s", e->bbfxVersion.c_str());
            if (!e->tags.empty()) {
                ImGui::BulletText("tags:");
                for (const auto& t : e->tags) { ImGui::SameLine(); ImGui::TextDisabled("[%s]", t.c_str()); }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void CommunityBrowserPanel::startInstall(const CommunityIndexEntry& e) {
    if (e.downloadUrl.empty()) {
        ToastSystem::instance().toast("No downloadUrl for " + e.id,
                                        ToastSeverity::Error, 5.0f);
        return;
    }

    // We don't know the plugin's permissions for sure until after the ZIP
    // is extracted (the index may lag the manifest). The install pipeline
    // (StudioApp::promptInstallZip) re-parses and prompts. Here we just
    // download + chain into that flow through PluginManager::installFromUrl.
    std::string id = e.id;
    ToastSystem::instance().toast("Downloading " + e.name + "...",
                                   ToastSeverity::Info, 2.5f);
    PluginManager::instance().installFromUrl(e.downloadUrl, e.sha256,
        [id](bool ok, std::string installedId, std::string error) {
            if (!ok) {
                ToastSystem::instance().toast(
                    "Install failed (" + id + "): " + error,
                    ToastSeverity::Error, 7.0f);
                return;
            }
            ToastSystem::instance().toast(
                "Installed '" + installedId + "'",
                ToastSeverity::Info, 4.0f);
        });
}

} // namespace bbfx

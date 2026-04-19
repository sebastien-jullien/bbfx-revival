#include "AuthorProfilePanel.h"

#include <cstdio>
#include <cstdlib>

#include <imgui.h>

#include "../../plugin/CommunityIndex.h"

namespace bbfx {

namespace {

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
    if (n < 1000)             std::snprintf(buf, sizeof(buf), "%d", n);
    else if (n < 1'000'000)   std::snprintf(buf, sizeof(buf), "%.1fk", n / 1000.0);
    else                      std::snprintf(buf, sizeof(buf), "%.1fM", n / 1'000'000.0);
    return buf;
}

} // anonymous

void AuthorProfilePanel::setAuthor(const std::string& name) {
    mAuthor = name;
}

void AuthorProfilePanel::render(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize({ 520, 420 }, ImGuiCond_FirstUseEver);
    std::string title = "Author — " + (mAuthor.empty() ? std::string("(none)")
                                                       : mAuthor) + "##bbfx_author";
    if (!ImGui::Begin(title.c_str(), open)) {
        ImGui::End();
        return;
    }

    if (mAuthor.empty()) {
        ImGui::TextDisabled("No author selected.");
        ImGui::TextDisabled("Click an author name in the Community Browser to "
                             "see their other plugins.");
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%s", mAuthor.c_str());
    ImGui::PopStyleColor();

    // Collect the author's entries and aggregate stats.
    std::vector<const CommunityIndexEntry*> mine;
    int    totalInstalls = 0;
    double ratingAcc     = 0.0;
    int    ratingN       = 0;
    std::string authorUrl;
    for (const auto& e : CommunityIndex::instance().entries()) {
        if (e.author != mAuthor) continue;
        mine.push_back(&e);
        totalInstalls += e.installs;
        if (e.ratingCount > 0) { ratingAcc += e.rating; ratingN += 1; }
        // In Lot I the index stores only the author name as a string; a
        // dedicated URL may be added in a later revision of the schema.
        // If we ever stash it alongside the entries we pick it up here.
    }

    ImGui::TextDisabled("%zu plugins  |  %s installs  |  avg rating %.1f",
                         mine.size(),
                         formatInstalls(totalInstalls).c_str(),
                         ratingN > 0 ? (ratingAcc / ratingN) : 0.0);

    // Offer a GitHub search for "<author> bbfx" as a best-effort profile.
    ImGui::Spacing();
    if (ImGui::Button("Search GitHub for this author")) {
        std::string q = "https://github.com/search?q=" + mAuthor + "+bbfx&type=users";
        openUrlInBrowser(q);
    }

    ImGui::Separator();

    if (mine.empty()) {
        ImGui::TextDisabled("(no plugins from %s in the index yet)", mAuthor.c_str());
    } else {
        ImGui::BeginChild("##authorPlugins", { 0, 0 });
        for (const auto* e : mine) {
            ImGui::PushID(e->id.c_str());
            if (ImGui::Selectable((e->name + "  v" + e->version + "##row").c_str())) {
                if (mOnSelect) mOnSelect(e->id);
            }
            ImGui::Indent();
            ImGui::TextDisabled("%s  —  %s  —  %s installs",
                                 e->category.c_str(),
                                 e->license.c_str(),
                                 formatInstalls(e->installs).c_str());
            if (!e->description.empty()) {
                ImGui::TextWrapped("%s", e->description.c_str());
            }
            ImGui::Unindent();
            ImGui::Spacing();
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace bbfx

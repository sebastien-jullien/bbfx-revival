#include "SetEditorPanel.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace bbfx {

SetEditorPanel::SetEditorPanel(sol::state& lua) : mLua(lua) {}

void SetEditorPanel::render() {
    ImGui::Begin("Set Editor");

    ImGui::TextDisabled("VJ Set List");

    // Add segment button
    if (ImGui::Button("+ Add Segment")) {
        Segment seg;
        seg.name = "Segment " + std::to_string(mSegments.size() + 1);
        mSegments.push_back(seg);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Set") && !mSegments.empty()) {
        mPlaying = !mPlaying;
        if (mPlaying) mCurrentSegment = 0;
    }
    if (mPlaying) {
        ImGui::SameLine();
        ImGui::TextColored({0, 1, 0, 1}, "PLAYING: %s",
            mCurrentSegment < (int)mSegments.size() ?
            mSegments[mCurrentSegment].name.c_str() : "END");
    }

    ImGui::Separator();

    // Segment list
    for (int i = 0; i < (int)mSegments.size(); i++) {
        auto& seg = mSegments[i];
        bool isCurrent = (i == mCurrentSegment && mPlaying);

        ImGui::PushID(i);
        if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Header, {0.0f, 0.4f, 0.0f, 1.0f});

        bool open = ImGui::TreeNode("##seg", "%d. %s", i + 1, seg.name.c_str());
        if (isCurrent) ImGui::PopStyleColor();

        // Drag to reorder
        if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
            int next = i + (ImGui::GetMouseDragDelta(0).y < 0.0f ? -1 : 1);
            if (next >= 0 && next < (int)mSegments.size()) {
                std::swap(mSegments[i], mSegments[next]);
                ImGui::ResetMouseDragDelta();
            }
        }

        if (open) {
            char nameBuf[128];
            std::strncpy(nameBuf, seg.name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                seg.name = nameBuf;

            char srcBuf[256];
            std::strncpy(srcBuf, seg.source.c_str(), sizeof(srcBuf) - 1);
            srcBuf[sizeof(srcBuf)-1] = '\0';
            if (ImGui::InputText("Source", srcBuf, sizeof(srcBuf)))
                seg.source = srcBuf;

            ImGui::SliderInt("Duration (bars)", &seg.durationBars, 1, 128);
            ImGui::SliderFloat("BPM", &seg.bpm, 40, 200);

            const char* transitions[] = {"cut", "crossfade", "fade_in", "fade_out"};
            int transIdx = 0;
            for (int t = 0; t < 4; t++) if (seg.transition == transitions[t]) transIdx = t;
            if (ImGui::Combo("Transition", &transIdx, transitions, 4))
                seg.transition = transitions[transIdx];

            ImGui::SliderInt("Trans. beats", &seg.transitionBeats, 1, 16);

            char notesBuf[512];
            std::strncpy(notesBuf, seg.notes.c_str(), sizeof(notesBuf) - 1);
            notesBuf[sizeof(notesBuf)-1] = '\0';
            if (ImGui::InputTextMultiline("Notes", notesBuf, sizeof(notesBuf), {-1, 40}))
                seg.notes = notesBuf;

            if (ImGui::Button("Delete")) {
                mSegments.erase(mSegments.begin() + i);
                ImGui::TreePop();
                ImGui::PopID();
                break; // iterator invalidated
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // Navigation
    if (!mSegments.empty()) {
        ImGui::Separator();
        if (ImGui::Button("<< Prev")) prevSegment();
        ImGui::SameLine();
        if (ImGui::Button("Next >>")) nextSegment();
    }

    ImGui::End();
}

void SetEditorPanel::nextSegment() {
    if (mCurrentSegment < (int)mSegments.size() - 1) mCurrentSegment++;
}

void SetEditorPanel::prevSegment() {
    if (mCurrentSegment > 0) mCurrentSegment--;
}

void SetEditorPanel::saveSet(const std::string& path) {
    using json = nlohmann::json;
    json j;
    j["version"] = 1;
    j["segments"] = json::array();
    for (auto& seg : mSegments) {
        j["segments"].push_back({
            {"name", seg.name},
            {"source", seg.source},
            {"duration_bars", seg.durationBars},
            {"bpm", seg.bpm},
            {"transition", seg.transition},
            {"transition_beats", seg.transitionBeats},
            {"notes", seg.notes}
        });
    }
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(2);
        std::cout << "[SetEditor] Saved to " << path << std::endl;
    }
}

void SetEditorPanel::loadSet(const std::string& path) {
    using json = nlohmann::json;
    std::ifstream file(path);
    if (!file.is_open()) return;

    json j;
    file >> j;
    mSegments.clear();
    mCurrentSegment = 0;

    if (j.contains("segments")) {
        for (auto& s : j["segments"]) {
            Segment seg;
            seg.name = s.value("name", "Untitled");
            seg.source = s.value("source", "");
            seg.durationBars = s.value("duration_bars", 32);
            seg.bpm = s.value("bpm", 120.0f);
            seg.transition = s.value("transition", "cut");
            seg.transitionBeats = s.value("transition_beats", 4);
            seg.notes = s.value("notes", "");
            mSegments.push_back(seg);
        }
    }
    std::cout << "[SetEditor] Loaded " << mSegments.size() << " segments from " << path << std::endl;
}

} // namespace bbfx

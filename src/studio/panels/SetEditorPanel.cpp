#include "SetEditorPanel.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace bbfx {

SetEditorPanel::SetEditorPanel(sol::state& lua) : mLua(lua) {}

void SetEditorPanel::update(float deltaTime) {
    if (!mPlaying || mSegments.empty()) return;
    if (mCurrentSegment >= (int)mSegments.size()) {
        stopPlayback();
        return;
    }

    auto& seg = mSegments[mCurrentSegment];
    double beatsPerSecond = seg.bpm / 60.0;
    double dBeats = beatsPerSecond * deltaTime;
    mElapsedBeats += dBeats;

    double totalBeats = seg.durationBars * 4.0; // 4 beats per bar

    // Transition phase check (starts transitionBeats before end)
    double transitionStart = totalBeats - seg.transitionBeats;
    if (mElapsedBeats >= transitionStart && !mInTransition && seg.transition != "cut") {
        mInTransition = true;
        mTransitionBeats = 0.0;
    }

    // Update transition alpha
    if (mInTransition && mCrossfadeAlphaCb) {
        mTransitionBeats += dBeats;
        float alpha = static_cast<float>(mTransitionBeats / seg.transitionBeats);
        if (alpha > 1.0f) alpha = 1.0f;

        if (seg.transition == "crossfade") {
            mCrossfadeAlphaCb(alpha); // 0→1 blend to next
        } else if (seg.transition == "fade_out") {
            mCrossfadeAlphaCb(1.0f - alpha); // 1→0 fade out current
        } else if (seg.transition == "fade_in") {
            mCrossfadeAlphaCb(alpha); // 0→1 fade in next
        }
    }

    // Auto-advance when segment duration is reached
    if (mElapsedBeats >= totalBeats) {
        int nextIdx = mCurrentSegment + 1;
        if (nextIdx < (int)mSegments.size()) {
            advanceToSegment(nextIdx);
        } else {
            // End of set
            stopPlayback();
            std::cout << "[SetEditor] Set playback finished" << std::endl;
        }
    }
}

void SetEditorPanel::advanceToSegment(int index) {
    if (index < 0 || index >= (int)mSegments.size()) return;
    mCurrentSegment = index;
    mElapsedBeats = 0.0;
    mInTransition = false;
    mTransitionBeats = 0.0;

    auto& seg = mSegments[index];

    // Set BPM
    if (mSetBpmCb) mSetBpmCb(seg.bpm);

    // Reset crossfade alpha
    if (mCrossfadeAlphaCb) mCrossfadeAlphaCb(1.0f);

    // Load source project if specified
    if (!seg.source.empty() && mLoadProjectCb) {
        mLoadProjectCb(seg.source);
    }

    std::cout << "[SetEditor] Now playing segment " << index << ": " << seg.name << std::endl;
}

void SetEditorPanel::stopPlayback() {
    mPlaying = false;
    mElapsedBeats = 0.0;
    mInTransition = false;
    mTransitionBeats = 0.0;
    if (mCrossfadeAlphaCb) mCrossfadeAlphaCb(1.0f);
}

void SetEditorPanel::render() {
    ImGui::Begin("Set Editor");

    ImGui::TextDisabled("VJ Set List");

    // Transport controls
    if (ImGui::Button("+ Add Segment")) {
        Segment seg;
        seg.name = "Segment " + std::to_string(mSegments.size() + 1);
        mSegments.push_back(seg);
    }
    ImGui::SameLine();
    if (!mPlaying) {
        if (ImGui::Button("Play Set") && !mSegments.empty()) {
            mPlaying = true;
            advanceToSegment(0);
        }
    } else {
        if (ImGui::Button("Stop")) {
            stopPlayback();
        }
    }

    if (mPlaying && mCurrentSegment < (int)mSegments.size()) {
        auto& seg = mSegments[mCurrentSegment];
        double totalBeats = seg.durationBars * 4.0;
        float progress = static_cast<float>(mElapsedBeats / totalBeats);
        if (progress > 1.0f) progress = 1.0f;

        ImGui::SameLine();
        ImGui::TextColored({0, 1, 0, 1}, "PLAYING: %s", seg.name.c_str());

        // Progress bar
        char overlay[64];
        int currentBar = static_cast<int>(mElapsedBeats / 4.0) + 1;
        std::snprintf(overlay, sizeof(overlay), "Bar %d / %d", currentBar, seg.durationBars);
        ImGui::ProgressBar(progress, {-1, 0}, overlay);

        // Transition indicator
        if (mInTransition) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.6f, 0.0f, 1.0f}, "[%s]", seg.transition.c_str());
        }
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

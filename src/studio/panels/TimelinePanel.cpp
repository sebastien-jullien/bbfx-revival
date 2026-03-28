#include "TimelinePanel.h"
#include "../StudioEngine.h"
#include "../../core/PrimitiveNodes.h"
#include "../../core/Animator.h"
#include "../../audio/AudioAnalyzer.h"
#include "../../record/InputRecorder.h"

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace bbfx {

void TimelinePanel::ensureDefaultChords() {
    if (!mChordBlocks.empty()) return;
    mChordBlocks.push_back({"Intro",   0.0f,   4.0f, 0.5f});
    mChordBlocks.push_back({"Verse",   4.0f,   12.0f, 0.6f});
    mChordBlocks.push_back({"Chorus",  12.0f,  20.0f, 0.0f});
    mChordBlocks.push_back({"Bridge",  20.0f,  24.0f, 0.8f});
    mChordBlocks.push_back({"Drop",    24.0f,  32.0f, 0.3f});
}

void TimelinePanel::render(StudioEngine* engine) {
    ensureDefaultChords();
    ImGui::Begin("Timeline");

    renderTransport(engine);
    ImGui::SameLine(0, 20);
    renderBPMDisplay();

    ImGui::Separator();

    // Timeline canvas
    ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = std::max(canvasSize.y, 80.0f);

    float currentBeat = 0.0f;
    if (auto* time = RootTimeNode::instance()) {
        auto& outputs = time->getOutputs();
        auto it = outputs.find("time");
        if (it != outputs.end()) {
            currentBeat = it->second->getValue() * (mBPM / 60.0f);
        }
    }

    float pixelsPerBeat = 40.0f; // zoom level (TODO: scroll/zoom)

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Clip to canvas
    draw->PushClipRect(canvasPos, {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y}, true);

    renderMarkers(pixelsPerBeat, currentBeat);
    renderChordBlocks(pixelsPerBeat);

    // Playhead
    float playheadX = canvasPos.x + currentBeat * pixelsPerBeat - mScrollX;
    draw->AddLine({playheadX, canvasPos.y}, {playheadX, canvasPos.y + canvasSize.y},
                  IM_COL32(0, 255, 255, 220), 2.0f);

    draw->PopClipRect();

    // Invisible button for scrolling
    ImGui::InvisibleButton("##timeline_canvas", canvasSize);

    ImGui::Separator();
    renderAudioSpectrum();

    ImGui::End();
}

void TimelinePanel::renderTransport(StudioEngine* engine) {
    // Play
    if (ImGui::Button(mPaused ? ">" : "||", {32, 0})) {
        mPaused = !mPaused;
    }
    ImGui::SameLine();
    // Stop
    if (ImGui::Button("[]", {32, 0})) {
        mPaused = false;
        auto* time = RootTimeNode::instance();
        if (time) time->reset();
    }
    ImGui::SameLine();
    // Record
    if (mRecording) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.0f, 0.0f, 1.0f});
    }
    if (ImGui::Button("REC", {40, 0})) {
        mRecording = !mRecording;
        static InputRecorder* recorder = nullptr;
        if (mRecording) {
            recorder = new InputRecorder();
            recorder->start("session.bbfx-session");
            std::cout << "[Timeline] Recording started → session.bbfx-session" << std::endl;
        } else if (recorder) {
            recorder->stop();
            delete recorder;
            recorder = nullptr;
            std::cout << "[Timeline] Recording stopped" << std::endl;
        }
    }
    if (mRecording) {
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "\xe2\x97\x8f"); // ●
    }
}

void TimelinePanel::renderBPMDisplay() {
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputFloat("BPM", &mBPM, 1.0f, 5.0f, "%.1f");
    mBPM = std::clamp(mBPM, 20.0f, 300.0f);
}

void TimelinePanel::renderMarkers(float pixelsPerBeat, float currentBeat) {
    ImDrawList* draw  = ImGui::GetWindowDrawList();
    ImVec2 pos        = ImGui::GetCursorScreenPos();
    ImVec2 avail      = ImGui::GetContentRegionAvail();

    int firstBeat = static_cast<int>(mScrollX / pixelsPerBeat);
    int lastBeat  = firstBeat + static_cast<int>(avail.x / pixelsPerBeat) + 2;

    for (int b = firstBeat; b <= lastBeat; ++b) {
        float x = pos.x + b * pixelsPerBeat - mScrollX;
        bool isBar = (b % 4 == 0);
        ImU32 col  = isBar
            ? IM_COL32(180, 180, 180, 180)
            : IM_COL32(100, 100, 100, 120);
        draw->AddLine({x, pos.y}, {x, pos.y + 60.0f}, col, isBar ? 1.5f : 1.0f);
        if (isBar) {
            char label[16];
            snprintf(label, sizeof(label), "Bar %d", b / 4 + 1);
            draw->AddText({x + 3, pos.y + 2}, IM_COL32(180, 180, 180, 200), label);
        }
    }
}

void TimelinePanel::renderChordBlocks(float pixelsPerBeat) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos       = ImGui::GetCursorScreenPos();

    static const ImU32 blockColors[] = {
        IM_COL32(0, 200, 200, 140),   // cyan
        IM_COL32(200, 0, 200, 140),   // magenta
        IM_COL32(200, 200, 0, 140),   // yellow
        IM_COL32(255, 140, 0, 140),   // orange
        IM_COL32(0, 200, 0, 140),     // green
    };

    for (size_t i = 0; i < mChordBlocks.size(); ++i) {
        auto& cb = mChordBlocks[i];
        float x0 = pos.x + cb.startBeat * pixelsPerBeat - mScrollX;
        float x1 = pos.x + cb.endBeat   * pixelsPerBeat - mScrollX;
        float y0 = pos.y + 20.0f;
        float y1 = pos.y + 55.0f;

        ImU32 col = blockColors[i % 5];
        draw->AddRectFilled({x0, y0}, {x1, y1}, col, 3.0f);
        draw->AddRect({x0, y0}, {x1, y1}, IM_COL32(255,255,255,80), 3.0f);
        draw->AddText({x0 + 4, y0 + 4}, IM_COL32(255,255,255,220), cb.name.c_str());

        // Drag interaction
        ImGui::SetCursorScreenPos({x0, y0});
        std::string btnId = "##chordblock_" + std::to_string(i);
        ImGui::InvisibleButton(btnId.c_str(), {x1 - x0, y1 - y0});

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            mDraggedBlock = static_cast<int>(i);
            float deltaBeat = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / pixelsPerBeat;
            float duration = cb.endBeat - cb.startBeat;

            // Snap to nearest beat
            float newStart = std::round(cb.startBeat + deltaBeat);
            // Clamp to valid range
            if (newStart < 0.0f) newStart = 0.0f;

            cb.startBeat = newStart;
            cb.endBeat   = newStart + duration;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        } else if (mDraggedBlock == static_cast<int>(i) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            mDraggedBlock = -1;
        }
    }
}

void TimelinePanel::renderAudioSpectrum() {
    ImGui::TextDisabled("Spectrum");

    // Try to read spectrum from an AudioAnalyzerNode in the DAG
    auto* animator = Animator::instance();
    if (animator) {
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "AudioAnalyzerNode") {
                auto* analyzerNode = dynamic_cast<AudioAnalyzerNode*>(node);
                if (analyzerNode) {
                    for (int b = 0; b < 8; ++b) {
                        mSpectrumBands[b] = analyzerNode->getBand(b);
                    }
                }
                break;
            }
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos       = ImGui::GetCursorScreenPos();

    static const char* bandNames[] = {"Sub","Bass","Lo","LoMid","Mid","HiMid","Hi","Air"};
    static const ImU32 bandCols[] = {
        IM_COL32(0,100,200,255), IM_COL32(0,150,200,255), IM_COL32(0,200,200,255),
        IM_COL32(0,200,150,255), IM_COL32(0,200,0,255),   IM_COL32(200,200,0,255),
        IM_COL32(255,100,0,255), IM_COL32(255,0,0,255)
    };

    float barW = 24.0f;
    float maxH = 40.0f;
    ImGui::Dummy({barW * 8, maxH});

    for (int b = 0; b < 8; ++b) {
        float h = mSpectrumBands[b] * maxH;
        float x0 = pos.x + b * barW;
        float y1 = pos.y + maxH;
        float y0 = y1 - h;
        draw->AddRectFilled({x0 + 2, y0}, {x0 + barW - 2, y1}, bandCols[b], 2.0f);
        draw->AddText({x0 + 4, pos.y + maxH + 2}, IM_COL32(160,160,160,200), bandNames[b]);
    }
}

} // namespace bbfx

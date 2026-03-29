#include "TimelinePanel.h"
#include "../StudioEngine.h"
#include "../../core/PrimitiveNodes.h"
#include "../../core/Animator.h"
#include "../../audio/AudioAnalyzer.h"
#include "../../audio/BeatDetector.h"
#include "../../record/InputRecorder.h"
#include "../commands/CommandManager.h"
#include "../commands/ChordCommands.h"

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace bbfx {

void TimelinePanel::ensureDefaultChords() {
    if (!mChordBlocks.empty()) return;
    mChordBlocks.push_back({"Intro",   0.0f,   4.0f, 0.5f});
    mChordBlocks.push_back({"Verse",   4.0f,   12.0f, 0.6f});
    mChordBlocks.push_back({"Chorus",  12.0f,  20.0f, 0.0f});
    mChordBlocks.push_back({"Bridge",  20.0f,  24.0f, 0.8f});
    mChordBlocks.push_back({"Drop",    24.0f,  32.0f, 0.3f});
}

void TimelinePanel::stop() {
    mPaused = false;
    if (auto* t = RootTimeNode::instance()) t->reset();
}

void TimelinePanel::render(StudioEngine* engine) {
    ensureDefaultChords();
    ImGui::Begin("Timeline");

    renderTransport(engine);
    ImGui::SameLine(0, 10);

    // "+" button to add chord block
    if (ImGui::Button("+##addchord", {24, 0})) {
        float lastEnd = 0.0f;
        for (auto& cb : mChordBlocks) {
            if (cb.endBeat > lastEnd) lastEnd = cb.endBeat;
        }
        ChordBlock newBlock;
        newBlock.name = "Chord " + std::to_string(mChordBlocks.size() + 1);
        newBlock.startBeat = lastEnd;
        newBlock.endBeat = lastEnd + 4.0f;
        newBlock.hue = std::fmod(mChordBlocks.size() * 0.15f, 1.0f);
        CommandManager::instance().execute(
            std::make_unique<AddChordCommand>(mChordBlocks, newBlock));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add chord block");

    ImGui::SameLine(0, 20);
    renderBPMDisplay();

    // Push BPM from timeline into RootTimeNode so beat/beatFrac outputs are synced
    if (auto* time = RootTimeNode::instance()) {
        time->setBPM(mBPM);

        // Feed the recorder with time advancement and beat events
        if (mRecording && mRecorder) {
            float dt = time->getOutputs().at("dt")->getValue();
            mRecorder->advanceTime(dt);
            // Record a beat event when beatFrac wraps around (crosses zero)
            // Use edge detection: only fire once per beat transition
            float beatFrac = time->getOutputs().at("beatFrac")->getValue();
            static bool beatFired = false;
            if (beatFrac < 0.05f && !beatFired && dt > 0.0f) {
                mRecorder->recordBeat();
                beatFired = true;
            } else if (beatFrac >= 0.05f) {
                beatFired = false;
            }
        }
    }

    // Sync BPM from BeatDetectorNode if present (auto-detect overrides manual)
    auto* animator = Animator::instance();
    if (animator) {
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "BeatDetectorNode") {
                auto& outputs = node->getOutputs();
                auto bpmIt = outputs.find("bpm");
                if (bpmIt != outputs.end()) {
                    float detectedBPM = bpmIt->second->getValue();
                    if (detectedBPM > 20.0f) mBPM = detectedBPM;
                }
                break;
            }
        }
    }

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

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Clip to canvas
    draw->PushClipRect(canvasPos, {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y}, true);

    renderMarkers(mPixelsPerBeat, currentBeat);
    renderChordBlocks(mPixelsPerBeat);

    // Playhead
    float playheadX = canvasPos.x + currentBeat * mPixelsPerBeat - mScrollX;
    draw->AddLine({playheadX, canvasPos.y}, {playheadX, canvasPos.y + canvasSize.y},
                  IM_COL32(0, 255, 255, 220), 2.0f);

    draw->PopClipRect();

    // Invisible button for interactions
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##timeline_canvas", canvasSize);

    // Zoom: mouse wheel over canvas
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            mPixelsPerBeat *= (1.0f + wheel * 0.1f);
            mPixelsPerBeat = std::clamp(mPixelsPerBeat, 10.0f, 200.0f);
        }
    }

    // Scroll: middle mouse drag or Ctrl+left drag
    if (ImGui::IsItemActive()) {
        bool middleDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
        bool ctrlLeftDrag = ImGui::GetIO().KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        if (middleDrag || ctrlLeftDrag) {
            ImVec2 delta = middleDrag
                ? ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle)
                : ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            mScrollX -= delta.x;
            if (mScrollX < 0.0f) mScrollX = 0.0f;
            if (middleDrag) ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
            else ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }

    // Seek: left click (without Ctrl) on timeline area
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !ImGui::GetIO().KeyCtrl) {
        float mouseX = ImGui::GetMousePos().x - canvasPos.x + mScrollX;
        float clickedBeat = mouseX / mPixelsPerBeat;
        if (clickedBeat >= 0.0f) {
            float seekTime = clickedBeat * 60.0f / mBPM;
            if (auto* time = RootTimeNode::instance()) {
                auto& outputs = time->getOutputs();
                auto it = outputs.find("time");
                if (it != outputs.end()) {
                    it->second->setValue(seekTime);
                }
            }
        }
    }

    // Right-click context menu on chord blocks
    if (ImGui::BeginPopup("ChordContextMenu")) {
        if (mContextChordIdx >= 0 && mContextChordIdx < static_cast<int>(mChordBlocks.size())) {
            auto& cb = mChordBlocks[mContextChordIdx];

            ImGui::Text("Chord: %s", cb.name.c_str());
            ImGui::Separator();

            // Rename
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("##renameChord", mRenameChordBuf, sizeof(mRenameChordBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string newName(mRenameChordBuf);
                if (!newName.empty()) {
                    CommandManager::instance().execute(
                        std::make_unique<RenameChordCommand>(
                            mChordBlocks, static_cast<size_t>(mContextChordIdx), newName));
                }
                ImGui::CloseCurrentPopup();
            }

            // Color picker (hue)
            float hue = cb.hue;
            float rgb[3];
            ImGui::ColorConvertHSVtoRGB(hue, 0.7f, 0.8f, rgb[0], rgb[1], rgb[2]);
            if (ImGui::ColorEdit3("Color", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel)) {
                float h, s, v;
                ImGui::ColorConvertRGBtoHSV(rgb[0], rgb[1], rgb[2], h, s, v);
                cb.hue = h;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                CommandManager::instance().execute(
                    std::make_unique<DeleteChordCommand>(
                        mChordBlocks, static_cast<size_t>(mContextChordIdx)));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

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
    // Record — save state BEFORE the button can toggle it
    bool wasRecording = mRecording;
    if (wasRecording) {
        ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.0f, 0.0f, 1.0f});
    }
    if (ImGui::Button("REC", {40, 0})) {
        mRecording = !mRecording;
        if (mRecording) {
            mRecorder = std::make_unique<InputRecorder>();
            mRecorder->start("session.bbfx-session");
            std::cout << "[Timeline] Recording started -> session.bbfx-session" << std::endl;
        } else {
            if (mRecorder) {
                mRecorder->stop();
                mRecorder.reset();
            }
            std::cout << "[Timeline] Recording stopped" << std::endl;
        }
    }
    if (wasRecording) {
        ImGui::PopStyleColor();
    }
    if (mRecording) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "REC"); // recording indicator
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

    constexpr float EDGE_GRAB_WIDTH = 6.0f;

    for (size_t i = 0; i < mChordBlocks.size(); ++i) {
        auto& cb = mChordBlocks[i];
        float x0 = pos.x + cb.startBeat * pixelsPerBeat - mScrollX;
        float x1 = pos.x + cb.endBeat   * pixelsPerBeat - mScrollX;
        float y0 = pos.y + 20.0f;
        float y1 = pos.y + 55.0f;

        // Color from hue
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(cb.hue, 0.7f, 0.8f, r, g, b);
        ImU32 col = IM_COL32(
            static_cast<int>(r * 255), static_cast<int>(g * 255),
            static_cast<int>(b * 255), 140);

        draw->AddRectFilled({x0, y0}, {x1, y1}, col, 3.0f);
        draw->AddRect({x0, y0}, {x1, y1}, IM_COL32(255,255,255,80), 3.0f);
        draw->AddText({x0 + 4, y0 + 4}, IM_COL32(255,255,255,220), cb.name.c_str());

        // ── Left edge resize handle ──────────────────────────────────────────
        ImGui::SetCursorScreenPos({x0 - EDGE_GRAB_WIDTH / 2, y0});
        std::string leftId = "##chordL_" + std::to_string(i);
        ImGui::InvisibleButton(leftId.c_str(), {EDGE_GRAB_WIDTH, y1 - y0});
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActivated()) {
            mResizingBlock = static_cast<int>(i);
            mResizingLeft = true;
            mResizeOldStart = cb.startBeat;
            mResizeOldEnd = cb.endBeat;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / pixelsPerBeat;
            float newStart = std::round(mResizeOldStart + delta);
            if (newStart < 0.0f) newStart = 0.0f;
            if (newStart < cb.endBeat - 1.0f) cb.startBeat = newStart;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            mResizeOldStart = cb.startBeat;
        }
        if (ImGui::IsItemDeactivated() && mResizingBlock == static_cast<int>(i) && mResizingLeft) {
            if (cb.startBeat != mResizeOldStart || cb.endBeat != mResizeOldEnd) {
                CommandManager::instance().execute(
                    std::make_unique<ResizeChordCommand>(
                        mChordBlocks, i, mResizeOldStart, mResizeOldEnd,
                        cb.startBeat, cb.endBeat));
            }
            mResizingBlock = -1;
        }

        // ── Right edge resize handle ─────────────────────────────────────────
        ImGui::SetCursorScreenPos({x1 - EDGE_GRAB_WIDTH / 2, y0});
        std::string rightId = "##chordR_" + std::to_string(i);
        ImGui::InvisibleButton(rightId.c_str(), {EDGE_GRAB_WIDTH, y1 - y0});
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActivated()) {
            mResizingBlock = static_cast<int>(i);
            mResizingLeft = false;
            mResizeOldStart = cb.startBeat;
            mResizeOldEnd = cb.endBeat;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / pixelsPerBeat;
            float newEnd = std::round(mResizeOldEnd + delta);
            if (newEnd > cb.startBeat + 1.0f) cb.endBeat = newEnd;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            mResizeOldEnd = cb.endBeat;
        }
        if (ImGui::IsItemDeactivated() && mResizingBlock == static_cast<int>(i) && !mResizingLeft) {
            if (cb.startBeat != mResizeOldStart || cb.endBeat != mResizeOldEnd) {
                CommandManager::instance().execute(
                    std::make_unique<ResizeChordCommand>(
                        mChordBlocks, i, mResizeOldStart, mResizeOldEnd,
                        cb.startBeat, cb.endBeat));
            }
            mResizingBlock = -1;
        }

        // ── Body drag ────────────────────────────────────────────────────────
        ImGui::SetCursorScreenPos({x0 + EDGE_GRAB_WIDTH, y0});
        float bodyW = std::max(1.0f, (x1 - x0) - 2 * EDGE_GRAB_WIDTH);
        std::string btnId = "##chordblock_" + std::to_string(i);
        ImGui::InvisibleButton(btnId.c_str(), {bodyW, y1 - y0});

        // Right-click context menu
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            mContextChordIdx = static_cast<int>(i);
            std::strncpy(mRenameChordBuf, cb.name.c_str(), sizeof(mRenameChordBuf) - 1);
            mRenameChordBuf[sizeof(mRenameChordBuf) - 1] = '\0';
            ImGui::OpenPopup("ChordContextMenu");
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            mDraggedBlock = static_cast<int>(i);
            float deltaBeat = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / pixelsPerBeat;
            float duration = cb.endBeat - cb.startBeat;

            float newStart = std::round(cb.startBeat + deltaBeat);
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

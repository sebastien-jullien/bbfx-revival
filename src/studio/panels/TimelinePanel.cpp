#include "TimelinePanel.h"
#include "../StudioEngine.h"
#include "../../core/PrimitiveNodes.h"
#include "../../core/Animator.h"
#include "../../audio/AudioAnalyzer.h"
#include "../../audio/BeatDetector.h"
#include "../../record/InputRecorder.h"
#include "../commands/CommandManager.h"
#include "../commands/ChordCommands.h"
#include "../commands/AutomationCommands.h"

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sstream>

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

void TimelinePanel::togglePause() {
    bool wasPaused = mPaused;
    mPaused = !mPaused;
    // On unpause transition, reset mLastTime to avoid time jump
    if (wasPaused && !mPaused) {
        if (auto* t = RootTimeNode::instance()) t->resume();
    }
}

void TimelinePanel::render(StudioEngine* engine) {
    ensureDefaultChords();
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

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

    // Seek: left click (without Ctrl, without Shift) on timeline area
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
        float mouseX = ImGui::GetMousePos().x - canvasPos.x + mScrollX;
        float clickedBeat = mouseX / mPixelsPerBeat;
        if (clickedBeat >= 0.0f) {
            float seekTime = clickedBeat * 60.0f / mBPM;
            if (auto* time = RootTimeNode::instance()) {
                time->seekTo(seekTime);
            }
        }
    }

    // Shift+drag on timeline area = create loop region
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::GetIO().KeyShift) {
        float mouseX = ImGui::GetMousePos().x - canvasPos.x + mScrollX;
        float clickedBeat = mouseX / mPixelsPerBeat;
        mCreatingLoop = true;
        mAutomation.loopRegion.startBeat = std::max(0.0f, clickedBeat);
        mAutomation.loopRegion.endBeat = mAutomation.loopRegion.startBeat;
        mAutomation.loopRegion.active = true;
    }
    if (mCreatingLoop && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float mouseX = ImGui::GetMousePos().x - canvasPos.x + mScrollX;
        float beat = std::max(0.0f, mouseX / mPixelsPerBeat);
        if (beat > mAutomation.loopRegion.startBeat)
            mAutomation.loopRegion.endBeat = beat;
        else {
            mAutomation.loopRegion.endBeat = mAutomation.loopRegion.startBeat;
            mAutomation.loopRegion.startBeat = beat;
        }
    }
    if (mCreatingLoop && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        mCreatingLoop = false;
        if (mAutomation.loopRegion.endBeat - mAutomation.loopRegion.startBeat < 0.5f)
            mAutomation.loopRegion.active = false;  // Too small, cancel
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

            // Chord snapshot
            if (ImGui::MenuItem("Store Snapshot")) {
                cb.snapshot.clear();
                auto* animator = Animator::instance();
                if (animator) {
                    for (auto& lane : mAutomation.lanes) {
                        if (lane.muted || lane.nodeName.empty()) continue;
                        auto* node = animator->getRegisteredNode(lane.nodeName);
                        if (!node) continue;
                        auto& inputs = node->getInputs();
                        auto it = inputs.find(lane.portName);
                        if (it != inputs.end()) {
                            cb.snapshot[lane.nodeName + "." + lane.portName] = it->second->getValue();
                        }
                    }
                }
            }
            if (!cb.snapshot.empty() && ImGui::MenuItem("Recall Snapshot")) {
                auto* animator = Animator::instance();
                if (animator) {
                    for (auto& [key, val] : cb.snapshot) {
                        auto dot = key.find('.');
                        if (dot == std::string::npos) continue;
                        std::string nodeName = key.substr(0, dot);
                        std::string portName = key.substr(dot + 1);
                        auto* node = animator->getRegisteredNode(nodeName);
                        if (!node) continue;
                        auto& inputs = node->getInputs();
                        auto it = inputs.find(portName);
                        if (it != inputs.end()) it->second->setValue(val);
                    }
                }
            }
            ImGui::SetNextItemWidth(80);
            ImGui::InputFloat("Transition", &cb.transitionBeats, 0.5f, 1.0f, "%.1f beats");
            if (cb.transitionBeats < 0.0f) cb.transitionBeats = 0.0f;

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

    // Render overlays: cue markers, loop region
    draw->PushClipRect(canvasPos, {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y}, true);
    renderCueMarkers(mPixelsPerBeat, canvasPos.y, canvasSize.y);
    renderLoopRegion(mPixelsPerBeat, canvasPos.y);
    draw->PopClipRect();

    // Handle keyboard shortcuts for automation
    handleAutomationKeys(currentBeat);

    ImGui::Separator();

    // Automation lanes
    renderAutomationLanes(mPixelsPerBeat, currentBeat);

    ImGui::Separator();
    renderAudioSpectrum();

    // Track previous beat for trigger events
    mPrevBeat = currentBeat;

    ImGui::End();
}

void TimelinePanel::renderTransport(StudioEngine* engine) {
    // Play/Pause
    if (ImGui::Button(mPaused ? ">" : "||", {32, 0})) {
        togglePause();
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
            // Post-record: thin redundant keyframes on armed lanes
            int totalRemoved = 0;
            for (auto& lane : mAutomation.lanes) {
                if (lane.armed && lane.keyframes.size() >= 3) {
                    totalRemoved += AutomationData::thinRedundantKeyframes(lane);
                }
            }
            if (totalRemoved > 0)
                std::cout << "[Timeline] Post-record: removed " << totalRemoved << " redundant keyframes" << std::endl;
            mLastRecordedValues.clear();
            std::cout << "[Timeline] Recording stopped" << std::endl;
        }
    }
    if (wasRecording) {
        ImGui::PopStyleColor();
    }
    if (mRecording) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "REC");
        ImGui::SameLine();
        if (ImGui::SmallButton(mRecordReplace ? "RPL" : "OVR")) {
            mRecordReplace = !mRecordReplace;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(mRecordReplace ? "Replace mode" : "Overdub mode");
    }

    // Loop indicator
    ImGui::SameLine(0, 10);
    if (mAutomation.loopRegion.active) {
        ImGui::TextColored({0.0f, 0.8f, 0.0f, 1.0f}, "LOOP");
    }

    // Lane count
    if (!mAutomation.lanes.empty()) {
        ImGui::SameLine(0, 10);
        int totalKf = 0;
        for (auto& l : mAutomation.lanes) totalKf += static_cast<int>(l.keyframes.size());
        ImGui::TextDisabled("%d lanes, %d kf", static_cast<int>(mAutomation.lanes.size()), totalKf);
    }
}

void TimelinePanel::renderBPMDisplay() {
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputFloat("BPM", &mBPM, 1.0f, 5.0f, "%.1f");
    mBPM = std::clamp(mBPM, 20.0f, 1200.0f);
}

void TimelinePanel::changeBPM(float delta) {
    mBPM = std::clamp(mBPM + delta, 20.0f, 1200.0f);
    if (auto* time = RootTimeNode::instance())
        time->setBPM(mBPM);
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
        // Snapshot indicator
        if (!cb.snapshot.empty()) {
            draw->AddText({x1 - 14, y0 + 4}, IM_COL32(255, 220, 50, 220), "S");
        }

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

// ── Cue Markers ─────────────────────────────────────────────────────────────

void TimelinePanel::renderCueMarkers(float pixelsPerBeat, float canvasTop, float canvasHeight) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 wPos = ImGui::GetWindowPos();

    for (int i = 0; i < static_cast<int>(mAutomation.cueMarkers.size()); ++i) {
        auto& cue = mAutomation.cueMarkers[i];
        float x = wPos.x + cue.beat * pixelsPerBeat - mScrollX + 80.0f; // offset for header
        ImU32 col = (i == mSelectedCue) ? IM_COL32(255, 220, 50, 255) : IM_COL32(255, 200, 0, 180);

        // Dashed vertical line
        for (float y = canvasTop; y < canvasTop + canvasHeight; y += 6.0f) {
            draw->AddLine({x, y}, {x, std::min(y + 3.0f, canvasTop + canvasHeight)}, col, 1.0f);
        }

        // Triangle marker at top
        draw->AddTriangleFilled({x - 5, canvasTop}, {x + 5, canvasTop}, {x, canvasTop + 8}, col);

        // Label
        draw->AddText({x + 4, canvasTop + 1}, col, cue.name.c_str());
    }
}

// ── Loop Region ─────────────────────────────────────────────────────────────

void TimelinePanel::renderLoopRegion(float pixelsPerBeat, float canvasTop) {
    if (!mAutomation.loopRegion.active) return;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 wPos = ImGui::GetWindowPos();
    float headerW = 80.0f;

    float x0 = wPos.x + mAutomation.loopRegion.startBeat * pixelsPerBeat - mScrollX + headerW;
    float x1 = wPos.x + mAutomation.loopRegion.endBeat * pixelsPerBeat - mScrollX + headerW;

    // Green overlay
    draw->AddRectFilled({x0, canvasTop}, {x1, canvasTop + 16.0f}, IM_COL32(0, 200, 0, 60));
    draw->AddRect({x0, canvasTop}, {x1, canvasTop + 16.0f}, IM_COL32(0, 200, 0, 150), 0, 0, 1.0f);
}

// ── Automation Lanes ────────────────────────────────────────────────────────

void TimelinePanel::renderAutomationLanes(float pixelsPerBeat, float currentBeat) {
    auto* animator = Animator::instance();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    float headerW = 80.0f;
    float collapsedH = 24.0f * mLaneZoomY;
    float expandedH = 80.0f * mLaneZoomY;

    // Calculate total height
    float totalH = 0.0f;
    for (auto& lane : mAutomation.lanes) {
        totalH += lane.collapsed ? collapsedH : expandedH;
    }

    float availH = std::min(totalH + 30.0f, 300.0f); // Max 300px, or less if fewer lanes
    if (mAutomation.lanes.empty()) availH = 30.0f;

    // Begin child for scrolling
    ImGui::BeginChild("##AutoLanes", {0, availH}, false, ImGuiWindowFlags_NoScrollbar);

    ImVec2 regionPos = ImGui::GetCursorScreenPos();
    ImVec2 regionSize = ImGui::GetContentRegionAvail();

    // Add Lane button
    if (ImGui::SmallButton("+Lane")) {
        AutomationLane newLane;
        newLane.id = "lane_" + std::to_string(mAutomation.lanes.size());
        newLane.displayName = "Unassigned";
        newLane.hue = std::fmod(mAutomation.lanes.size() * 0.125f, 1.0f);
        CommandManager::instance().execute(
            std::make_unique<AddLaneCommand>(&mAutomation, newLane));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add automation lane");

    // Vertical scroll with mouse wheel
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (ImGui::GetIO().KeyCtrl && wheel != 0.0f) {
            // Ctrl+wheel = zoom lanes vertically
            mLaneZoomY *= (1.0f + wheel * 0.1f);
            mLaneZoomY = std::clamp(mLaneZoomY, 0.3f, 2.5f);
        } else if (wheel != 0.0f) {
            mLaneScrollY -= wheel * 20.0f;
            mLaneScrollY = std::clamp(mLaneScrollY, 0.0f, std::max(0.0f, totalH - availH));
        }
    }

    float curY = regionPos.y - mLaneScrollY;

    for (int li = 0; li < static_cast<int>(mAutomation.lanes.size()); ++li) {
        auto& lane = mAutomation.lanes[li];
        float laneH = lane.collapsed ? collapsedH : expandedH;
        float laneTop = curY;
        float laneBot = curY + laneH;
        curY = laneBot;

        // Skip if not visible
        if (laneBot < regionPos.y || laneTop > regionPos.y + regionSize.y) continue;

        // Lane background
        ImU32 bgCol = (li == mSelectedLane) ? IM_COL32(60, 60, 80, 200) : IM_COL32(40, 40, 50, 200);
        if (lane.armed) bgCol = IM_COL32(80, 30, 30, 200);
        draw->AddRectFilled({regionPos.x, laneTop}, {regionPos.x + regionSize.x, laneBot}, bgCol);
        draw->AddLine({regionPos.x, laneBot}, {regionPos.x + regionSize.x, laneBot}, IM_COL32(80, 80, 80, 150));

        // Header area (left side)
        ImGui::SetCursorScreenPos({regionPos.x + 2, laneTop + 2});

        // Mute button
        std::string muteId = "M##mute" + std::to_string(li);
        bool wasMuted = lane.muted;
        if (wasMuted) ImGui::PushStyleColor(ImGuiCol_Button, {0.5f, 0.5f, 0.5f, 1.0f});
        if (ImGui::SmallButton(muteId.c_str())) lane.muted = !lane.muted;
        if (wasMuted) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute lane");

        ImGui::SameLine();

        // Arm button (for recording)
        std::string armId = "R##arm" + std::to_string(li);
        bool wasArmed = lane.armed;
        if (wasArmed) ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.0f, 0.0f, 1.0f});
        if (ImGui::SmallButton(armId.c_str())) lane.armed = !lane.armed;
        if (wasArmed) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Arm for recording");

        ImGui::SameLine();

        // Collapse toggle
        std::string colId = (lane.collapsed ? ">##col" : "v##col") + std::to_string(li);
        if (ImGui::SmallButton(colId.c_str())) lane.collapsed = !lane.collapsed;

        // Display name (clickable for selection + port assignment)
        ImGui::SetCursorScreenPos({regionPos.x + 2, laneTop + 16});
        std::string nameId = "##lanename" + std::to_string(li);
        ImGui::SetNextItemWidth(headerW - 6);
        if (ImGui::Selectable(lane.displayName.c_str(), li == mSelectedLane, 0, {headerW - 6, 14})) {
            mSelectedLane = li;
        }

        // Right-click on lane header = context menu
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            mSelectedLane = li;
            ImGui::OpenPopup(("##laneCtx" + std::to_string(li)).c_str());
        }

        // Lane context menu
        if (ImGui::BeginPopup(("##laneCtx" + std::to_string(li)).c_str())) {
            // Port assignment submenu
            if (ImGui::BeginMenu("Assign Port")) {
                if (animator) {
                    for (auto& nodeName : animator->getRegisteredNodeNames()) {
                        if (nodeName == "time") continue; // Skip RootTimeNode
                        auto* node = animator->getRegisteredNode(nodeName);
                        if (!node) continue;
                        if (ImGui::BeginMenu(nodeName.c_str())) {
                            for (auto& [portName, port] : node->getInputs()) {
                                if (ImGui::MenuItem(portName.c_str())) {
                                    float pMin = 0.0f, pMax = 1.0f;
                                    if (auto* spec = node->getParamSpec()) {
                                        auto* pd = spec->getParam(portName);
                                        if (pd) { pMin = pd->minVal; pMax = pd->maxVal; }
                                    }
                                    std::string disp = nodeName + " / " + portName;
                                    CommandManager::instance().execute(
                                        std::make_unique<AssignLanePortCommand>(
                                            &mAutomation, li, nodeName, portName, disp, pMin, pMax));
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Generate LFO...")) {
                mShowLFOPopup = true;
                mLFOLane = li;
                mLFOStartBeat = currentBeat;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete Lane")) {
                CommandManager::instance().execute(
                    std::make_unique<DeleteLaneCommand>(&mAutomation, li));
                ImGui::EndPopup();
                continue;
            }
            ImGui::EndPopup();
        }

        // ── Draw curves and keyframes (only if expanded) ────────────────────
        if (!lane.collapsed && !lane.keyframes.empty()) {
            float graphL = regionPos.x + headerW;
            float graphR = regionPos.x + regionSize.x;
            float graphT = laneTop + 2;
            float graphB = laneBot - 2;
            float graphH = graphB - graphT;
            float valRange = lane.maxVal - lane.minVal;
            if (valRange <= 0.0f) valRange = 1.0f;

            auto beatToX = [&](float beat) -> float {
                return graphL + beat * pixelsPerBeat - mScrollX;
            };
            auto valToY = [&](float val) -> float {
                return graphB - ((val - lane.minVal) / valRange) * graphH;
            };
            auto yToVal = [&](float y) -> float {
                return lane.minVal + (1.0f - (y - graphT) / graphH) * valRange;
            };

            // Draw value grid lines
            for (int g = 0; g <= 4; ++g) {
                float gy = graphT + g * graphH / 4.0f;
                draw->AddLine({graphL, gy}, {graphR, gy}, IM_COL32(60, 60, 60, 100));
            }

            // Get lane color
            float cr, cg, cb;
            ImGui::ColorConvertHSVtoRGB(lane.hue, 0.5f, 0.8f, cr, cg, cb);
            ImU32 curveCol = IM_COL32((int)(cr*255), (int)(cg*255), (int)(cb*255), 180);
            ImGui::ColorConvertHSVtoRGB(lane.hue, 0.8f, 1.0f, cr, cg, cb);
            ImU32 kfCol = IM_COL32((int)(cr*255), (int)(cg*255), (int)(cb*255), 255);

            // Draw curves between keyframes
            const auto& kfs = lane.keyframes;

            // Extend left (before first keyframe)
            if (!kfs.empty()) {
                float x0 = beatToX(0);
                float x1 = beatToX(kfs.front().beat);
                float y = valToY(kfs.front().value);
                if (x1 > graphL) draw->AddLine({std::max(x0, graphL), y}, {x1, y}, curveCol, 1.5f);
            }

            for (int ki = 0; ki + 1 < static_cast<int>(kfs.size()); ++ki) {
                float x0 = beatToX(kfs[ki].beat);
                float x1 = beatToX(kfs[ki + 1].beat);
                float y0 = valToY(kfs[ki].value);
                float y1 = valToY(kfs[ki + 1].value);

                if (x1 < graphL || x0 > graphR) continue; // Off-screen

                InterpolationMode mode = kfs[ki].mode;
                if (mode == InterpolationMode::Linear) {
                    draw->AddLine({x0, y0}, {x1, y1}, curveCol, 1.5f);
                } else if (mode == InterpolationMode::Step) {
                    draw->AddLine({x0, y0}, {x1, y0}, curveCol, 1.5f);
                    draw->AddLine({x1, y0}, {x1, y1}, curveCol, 1.5f);
                } else {
                    // Smooth/EaseIn/EaseOut/Bezier: sample 16 points
                    ImVec2 points[17];
                    for (int s = 0; s <= 16; ++s) {
                        float t = s / 16.0f;
                        float beat = kfs[ki].beat + t * (kfs[ki+1].beat - kfs[ki].beat);
                        float val = mAutomation.evaluate(lane, beat);
                        points[s] = {beatToX(beat), valToY(val)};
                    }
                    draw->AddPolyline(points, 17, curveCol, ImDrawFlags_None, 1.5f);
                }
            }

            // Extend right (after last keyframe)
            if (!kfs.empty()) {
                float x0 = beatToX(kfs.back().beat);
                float y = valToY(kfs.back().value);
                if (x0 < graphR) draw->AddLine({x0, y}, {graphR, y}, curveCol, 1.5f);
            }

            // Draw keyframes (diamonds)
            for (int ki = 0; ki < static_cast<int>(kfs.size()); ++ki) {
                float cx = beatToX(kfs[ki].beat);
                float cy = valToY(kfs[ki].value);
                if (cx < graphL - 10 || cx > graphR + 10) continue;

                bool selected = mSelectedKeyframes.count({li, ki}) > 0;
                float sz = 5.0f;

                ImVec2 diamond[4] = {
                    {cx, cy - sz}, {cx + sz, cy}, {cx, cy + sz}, {cx - sz, cy}
                };
                draw->AddConvexPolyFilled(diamond, 4, selected ? IM_COL32(255, 255, 255, 255) : kfCol);
                if (selected) {
                    draw->AddPolyline(diamond, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 2.0f);
                }

                // Bezier tangent handles (render + drag when selected and in bezier mode)
                if (selected && kfs[ki].mode == InterpolationMode::Bezier) {
                    float valRange = lane.maxVal - lane.minVal;
                    if (valRange <= 0) valRange = 1.0f;
                    float graphH = graphB - graphT;

                    // Out handle (right side)
                    float hOutX = cx + kfs[ki].tangentOutBeat * pixelsPerBeat;
                    float hOutY = cy - kfs[ki].tangentOutValue / valRange * graphH;
                    draw->AddLine({cx, cy}, {hOutX, hOutY}, IM_COL32(180, 180, 180, 200), 1.0f);
                    draw->AddCircleFilled({hOutX, hOutY}, 4.0f, IM_COL32(255, 255, 255, 220));

                    // In handle (left side)
                    float hInX = cx + kfs[ki].tangentInBeat * pixelsPerBeat;
                    float hInY = cy - kfs[ki].tangentInValue / valRange * graphH;
                    draw->AddLine({cx, cy}, {hInX, hInY}, IM_COL32(180, 180, 180, 200), 1.0f);
                    draw->AddCircleFilled({hInX, hInY}, 4.0f, IM_COL32(255, 255, 255, 220));

                    // Hit-test on handles for drag start
                    if (!mDraggingTangent && !mDraggingKeyframe && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        ImVec2 mp = ImGui::GetMousePos();
                        float dxOut = mp.x - hOutX, dyOut = mp.y - hOutY;
                        float dxIn = mp.x - hInX, dyIn = mp.y - hInY;

                        if (dxOut*dxOut + dyOut*dyOut < 36.0f) { // 6px radius
                            mDraggingTangent = true;
                            mDragTangentIsOut = true;
                            mDragTangentLane = li;
                            mDragTangentKf = ki;
                            mDragTangentOldInB = kfs[ki].tangentInBeat;
                            mDragTangentOldInV = kfs[ki].tangentInValue;
                            mDragTangentOldOutB = kfs[ki].tangentOutBeat;
                            mDragTangentOldOutV = kfs[ki].tangentOutValue;
                        } else if (dxIn*dxIn + dyIn*dyIn < 36.0f) {
                            mDraggingTangent = true;
                            mDragTangentIsOut = false;
                            mDragTangentLane = li;
                            mDragTangentKf = ki;
                            mDragTangentOldInB = kfs[ki].tangentInBeat;
                            mDragTangentOldInV = kfs[ki].tangentInValue;
                            mDragTangentOldOutB = kfs[ki].tangentOutBeat;
                            mDragTangentOldOutV = kfs[ki].tangentOutValue;
                        }
                    }
                }

                // Tooltip on hover
                ImVec2 mousePos = ImGui::GetMousePos();
                float dx = mousePos.x - cx, dy = mousePos.y - cy;
                if (dx*dx + dy*dy < 64.0f) { // within 8px radius
                    ImGui::SetTooltip("Beat: %.2f  Value: %.3f\n%s",
                        kfs[ki].beat, kfs[ki].value,
                        interpolationModeToString(kfs[ki].mode));

                    // Click to select
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (!ImGui::GetIO().KeyShift) mSelectedKeyframes.clear();
                        auto key = std::make_pair(li, ki);
                        if (mSelectedKeyframes.count(key))
                            mSelectedKeyframes.erase(key);
                        else
                            mSelectedKeyframes.insert(key);
                        mSelectedLane = li;

                        // Start drag
                        mDraggingKeyframe = true;
                        mDragKfLane = li;
                        mDragKfIdx = ki;
                        mDragKfOldBeat = kfs[ki].beat;
                        mDragKfOldValue = kfs[ki].value;
                    }

                    // Right-click context menu
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        mSelectedLane = li;
                        ImGui::OpenPopup("##kfCtx");
                        mEditKfLane = li;
                        mEditKfIdx = ki;
                    }

                    // Double-click to edit
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        mShowEditKfPopup = true;
                        mEditKfLane = li;
                        mEditKfIdx = ki;
                        mEditKfBeat = kfs[ki].beat;
                        mEditKfValue = kfs[ki].value;
                        mEditKfMode = static_cast<int>(kfs[ki].mode);
                    }
                }
            }

            // Box select: start on single click in empty lane area (not on keyframe, not shift)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mDraggingKeyframe && !ImGui::GetIO().KeyShift) {
                ImVec2 mp = ImGui::GetMousePos();
                if (mp.x > graphL && mp.x < graphR && mp.y > graphT && mp.y < graphB) {
                    bool onKf = false;
                    for (int ki = 0; ki < static_cast<int>(kfs.size()); ++ki) {
                        float cx = beatToX(kfs[ki].beat), cy = valToY(kfs[ki].value);
                        float ddx = mp.x - cx, ddy = mp.y - cy;
                        if (ddx*ddx + ddy*ddy < 64.0f) { onKf = true; break; }
                    }
                    if (!onKf && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        mBoxSelecting = true;
                        mBoxSelectStart = mp;
                        mSelectedKeyframes.clear();
                        mSelectedLane = li;
                    }
                }
            }

            // Box select: update and render rubber band
            if (mBoxSelecting && mSelectedLane == li && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 mp = ImGui::GetMousePos();
                float rx0 = std::min(mBoxSelectStart.x, mp.x), ry0 = std::min(mBoxSelectStart.y, mp.y);
                float rx1 = std::max(mBoxSelectStart.x, mp.x), ry1 = std::max(mBoxSelectStart.y, mp.y);
                draw->AddRectFilled({rx0, ry0}, {rx1, ry1}, IM_COL32(100, 150, 255, 40));
                draw->AddRect({rx0, ry0}, {rx1, ry1}, IM_COL32(100, 150, 255, 180), 0, 0, 1.0f);

                // Select keyframes inside the rectangle
                mSelectedKeyframes.clear();
                for (int ki = 0; ki < static_cast<int>(kfs.size()); ++ki) {
                    float cx = beatToX(kfs[ki].beat), cy = valToY(kfs[ki].value);
                    if (cx >= rx0 && cx <= rx1 && cy >= ry0 && cy <= ry1) {
                        mSelectedKeyframes.insert({li, ki});
                    }
                }
            }
            if (mBoxSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                mBoxSelecting = false;
            }

            // Double-click in empty lane area = create keyframe
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x > graphL && mousePos.x < graphR &&
                    mousePos.y > graphT && mousePos.y < graphB) {
                    // Check we're not on a keyframe
                    bool onKf = false;
                    for (int ki = 0; ki < static_cast<int>(kfs.size()); ++ki) {
                        float cx = beatToX(kfs[ki].beat);
                        float cy = valToY(kfs[ki].value);
                        float dx = mousePos.x - cx, dy = mousePos.y - cy;
                        if (dx*dx + dy*dy < 64.0f) { onKf = true; break; }
                    }
                    if (!onKf) {
                        float beat = (mousePos.x - graphL + mScrollX) / pixelsPerBeat;
                        float val = yToVal(mousePos.y);
                        val = std::clamp(val, lane.minVal, lane.maxVal);
                        beat = std::max(0.0f, beat);
                        Keyframe kf;
                        kf.beat = beat;
                        kf.value = val;
                        CommandManager::instance().execute(
                            std::make_unique<AddKeyframeCommand>(&mAutomation, li, kf));
                    }
                }
            }
        } else if (!lane.collapsed && lane.keyframes.empty()) {
            // Empty lane - show hint
            float graphL = regionPos.x + headerW;
            ImGui::SetCursorScreenPos({graphL + 10, laneTop + laneH/2 - 7});
            ImGui::TextDisabled("Double-click to add keyframe");

            // Double-click creates first keyframe
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ImVec2 mp = ImGui::GetMousePos();
                float graphR = regionPos.x + regionSize.x;
                float graphT = laneTop + 2, graphB = laneBot - 2;
                if (mp.x > graphL && mp.x < graphR && mp.y > graphT && mp.y < graphB) {
                    float beat = (mp.x - graphL + mScrollX) / pixelsPerBeat;
                    float graphH = graphB - graphT;
                    float valRange = lane.maxVal - lane.minVal;
                    if (valRange <= 0) valRange = 1;
                    float val = lane.minVal + (1.0f - (mp.y - graphT) / graphH) * valRange;
                    val = std::clamp(val, lane.minVal, lane.maxVal);
                    Keyframe kf; kf.beat = std::max(0.0f, beat); kf.value = val;
                    CommandManager::instance().execute(
                        std::make_unique<AddKeyframeCommand>(&mAutomation, li, kf));
                }
            }
        }
    }

    // Keyframe context menu
    if (ImGui::BeginPopup("##kfCtx")) {
        if (mEditKfLane >= 0 && mEditKfLane < static_cast<int>(mAutomation.lanes.size()) &&
            mEditKfIdx >= 0 && mEditKfIdx < static_cast<int>(mAutomation.lanes[mEditKfLane].keyframes.size())) {

            auto& kf = mAutomation.lanes[mEditKfLane].keyframes[mEditKfIdx];
            if (ImGui::BeginMenu("Interpolation")) {
                const char* names[] = {"Step", "Linear", "Smooth", "EaseIn", "EaseOut", "Bezier"};
                for (int m = 0; m < 6; ++m) {
                    bool current = static_cast<int>(kf.mode) == m;
                    if (ImGui::MenuItem(names[m], nullptr, current)) {
                        CommandManager::instance().execute(
                            std::make_unique<SetInterpolationModeCommand>(
                                &mAutomation, mEditKfLane, mEditKfIdx,
                                static_cast<InterpolationMode>(m)));
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Delete Keyframe")) {
                CommandManager::instance().execute(
                    std::make_unique<DeleteKeyframeCommand>(&mAutomation, mEditKfLane, mEditKfIdx));
                mSelectedKeyframes.erase({mEditKfLane, mEditKfIdx});
            }
        }
        ImGui::EndPopup();
    }

    // Edit keyframe popup
    if (mShowEditKfPopup) {
        ImGui::OpenPopup("Edit Keyframe");
        mShowEditKfPopup = false;
    }
    if (ImGui::BeginPopup("Edit Keyframe")) {
        ImGui::Text("Edit Keyframe");
        ImGui::Separator();
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("Beat", &mEditKfBeat, 0.25f, 1.0f, "%.2f");
        if (mEditKfBeat < 0) mEditKfBeat = 0;
        ImGui::SetNextItemWidth(100);
        if (mEditKfLane >= 0 && mEditKfLane < static_cast<int>(mAutomation.lanes.size())) {
            auto& lane = mAutomation.lanes[mEditKfLane];
            ImGui::SliderFloat("Value", &mEditKfValue, lane.minVal, lane.maxVal, "%.3f");
        }
        const char* modes[] = {"Step", "Linear", "Smooth", "EaseIn", "EaseOut", "Bezier"};
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("Mode", &mEditKfMode, modes, 6);

        if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (mEditKfLane >= 0 && mEditKfIdx >= 0) {
                auto& kf = mAutomation.lanes[mEditKfLane].keyframes[mEditKfIdx];
                float oldBeat = kf.beat, oldVal = kf.value;
                auto oldMode = kf.mode;
                if (oldBeat != mEditKfBeat || oldVal != mEditKfValue) {
                    CommandManager::instance().execute(
                        std::make_unique<MoveKeyframeCommand>(
                            &mAutomation, mEditKfLane, mEditKfIdx,
                            oldBeat, oldVal, mEditKfBeat, mEditKfValue));
                }
                if (static_cast<int>(oldMode) != mEditKfMode) {
                    CommandManager::instance().execute(
                        std::make_unique<SetInterpolationModeCommand>(
                            &mAutomation, mEditKfLane, mEditKfIdx,
                            static_cast<InterpolationMode>(mEditKfMode)));
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // LFO generation popup
    if (mShowLFOPopup) {
        ImGui::OpenPopup("Generate LFO");
        mShowLFOPopup = false;
    }
    if (ImGui::BeginPopup("Generate LFO")) {
        ImGui::Text("Generate LFO Keyframes");
        ImGui::Separator();
        const char* waves[] = {"Sine", "Square", "Triangle", "Sawtooth"};
        ImGui::Combo("Waveform", &mLFOWaveform, waves, 4);
        ImGui::InputFloat("Frequency (beats)", &mLFOFreq, 0.5f, 1.0f, "%.1f");
        ImGui::InputFloat("Amplitude", &mLFOAmplitude, 0.1f, 0.5f, "%.2f");
        ImGui::InputFloat("Offset", &mLFOOffset, 0.1f, 0.5f, "%.2f");
        ImGui::InputFloat("Start Beat", &mLFOStartBeat, 1.0f, 4.0f, "%.1f");
        ImGui::InputInt("Cycles", &mLFOCycles);
        if (mLFOCycles < 1) mLFOCycles = 1;
        if (mLFOFreq < 0.25f) mLFOFreq = 0.25f;

        if (ImGui::Button("Generate") && mLFOLane >= 0 &&
            mLFOLane < static_cast<int>(mAutomation.lanes.size())) {
            auto& lane = mAutomation.lanes[mLFOLane];
            int pointsPerCycle = (mLFOWaveform == 0) ? 8 : (mLFOWaveform == 1) ? 2 : 4;
            for (int c = 0; c < mLFOCycles; ++c) {
                for (int p = 0; p < pointsPerCycle; ++p) {
                    float t = static_cast<float>(p) / pointsPerCycle;
                    float beat = mLFOStartBeat + (c + t) * mLFOFreq;
                    float val = 0.0f;
                    switch (mLFOWaveform) {
                        case 0: val = std::sin(t * 2.0f * 3.14159265f); break; // Sine
                        case 1: val = (p == 0) ? 1.0f : -1.0f; break; // Square
                        case 2: // Triangle
                            if (t < 0.25f) val = t * 4.0f;
                            else if (t < 0.75f) val = 1.0f - (t - 0.25f) * 4.0f;
                            else val = -1.0f + (t - 0.75f) * 4.0f;
                            break;
                        case 3: val = t * 2.0f - 1.0f; break; // Sawtooth
                    }
                    Keyframe kf;
                    kf.beat = beat;
                    kf.value = std::clamp(mLFOOffset + val * mLFOAmplitude, lane.minVal, lane.maxVal);
                    kf.mode = (mLFOWaveform == 0) ? InterpolationMode::Smooth :
                              (mLFOWaveform == 1) ? InterpolationMode::Step : InterpolationMode::Linear;
                    CommandManager::instance().execute(
                        std::make_unique<AddKeyframeCommand>(&mAutomation, mLFOLane, kf));
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Handle keyframe dragging
    if (mDraggingKeyframe && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (mDragKfLane >= 0 && mDragKfLane < static_cast<int>(mAutomation.lanes.size()) &&
            mDragKfIdx >= 0 && mDragKfIdx < static_cast<int>(mAutomation.lanes[mDragKfLane].keyframes.size())) {
            auto& lane = mAutomation.lanes[mDragKfLane];
            auto& kf = lane.keyframes[mDragKfIdx];
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            float beatDelta = delta.x / pixelsPerBeat;
            float valRange = lane.maxVal - lane.minVal;
            if (valRange <= 0) valRange = 1;
            float laneH = expandedH - 4;
            float valDelta = -(delta.y / laneH) * valRange;

            kf.beat = std::max(0.0f, mDragKfOldBeat + beatDelta);
            kf.value = std::clamp(mDragKfOldValue + valDelta, lane.minVal, lane.maxVal);
        }
    }
    if (mDraggingKeyframe && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (mDragKfLane >= 0 && mDragKfLane < static_cast<int>(mAutomation.lanes.size()) &&
            mDragKfIdx >= 0 && mDragKfIdx < static_cast<int>(mAutomation.lanes[mDragKfLane].keyframes.size())) {
            auto& kf = mAutomation.lanes[mDragKfLane].keyframes[mDragKfIdx];
            if (kf.beat != mDragKfOldBeat || kf.value != mDragKfOldValue) {
                float newBeat = kf.beat, newVal = kf.value;
                kf.beat = mDragKfOldBeat;
                kf.value = mDragKfOldValue;
                CommandManager::instance().execute(
                    std::make_unique<MoveKeyframeCommand>(
                        &mAutomation, mDragKfLane, mDragKfIdx,
                        mDragKfOldBeat, mDragKfOldValue, newBeat, newVal));
            }
        }
        mDraggingKeyframe = false;
    }

    // Handle tangent handle dragging
    if (mDraggingTangent && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (mDragTangentLane >= 0 && mDragTangentLane < static_cast<int>(mAutomation.lanes.size()) &&
            mDragTangentKf >= 0 && mDragTangentKf < static_cast<int>(mAutomation.lanes[mDragTangentLane].keyframes.size())) {
            auto& lane = mAutomation.lanes[mDragTangentLane];
            auto& kf = lane.keyframes[mDragTangentKf];
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            float deltaBeat = delta.x / pixelsPerBeat;
            float valRange = lane.maxVal - lane.minVal;
            if (valRange <= 0) valRange = 1.0f;
            float gH = expandedH - 4.0f;
            float deltaValue = -(delta.y / gH) * valRange;

            if (mDragTangentIsOut) {
                kf.tangentOutBeat = mDragTangentOldOutB + deltaBeat;
                kf.tangentOutValue = mDragTangentOldOutV + deltaValue;
            } else {
                kf.tangentInBeat = mDragTangentOldInB + deltaBeat;
                kf.tangentInValue = mDragTangentOldInV + deltaValue;
            }
        }
    }
    if (mDraggingTangent && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (mDragTangentLane >= 0 && mDragTangentLane < static_cast<int>(mAutomation.lanes.size()) &&
            mDragTangentKf >= 0 && mDragTangentKf < static_cast<int>(mAutomation.lanes[mDragTangentLane].keyframes.size())) {
            auto& kf = mAutomation.lanes[mDragTangentLane].keyframes[mDragTangentKf];
            // Restore old values and emit command
            float newInB = kf.tangentInBeat, newInV = kf.tangentInValue;
            float newOutB = kf.tangentOutBeat, newOutV = kf.tangentOutValue;
            kf.tangentInBeat = mDragTangentOldInB; kf.tangentInValue = mDragTangentOldInV;
            kf.tangentOutBeat = mDragTangentOldOutB; kf.tangentOutValue = mDragTangentOldOutV;
            CommandManager::instance().execute(
                std::make_unique<SetTangentCommand>(
                    &mAutomation, mDragTangentLane, mDragTangentKf,
                    newInB, newInV, newOutB, newOutV));
        }
        mDraggingTangent = false;
    }

    ImGui::EndChild();
}

// ── Keyboard shortcuts for automation ───────────────────────────────────────

void TimelinePanel::handleAutomationKeys(float currentBeat) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    // M = Add cue marker at playhead
    if (ImGui::IsKeyPressed(ImGuiKey_M) && !io.KeyCtrl) {
        CueMarker cue;
        cue.beat = currentBeat;
        cue.name = "Cue " + std::to_string(mCueCounter++);
        CommandManager::instance().execute(
            std::make_unique<AddCueMarkerCommand>(&mAutomation, cue));
    }

    // L = Toggle loop
    if (ImGui::IsKeyPressed(ImGuiKey_L) && !io.KeyCtrl) {
        mAutomation.loopRegion.active = !mAutomation.loopRegion.active;
    }

    // Delete = delete selected keyframes or cue
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (mSelectedCue >= 0 && mSelectedCue < static_cast<int>(mAutomation.cueMarkers.size())) {
            CommandManager::instance().execute(
                std::make_unique<DeleteCueMarkerCommand>(&mAutomation, mSelectedCue));
            mSelectedCue = -1;
        } else if (!mSelectedKeyframes.empty()) {
            // Delete in reverse order to maintain indices
            std::vector<std::pair<int,int>> sorted(mSelectedKeyframes.begin(), mSelectedKeyframes.end());
            std::sort(sorted.rbegin(), sorted.rend());
            for (auto& [lane, kf] : sorted) {
                if (lane < static_cast<int>(mAutomation.lanes.size()) &&
                    kf < static_cast<int>(mAutomation.lanes[lane].keyframes.size())) {
                    CommandManager::instance().execute(
                        std::make_unique<DeleteKeyframeCommand>(&mAutomation, lane, kf));
                }
            }
            mSelectedKeyframes.clear();
        }
    }

    // Ctrl+Left/Right = navigate cues
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        // Find cue before currentBeat
        float bestBeat = -1.0f;
        int bestIdx = -1;
        for (int i = 0; i < static_cast<int>(mAutomation.cueMarkers.size()); ++i) {
            if (mAutomation.cueMarkers[i].beat < currentBeat - 0.1f) {
                if (mAutomation.cueMarkers[i].beat > bestBeat) {
                    bestBeat = mAutomation.cueMarkers[i].beat;
                    bestIdx = i;
                }
            }
        }
        if (bestIdx >= 0) {
            float seekTime = bestBeat * 60.0f / mBPM;
            if (auto* time = RootTimeNode::instance()) time->seekTo(seekTime);
            mSelectedCue = bestIdx;
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        // Find cue after currentBeat
        float bestBeat = 1e9f;
        int bestIdx = -1;
        for (int i = 0; i < static_cast<int>(mAutomation.cueMarkers.size()); ++i) {
            if (mAutomation.cueMarkers[i].beat > currentBeat + 0.1f) {
                if (mAutomation.cueMarkers[i].beat < bestBeat) {
                    bestBeat = mAutomation.cueMarkers[i].beat;
                    bestIdx = i;
                }
            }
        }
        if (bestIdx >= 0) {
            float seekTime = bestBeat * 60.0f / mBPM;
            if (auto* time = RootTimeNode::instance()) time->seekTo(seekTime);
            mSelectedCue = bestIdx;
        }
    }

    // Ctrl+Q = Quantize
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
        ImGui::OpenPopup("Quantize");
    }
    if (ImGui::BeginPopup("Quantize")) {
        ImGui::Text("Quantize Keyframes");
        ImGui::Separator();
        float grids[] = {1.0f, 0.5f, 0.25f, 4.0f};
        const char* labels[] = {"1 Beat", "1/2 Beat", "1/4 Beat", "1 Bar (4 beats)"};
        for (int g = 0; g < 4; ++g) {
            if (ImGui::MenuItem(labels[g])) {
                float grid = grids[g];
                if (!mSelectedKeyframes.empty()) {
                    for (auto& [li, ki] : mSelectedKeyframes) {
                        if (li < static_cast<int>(mAutomation.lanes.size()) &&
                            ki < static_cast<int>(mAutomation.lanes[li].keyframes.size())) {
                            auto& kf = mAutomation.lanes[li].keyframes[ki];
                            float newBeat = std::round(kf.beat / grid) * grid;
                            if (newBeat != kf.beat) {
                                CommandManager::instance().execute(
                                    std::make_unique<MoveKeyframeCommand>(
                                        &mAutomation, li, ki,
                                        kf.beat, kf.value, newBeat, kf.value));
                            }
                        }
                    }
                } else if (mSelectedLane >= 0 && mSelectedLane < static_cast<int>(mAutomation.lanes.size())) {
                    auto& kfs = mAutomation.lanes[mSelectedLane].keyframes;
                    for (int ki = 0; ki < static_cast<int>(kfs.size()); ++ki) {
                        float newBeat = std::round(kfs[ki].beat / grid) * grid;
                        if (newBeat != kfs[ki].beat) {
                            CommandManager::instance().execute(
                                std::make_unique<MoveKeyframeCommand>(
                                    &mAutomation, mSelectedLane, ki,
                                    kfs[ki].beat, kfs[ki].value, newBeat, kfs[ki].value));
                        }
                    }
                }
            }
        }
        ImGui::EndPopup();
    }

    // Ctrl+C = Copy keyframes
    // Ctrl+V = Paste keyframes
    // (simplified: store in static vector)
    static std::vector<Keyframe> sClipboard;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !mSelectedKeyframes.empty()) {
        sClipboard.clear();
        float minBeat = 1e9f;
        for (auto& [li, ki] : mSelectedKeyframes) {
            if (li < static_cast<int>(mAutomation.lanes.size()) &&
                ki < static_cast<int>(mAutomation.lanes[li].keyframes.size())) {
                auto kf = mAutomation.lanes[li].keyframes[ki];
                if (kf.beat < minBeat) minBeat = kf.beat;
                sClipboard.push_back(kf);
            }
        }
        // Relativize beats
        for (auto& kf : sClipboard) kf.beat -= minBeat;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !sClipboard.empty() &&
        mSelectedLane >= 0 && mSelectedLane < static_cast<int>(mAutomation.lanes.size())) {
        for (auto kf : sClipboard) {
            kf.beat += currentBeat;
            CommandManager::instance().execute(
                std::make_unique<AddKeyframeCommand>(&mAutomation, mSelectedLane, kf));
        }
    }
}

// ── Add lane from Inspector ─────────────────────────────────────────────────

void TimelinePanel::addLaneForPort(const std::string& nodeName, const std::string& portName,
                                    float minVal, float maxVal) {
    // Check if lane already exists
    for (int i = 0; i < static_cast<int>(mAutomation.lanes.size()); ++i) {
        if (mAutomation.lanes[i].nodeName == nodeName && mAutomation.lanes[i].portName == portName) {
            mSelectedLane = i; // Flash/highlight existing
            return;
        }
    }

    AutomationLane lane;
    lane.id = nodeName + "." + portName;
    lane.nodeName = nodeName;
    lane.portName = portName;
    lane.displayName = nodeName + " / " + portName;
    lane.hue = std::fmod(mAutomation.lanes.size() * 0.125f, 1.0f);
    lane.minVal = minVal;
    lane.maxVal = maxVal;
    CommandManager::instance().execute(std::make_unique<AddLaneCommand>(&mAutomation, lane));
}

// ── Record value ────────────────────────────────────────────────────────────

void TimelinePanel::recordValue(const std::string& nodeName, const std::string& portName,
                                 float value, float currentBeat) {
    if (!mRecording) return;

    std::string key = nodeName + "." + portName;

    // Deduplication filter
    auto it = mLastRecordedValues.find(key);
    if (it != mLastRecordedValues.end() && std::abs(value - it->second) < 0.01f) return;
    mLastRecordedValues[key] = value;

    // Find matching armed lane
    for (int li = 0; li < static_cast<int>(mAutomation.lanes.size()); ++li) {
        auto& lane = mAutomation.lanes[li];
        if (lane.armed && lane.nodeName == nodeName && lane.portName == portName) {
            Keyframe kf;
            kf.beat = currentBeat;
            kf.value = value;
            CommandManager::instance().execute(
                std::make_unique<AddKeyframeCommand>(&mAutomation, li, kf));
            break;
        }
    }
}

} // namespace bbfx

#pragma once

#include "CommandManager.h"
#include "../../core/AutomationData.h"
#include <vector>
#include <string>

namespace bbfx {

// ── Lane Commands ───────────────────────────────────────────────────────────

class AddLaneCommand : public Command {
public:
    AddLaneCommand(AutomationData* data, const AutomationLane& lane)
        : mData(data), mLane(lane) {}
    void execute() override { mData->lanes.push_back(mLane); }
    void undo() override { if (!mData->lanes.empty()) mData->lanes.pop_back(); }
    std::string description() const override { return "Add automation lane"; }
private:
    AutomationData* mData;
    AutomationLane mLane;
};

class DeleteLaneCommand : public Command {
public:
    DeleteLaneCommand(AutomationData* data, int laneIdx)
        : mData(data), mIdx(laneIdx) {}
    void execute() override {
        mSaved = mData->lanes[mIdx];
        mData->lanes.erase(mData->lanes.begin() + mIdx);
    }
    void undo() override {
        mData->lanes.insert(mData->lanes.begin() + mIdx, mSaved);
    }
    std::string description() const override { return "Delete automation lane"; }
private:
    AutomationData* mData;
    int mIdx;
    AutomationLane mSaved;
};

class AssignLanePortCommand : public Command {
public:
    AssignLanePortCommand(AutomationData* data, int laneIdx,
                          const std::string& nodeName, const std::string& portName,
                          const std::string& displayName, float minVal, float maxVal)
        : mData(data), mIdx(laneIdx),
          mNewNode(nodeName), mNewPort(portName), mNewDisplay(displayName),
          mNewMin(minVal), mNewMax(maxVal) {}
    void execute() override {
        auto& l = mData->lanes[mIdx];
        mOldNode = l.nodeName; mOldPort = l.portName; mOldDisplay = l.displayName;
        mOldMin = l.minVal; mOldMax = l.maxVal;
        l.nodeName = mNewNode; l.portName = mNewPort; l.displayName = mNewDisplay;
        l.id = mNewNode + "." + mNewPort;
        l.minVal = mNewMin; l.maxVal = mNewMax;
    }
    void undo() override {
        auto& l = mData->lanes[mIdx];
        l.nodeName = mOldNode; l.portName = mOldPort; l.displayName = mOldDisplay;
        l.id = mOldNode + "." + mOldPort;
        l.minVal = mOldMin; l.maxVal = mOldMax;
    }
    std::string description() const override { return "Assign lane port"; }
private:
    AutomationData* mData;
    int mIdx;
    std::string mNewNode, mNewPort, mNewDisplay, mOldNode, mOldPort, mOldDisplay;
    float mNewMin, mNewMax, mOldMin, mOldMax;
};

// ── Keyframe Commands ───────────────────────────────────────────────────────

class AddKeyframeCommand : public Command {
public:
    AddKeyframeCommand(AutomationData* data, int laneIdx, const Keyframe& kf)
        : mData(data), mLaneIdx(laneIdx), mKf(kf) {}
    void execute() override {
        mInsertIdx = AutomationData::insertKeyframe(mData->lanes[mLaneIdx], mKf);
    }
    void undo() override {
        auto& kfs = mData->lanes[mLaneIdx].keyframes;
        if (mInsertIdx >= 0 && mInsertIdx < static_cast<int>(kfs.size()))
            kfs.erase(kfs.begin() + mInsertIdx);
    }
    std::string description() const override { return "Add keyframe"; }
private:
    AutomationData* mData;
    int mLaneIdx;
    Keyframe mKf;
    int mInsertIdx = -1;
};

class DeleteKeyframeCommand : public Command {
public:
    DeleteKeyframeCommand(AutomationData* data, int laneIdx, int kfIdx)
        : mData(data), mLaneIdx(laneIdx), mKfIdx(kfIdx) {}
    void execute() override {
        auto& kfs = mData->lanes[mLaneIdx].keyframes;
        mSaved = kfs[mKfIdx];
        kfs.erase(kfs.begin() + mKfIdx);
    }
    void undo() override {
        auto& kfs = mData->lanes[mLaneIdx].keyframes;
        kfs.insert(kfs.begin() + mKfIdx, mSaved);
    }
    std::string description() const override { return "Delete keyframe"; }
private:
    AutomationData* mData;
    int mLaneIdx, mKfIdx;
    Keyframe mSaved;
};

class MoveKeyframeCommand : public Command {
public:
    MoveKeyframeCommand(AutomationData* data, int laneIdx, int kfIdx,
                        float oldBeat, float oldValue, float newBeat, float newValue)
        : mData(data), mLaneIdx(laneIdx), mKfIdx(kfIdx),
          mOldBeat(oldBeat), mOldValue(oldValue), mNewBeat(newBeat), mNewValue(newValue) {}
    void execute() override {
        auto& kf = mData->lanes[mLaneIdx].keyframes[mKfIdx];
        kf.beat = mNewBeat;
        kf.value = mNewValue;
        // Re-sort
        auto& kfs = mData->lanes[mLaneIdx].keyframes;
        std::sort(kfs.begin(), kfs.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.beat < b.beat; });
    }
    void undo() override {
        auto& kfs = mData->lanes[mLaneIdx].keyframes;
        // Find by new beat (approximate)
        for (auto& kf : kfs) {
            if (std::abs(kf.beat - mNewBeat) < 0.001f && std::abs(kf.value - mNewValue) < 0.001f) {
                kf.beat = mOldBeat;
                kf.value = mOldValue;
                break;
            }
        }
        std::sort(kfs.begin(), kfs.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.beat < b.beat; });
    }
    std::string description() const override { return "Move keyframe"; }
private:
    AutomationData* mData;
    int mLaneIdx, mKfIdx;
    float mOldBeat, mOldValue, mNewBeat, mNewValue;
};

class SetInterpolationModeCommand : public Command {
public:
    SetInterpolationModeCommand(AutomationData* data, int laneIdx, int kfIdx,
                                 InterpolationMode newMode)
        : mData(data), mLaneIdx(laneIdx), mKfIdx(kfIdx), mNewMode(newMode) {}
    void execute() override {
        mOldMode = mData->lanes[mLaneIdx].keyframes[mKfIdx].mode;
        mData->lanes[mLaneIdx].keyframes[mKfIdx].mode = mNewMode;
    }
    void undo() override {
        mData->lanes[mLaneIdx].keyframes[mKfIdx].mode = mOldMode;
    }
    std::string description() const override { return "Set interpolation mode"; }
private:
    AutomationData* mData;
    int mLaneIdx, mKfIdx;
    InterpolationMode mOldMode, mNewMode;
};

class SetTangentCommand : public Command {
public:
    SetTangentCommand(AutomationData* data, int laneIdx, int kfIdx,
                      float newInB, float newInV, float newOutB, float newOutV)
        : mData(data), mLaneIdx(laneIdx), mKfIdx(kfIdx),
          mNewInB(newInB), mNewInV(newInV), mNewOutB(newOutB), mNewOutV(newOutV) {}
    void execute() override {
        auto& kf = mData->lanes[mLaneIdx].keyframes[mKfIdx];
        mOldInB = kf.tangentInBeat; mOldInV = kf.tangentInValue;
        mOldOutB = kf.tangentOutBeat; mOldOutV = kf.tangentOutValue;
        kf.tangentInBeat = mNewInB; kf.tangentInValue = mNewInV;
        kf.tangentOutBeat = mNewOutB; kf.tangentOutValue = mNewOutV;
    }
    void undo() override {
        auto& kf = mData->lanes[mLaneIdx].keyframes[mKfIdx];
        kf.tangentInBeat = mOldInB; kf.tangentInValue = mOldInV;
        kf.tangentOutBeat = mOldOutB; kf.tangentOutValue = mOldOutV;
    }
    std::string description() const override { return "Set tangent"; }
private:
    AutomationData* mData;
    int mLaneIdx, mKfIdx;
    float mNewInB, mNewInV, mNewOutB, mNewOutV;
    float mOldInB, mOldInV, mOldOutB, mOldOutV;
};

// ── Cue Marker Commands ─────────────────────────────────────────────────────

class AddCueMarkerCommand : public Command {
public:
    AddCueMarkerCommand(AutomationData* data, const CueMarker& cue)
        : mData(data), mCue(cue) {}
    void execute() override {
        auto& cues = mData->cueMarkers;
        auto it = std::lower_bound(cues.begin(), cues.end(), mCue.beat,
            [](const CueMarker& c, float b) { return c.beat < b; });
        mIdx = static_cast<int>(it - cues.begin());
        cues.insert(it, mCue);
    }
    void undo() override {
        mData->cueMarkers.erase(mData->cueMarkers.begin() + mIdx);
    }
    std::string description() const override { return "Add cue marker"; }
private:
    AutomationData* mData;
    CueMarker mCue;
    int mIdx = 0;
};

class DeleteCueMarkerCommand : public Command {
public:
    DeleteCueMarkerCommand(AutomationData* data, int idx)
        : mData(data), mIdx(idx) {}
    void execute() override {
        mSaved = mData->cueMarkers[mIdx];
        mData->cueMarkers.erase(mData->cueMarkers.begin() + mIdx);
    }
    void undo() override {
        mData->cueMarkers.insert(mData->cueMarkers.begin() + mIdx, mSaved);
    }
    std::string description() const override { return "Delete cue marker"; }
private:
    AutomationData* mData;
    int mIdx;
    CueMarker mSaved;
};

class RenameCueMarkerCommand : public Command {
public:
    RenameCueMarkerCommand(AutomationData* data, int idx, const std::string& newName)
        : mData(data), mIdx(idx), mNewName(newName) {}
    void execute() override {
        mOldName = mData->cueMarkers[mIdx].name;
        mData->cueMarkers[mIdx].name = mNewName;
    }
    void undo() override {
        mData->cueMarkers[mIdx].name = mOldName;
    }
    std::string description() const override { return "Rename cue marker"; }
private:
    AutomationData* mData;
    int mIdx;
    std::string mNewName, mOldName;
};

// ── Trigger Event Commands ──────────────────────────────────────────────────

class AddTriggerEventCommand : public Command {
public:
    AddTriggerEventCommand(AutomationData* data, const TriggerEvent& te)
        : mData(data), mEvent(te) {}
    void execute() override {
        auto& evts = mData->triggerEvents;
        auto it = std::lower_bound(evts.begin(), evts.end(), mEvent.beat,
            [](const TriggerEvent& e, float b) { return e.beat < b; });
        mIdx = static_cast<int>(it - evts.begin());
        evts.insert(it, mEvent);
    }
    void undo() override {
        mData->triggerEvents.erase(mData->triggerEvents.begin() + mIdx);
    }
    std::string description() const override { return "Add trigger event"; }
private:
    AutomationData* mData;
    TriggerEvent mEvent;
    int mIdx = 0;
};

class DeleteTriggerEventCommand : public Command {
public:
    DeleteTriggerEventCommand(AutomationData* data, int idx)
        : mData(data), mIdx(idx) {}
    void execute() override {
        mSaved = mData->triggerEvents[mIdx];
        mData->triggerEvents.erase(mData->triggerEvents.begin() + mIdx);
    }
    void undo() override {
        mData->triggerEvents.insert(mData->triggerEvents.begin() + mIdx, mSaved);
    }
    std::string description() const override { return "Delete trigger event"; }
private:
    AutomationData* mData;
    int mIdx;
    TriggerEvent mSaved;
};

// ── Loop Region Command ─────────────────────────────────────────────────────

class SetLoopRegionCommand : public Command {
public:
    SetLoopRegionCommand(AutomationData* data, const LoopRegion& newRegion)
        : mData(data), mNew(newRegion) {}
    void execute() override {
        mOld = mData->loopRegion;
        mData->loopRegion = mNew;
    }
    void undo() override {
        mData->loopRegion = mOld;
    }
    std::string description() const override { return "Set loop region"; }
private:
    AutomationData* mData;
    LoopRegion mOld, mNew;
};

} // namespace bbfx

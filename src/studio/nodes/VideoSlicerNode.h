#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <string>

namespace bbfx {

class TheoraClip;
class TheoraClipNode;

/// VideoSlicerNode — slices a TheoraClip into a sub-loop (in_point/out_point)
/// and offers direct scrub via the `playhead` port. When `playhead` is wired
/// to a DAG source (e.g. a MIDI fader), each frame seeks the underlying clip
/// to `in_point + playhead * (out_point - in_point)`. Otherwise the node loops
/// the segment forward (or reverses if playback_speed < 0).
class VideoSlicerNode : public AnimationNode {
public:
    explicit VideoSlicerNode(const std::string& name);
    ~VideoSlicerNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "VideoSlicerNode"; }

    float getInternalPlayhead() const { return mInternalPlayhead; }
    bool  isPlayheadConnected() const { return mPlayheadConnected; }
    const std::string& getMaterialName() const { return mLastMaterialName; }

private:
    ParamSpec mSpec;
    AnimationNode* mPrevClipNode = nullptr;
    TheoraClip* mClip = nullptr;
    float mInPoint = 0.0f;
    float mOutPoint = 1.0f;
    float mInternalPlayhead = 0.0f;
    bool  mAutoPlay = true;
    float mPlaybackSpeed = 1.0f;
    bool  mPlayheadConnected = false;
    float mPrevDt = 1.0f / 60.0f;
    std::string mLastMaterialName;

    void resolveClip();
};

} // namespace bbfx

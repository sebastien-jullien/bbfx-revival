#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include "TheoraClip.h"
#include <memory>

namespace bbfx {

class TheoraClipNode : public AnimationNode {
public:
    /// Backward-compatible: when called with a single argument, the node name
    /// defaults to "TheoraClipNode" (legacy behavior). Newer call sites should
    /// pass an explicit name first to support multiple instances.
    explicit TheoraClipNode(const std::string& filename);
    TheoraClipNode(const std::string& name, const std::string& filename);
    ~TheoraClipNode() override;
    void update() override;
    std::string getTypeName() const override { return "TheoraClipNode"; }

    void play() { mClip->play(); }
    void pause() { mClip->pause(); }
    void stop() { mClip->stop(); }

    TheoraClip* getClip() { return mClip.get(); }

    /// Read-only ParamSpec mirror name: `material_out` — exposes
    /// `mClip->getMaterialName()` for downstream consumers (MaterialBridgeNode
    /// Lot T routing video → mesh, VideoCrossfade chaining, etc.). Empty
    /// string when the clip is dormant (file missing / not yet initialized).
    /// Pattern aligned with VideoCrossfadeNode/VideoSlicerNode/VideoLibraryNode.
    /// Output port `material_ready` (FLOAT 0/1) pulses 1.0 each frame the
    /// material name is non-empty AND the clip is currently playing.

private:
    ParamSpec                   mSpec;
    std::unique_ptr<TheoraClip> mClip;
    float                       mLastPlayVal = 0.0f;  // v3.5.2 Lot AU.7 — edge-detect the `play` port
    bool                        mLastReverseVal = false;   // Lot AV.4 round 4 — edge-detect reverse port
    std::string                 mLoadedReverseFile;        // Lot AV.4 round 4 — track loaded reverse path to avoid re-load
    float                       mLastTimeControl = -1.0f;  // Lot AV.5 round 16 — edge-detect time_control for test seeks
    float                       mLastSpeed = 1.0f;         // Lot AW round 1 — per-instance speed-change tracking (was a shared function-static)
};

} // namespace bbfx

#pragma once
#include <string>
#include <vector>

namespace Ogre { class Viewport; }

namespace bbfx {
class Animator;

/// Compositor Stack Panel: lists active CompositorNodes, drag-reorder, inline params, solo/bypass.
class CompositorStackPanel {
public:
    void render();
    void setAnimator(Animator* anim) { mAnimator = anim; }

    const std::vector<std::string>& getStackOrder() const { return mStackOrder; }
    bool isDirty() const { return mDirty; }
    void clearDirty() { mDirty = false; }
    void setStackOrder(const std::vector<std::string>& order) { mStackOrder = order; mDirty = true; }

    /// Apply the compositor chain to an OGRE viewport (Studio or Performance Mode).
    /// Manages add/remove/enable of compositors according to stack order.
    void applyToViewport(Ogre::Viewport* viewport);

    /// Remove all applied compositors from the viewport.
    void removeFromViewport(Ogre::Viewport* viewport);

    /// Sync mStackOrder with CompositorNodes in the DAG.
    void syncStackOrder();

    /// Force re-application of compositors on next applyToViewport (after RT resize).
    void invalidateApplied() { mAppliedCompositors.clear(); mDirty = true; }

private:
    Animator* mAnimator = nullptr;
    std::vector<std::string> mStackOrder;
    bool mDirty = false;

    // Solo state
    bool mSoloActive = false;
    std::vector<std::pair<std::string, bool>> mSavedStates;

    // Applied compositors tracking
    std::vector<std::string> mAppliedCompositors;
};

} // namespace bbfx

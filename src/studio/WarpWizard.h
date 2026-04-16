#pragma once

#include "WarpProfile.h"
#include <string>

namespace bbfx {

class OutputManager;

/// State machine for the 4-click projector calibration wizard.
enum class WarpWizardState {
    IDLE,       ///< Wizard is not running
    DETECTING,  ///< Auto-detecting best output monitor
    PATTERN,    ///< Showing test pattern, awaiting first click
    CLICK_TL,   ///< TL clicked; awaiting TR
    CLICK_TR,   ///< TR clicked; awaiting BL
    CLICK_BL,   ///< BL clicked; awaiting BR
    CLICK_BR,   ///< BR clicked; awaiting final confirm (or auto-compute)
    COMPUTING,  ///< Computing perspective warp from the 4 points
    DONE,       ///< Calibration applied successfully
    CANCELLED   ///< User cancelled
};

/// 4-click projector calibration wizard.
///
/// Usage:
///   1. start(slotId, mgr)          — begins DETECTING → PATTERN
///   2. handleMouseClick(nx, ny)     — record each corner click
///   3. After 4 clicks → COMPUTING → auto-applies → DONE
///   4. handleEscapeKey() / cancel() — restores previous warp
///
/// The WarpWizard directly sets OutputSlot::testPattern state so the
/// OutputManager renders the test pattern overlay during calibration.
class WarpWizard {
public:
    WarpWizard() = default;
    ~WarpWizard() = default;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /// Start calibration for the given output slot. Saves current warp as backup.
    void start(int outputSlotId, OutputManager* mgr);

    /// Cancel calibration and restore the previous warp.
    void cancel();

    // ── Queries ──────────────────────────────────────────────────────────────

    bool isActive()    const { return mState != WarpWizardState::IDLE; }
    WarpWizardState getState()     const { return mState; }
    int   getOutputSlotId()        const { return mOutputSlotId; }

    /// Human-readable instruction for the current step.
    std::string getInstructionText() const;

    /// How many corner clicks have been recorded so far (0-4).
    int getNumClicked() const { return mNumClicked; }

    /// Access to the 4 clicked points (normalised 0-1, pairs of x,y).
    const float* getClickedPoints() const { return mClickedPoints; }

    /// The warp profile before calibration started (used by undo).
    const WarpProfile& getPreviousWarp()  const { return mPreviousWarp; }
    /// The warp computed from the 4 clicks (valid after DONE).
    const WarpProfile& getComputedWarp()  const { return mComputedWarp; }

    // ── Event routing ────────────────────────────────────────────────────────

    /// Called from StudioApp::handleEvent when a left click occurs on the
    /// wizard's output window.  Coordinates are normalised 0-1.
    void handleMouseClick(float nx, float ny);

    /// Called when Escape is pressed while the wizard is active.
    void handleEscapeKey();

private:
    void advanceState();       ///< Move to next CLICK_* state after a click.
    void computeAndApply();    ///< Compute warp from mClickedPoints and apply.

    WarpWizardState mState       = WarpWizardState::IDLE;
    int             mOutputSlotId = -1;
    OutputManager*  mManager      = nullptr;

    WarpProfile mPreviousWarp;  ///< Backup of warp before wizard started.
    WarpProfile mComputedWarp;  ///< Result after COMPUTING.

    /// Up to 4 clicked points (x0,y0, x1,y1, x2,y2, x3,y3), normalised 0-1.
    float mClickedPoints[8] = {};
    int   mNumClicked       = 0;
};

} // namespace bbfx

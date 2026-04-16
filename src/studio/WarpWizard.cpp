#include "WarpWizard.h"
#include "OutputManager.h"
#include <iostream>
#include <cstring>
#include <cmath>

namespace bbfx {

// ── start / cancel ────────────────────────────────────────────────────────────

void WarpWizard::start(int outputSlotId, OutputManager* mgr) {
    if (!mgr) return;

    // Only one wizard at a time.
    if (mState != WarpWizardState::IDLE) {
        std::cerr << "[WarpWizard] Already active on slot " << mOutputSlotId
                  << " — ignoring start request for slot " << outputSlotId << std::endl;
        return;
    }

    mOutputSlotId = outputSlotId;
    mManager      = mgr;
    mNumClicked   = 0;
    std::memset(mClickedPoints, 0, sizeof(mClickedPoints));

    auto* slot = mgr->getSlot(outputSlotId);
    if (!slot) {
        std::cerr << "[WarpWizard] Slot " << outputSlotId << " not found." << std::endl;
        return;
    }

    // Backup current warp.
    mPreviousWarp = slot->warpProfile;

    // Auto-detect phase: immediately transition to PATTERN (single-machine).
    mState = WarpWizardState::DETECTING;

    // Enable test pattern on the slot.
    slot->testPattern.active    = true;
    slot->testPattern.numClicked = 0;

    // Transition to PATTERN state.
    mState = WarpWizardState::PATTERN;

    std::cout << "[WarpWizard] Started on slot " << outputSlotId << std::endl;
}

void WarpWizard::cancel() {
    if (mState == WarpWizardState::IDLE) return;

    // Restore previous warp.
    if (mManager) {
        auto* slot = mManager->getSlot(mOutputSlotId);
        if (slot) {
            slot->warpProfile  = mPreviousWarp;
            slot->testPattern.active    = false;
            slot->testPattern.numClicked = 0;
            if (slot->warpEnabled) mManager->updateWarpParams(mOutputSlotId);
        }
    }

    mState        = WarpWizardState::CANCELLED;
    mManager      = nullptr;
    mOutputSlotId = -1;
    mNumClicked   = 0;
    mState        = WarpWizardState::IDLE;

    std::cout << "[WarpWizard] Cancelled — previous warp restored." << std::endl;
}

// ── Event routing ─────────────────────────────────────────────────────────────

void WarpWizard::handleMouseClick(float nx, float ny) {
    // Only accept clicks in PATTERN or CLICK_* states.
    if (mState == WarpWizardState::IDLE ||
        mState == WarpWizardState::DETECTING ||
        mState == WarpWizardState::COMPUTING ||
        mState == WarpWizardState::DONE ||
        mState == WarpWizardState::CANCELLED) {
        return;
    }

    if (mNumClicked >= 4) return;

    mClickedPoints[mNumClicked * 2 + 0] = nx;
    mClickedPoints[mNumClicked * 2 + 1] = ny;
    ++mNumClicked;

    // Update test pattern overlay with the new click.
    if (mManager) {
        auto* slot = mManager->getSlot(mOutputSlotId);
        if (slot) {
            slot->testPattern.numClicked = mNumClicked;
            std::memcpy(slot->testPattern.clickedPoints, mClickedPoints, sizeof(mClickedPoints));
        }
    }

    std::cout << "[WarpWizard] Click " << mNumClicked << "/4 at ("
              << nx << ", " << ny << ")" << std::endl;

    advanceState();

    if (mNumClicked == 4) {
        computeAndApply();
    }
}

void WarpWizard::handleEscapeKey() {
    if (!isActive()) return;
    cancel();
}

// ── State machine ─────────────────────────────────────────────────────────────

void WarpWizard::advanceState() {
    switch (mState) {
        case WarpWizardState::PATTERN:   mState = WarpWizardState::CLICK_TL; break;
        case WarpWizardState::CLICK_TL:  mState = WarpWizardState::CLICK_TR; break;
        case WarpWizardState::CLICK_TR:  mState = WarpWizardState::CLICK_BL; break;
        case WarpWizardState::CLICK_BL:  mState = WarpWizardState::CLICK_BR; break;
        case WarpWizardState::CLICK_BR:  mState = WarpWizardState::COMPUTING; break;
        default: break;
    }
}

// ── Perspective computation ────────────────────────────────────────────────────

/// Compute warp profile from 4 clicked points.
///
/// The user clicked the 4 corners of their target projection surface in order:
///   TL → TR → BL → BR
/// These normalised screen positions become the WarpProfile corners, since
/// the quad_warp shader maps output pixels through the inverse bilinear to
/// find source UV — so clicking where content TL should land defines the
/// TL corner of the warp quad.
void WarpWizard::computeAndApply() {
    mState = WarpWizardState::COMPUTING;

    // Build the computed WarpProfile from clicked points.
    // corners: TL(0,1) TR(2,3) BL(4,5) BR(6,7)
    mComputedWarp.corners[0] = mClickedPoints[0]; // TL.x
    mComputedWarp.corners[1] = mClickedPoints[1]; // TL.y
    mComputedWarp.corners[2] = mClickedPoints[2]; // TR.x
    mComputedWarp.corners[3] = mClickedPoints[3]; // TR.y
    mComputedWarp.corners[4] = mClickedPoints[4]; // BL.x
    mComputedWarp.corners[5] = mClickedPoints[5]; // BL.y
    mComputedWarp.corners[6] = mClickedPoints[6]; // BR.x
    mComputedWarp.corners[7] = mClickedPoints[7]; // BR.y

    // Apply the computed warp to the slot.
    if (mManager) {
        auto* slot = mManager->getSlot(mOutputSlotId);
        if (slot) {
            slot->warpProfile = mComputedWarp;
            slot->testPattern.active    = false;
            slot->testPattern.numClicked = 0;
            if (!slot->warpEnabled) mManager->enableWarp(mOutputSlotId);
            else                    mManager->updateWarpParams(mOutputSlotId);
        }
    }

    mState = WarpWizardState::DONE;

    std::cout << "[WarpWizard] Calibration complete — warp applied to slot "
              << mOutputSlotId << std::endl;
}

// ── Instruction text ──────────────────────────────────────────────────────────

std::string WarpWizard::getInstructionText() const {
    switch (mState) {
        case WarpWizardState::IDLE:       return "";
        case WarpWizardState::DETECTING:  return "Detecting output monitor...";
        case WarpWizardState::PATTERN:    return "Step 1/4: Click the TOP-LEFT corner of your projection surface";
        case WarpWizardState::CLICK_TL:   return "Step 2/4: Click the TOP-RIGHT corner of your projection surface";
        case WarpWizardState::CLICK_TR:   return "Step 3/4: Click the BOTTOM-LEFT corner of your projection surface";
        case WarpWizardState::CLICK_BL:   return "Step 4/4: Click the BOTTOM-RIGHT corner of your projection surface";
        case WarpWizardState::CLICK_BR:   return "Computing perspective warp...";
        case WarpWizardState::COMPUTING:  return "Computing perspective warp...";
        case WarpWizardState::DONE:       return "Calibration complete!";
        case WarpWizardState::CANCELLED:  return "Calibration cancelled.";
        default:                          return "";
    }
}

} // namespace bbfx

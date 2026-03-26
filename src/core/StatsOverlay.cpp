#include "StatsOverlay.h"
#include <sstream>
#include <iomanip>

namespace bbfx {

StatsOverlay* StatsOverlay::sInstance = nullptr;

StatsOverlay::StatsOverlay() {
    assert(!sInstance);
    sInstance = this;

    auto& overlayMgr = Ogre::OverlayManager::getSingleton();

    // Create overlay
    mOverlay = overlayMgr.create("BBFx/StatsOverlay");

    // Create container panel
    auto* panel = static_cast<Ogre::OverlayContainer*>(
        overlayMgr.createOverlayElement("Panel", "BBFx/StatsPanel"));
    panel->setMetricsMode(Ogre::GMM_PIXELS);
    panel->setPosition(10, 10);
    panel->setDimensions(200, 40);

    // Create text element
    mFpsText = static_cast<Ogre::TextAreaOverlayElement*>(
        overlayMgr.createOverlayElement("TextArea", "BBFx/StatsText"));
    mFpsText->setMetricsMode(Ogre::GMM_PIXELS);
    mFpsText->setPosition(0, 0);
    mFpsText->setDimensions(200, 40);
    mFpsText->setCharHeight(16);
    mFpsText->setColour(Ogre::ColourValue::White);
    mFpsText->setCaption("FPS: ---");

    panel->addChild(mFpsText);
    mOverlay->add2D(panel);
    mOverlay->setZOrder(500);
    // Start hidden
    mOverlay->hide();
}

StatsOverlay::~StatsOverlay() {
    if (mOverlay) {
        auto& overlayMgr = Ogre::OverlayManager::getSingleton();
        overlayMgr.destroy(mOverlay);
    }
    sInstance = nullptr;
}

StatsOverlay* StatsOverlay::instance() {
    return sInstance;
}

void StatsOverlay::toggle() {
    if (mVisible) hide(); else show();
}

void StatsOverlay::show() {
    mVisible = true;
    if (mOverlay) mOverlay->show();
}

void StatsOverlay::hide() {
    mVisible = false;
    if (mOverlay) mOverlay->hide();
}

bool StatsOverlay::isVisible() const {
    return mVisible;
}

void StatsOverlay::update(float dt) {
    if (!mVisible) return;

    mAccumTime += dt;
    mFrameCount++;
    mLastFrameTime = dt;

    // Update display at least every second
    if (mAccumTime >= 1.0f) {
        mLastFps = static_cast<float>(mFrameCount) / mAccumTime;
        mAccumTime = 0.0f;
        mFrameCount = 0;

        if (mFpsText) {
            std::ostringstream ss;
            ss << "FPS: " << static_cast<int>(mLastFps)
               << "  (" << std::fixed << std::setprecision(1)
               << (mLastFrameTime * 1000.0f) << " ms)";
            mFpsText->setCaption(ss.str());
        }
    }
}

} // namespace bbfx

#pragma once

#include "../bbfx.h"
#include <OgreOverlay.h>
#include <OgreOverlayManager.h>
#include <OgreOverlayContainer.h>
#include <OgreTextAreaOverlayElement.h>
#include <OgreFontManager.h>

namespace bbfx {

class StatsOverlay {
public:
    StatsOverlay();
    ~StatsOverlay();
    static StatsOverlay* instance();

    void toggle();
    void show();
    void hide();
    bool isVisible() const;
    void update(float dt);

private:
    static StatsOverlay* sInstance;
    Ogre::Overlay* mOverlay = nullptr;
    Ogre::TextAreaOverlayElement* mFpsText = nullptr;
    bool mVisible = false;
    float mAccumTime = 0.0f;
    int mFrameCount = 0;
    float mLastFps = 0.0f;
    float mLastFrameTime = 0.0f;
};

} // namespace bbfx

#pragma once

#include "../bbfx.h"
#include "../platform.h"
#include "Animator.h"
#include "../input/InputManager.h"

#include <sol/sol.hpp>
#include <SDL3/SDL.h>
#include <OgreRoot.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>

namespace bbfx {

class VideoExporter;

class Engine {
public:
    explicit Engine(sol::state& lua);
    ~Engine();
    static Engine* instance();

    void startRendering();
    void stopRendering();
    void screenshot();
    void toggleFullscreen();

    Ogre::SceneManager* getSceneManager() const;
    Ogre::RenderWindow* getRenderWindow() const;

    void setOfflineMode(int fps);
    void setOnlineMode();
    bool isOfflineMode() const { return mOfflineMode; }
    float getOfflineDt() const { return mOfflineDt; }
    void setVideoExporter(VideoExporter* exporter);
    void clearVideoExporter(VideoExporter* exporter = nullptr);

protected:
    void loadOgrePlugins();
    void loadResources();
    void fillWindowParams(Ogre::NameValuePairList& params);

    volatile bool mStopQueued = false;
    bool mFullscreen = false;
    sol::state& mLua;
    SDL_Window* mWindow = nullptr;
    Ogre::Root* mRoot = nullptr;
    Ogre::RenderWindow* mRenderWindow = nullptr;
    Ogre::SceneManager* mSceneManager = nullptr;
    InputManager* mInputManager = nullptr;

    bool mOfflineMode = false;
    float mOfflineDt = 1.0f / 60.0f;
    VideoExporter* mVideoExporter = nullptr;

private:
    static Engine* sInstance;
};

} // namespace bbfx

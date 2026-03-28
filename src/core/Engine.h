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
    // Standard headless/REPL mode constructor (bbfx executable)
    explicit Engine(sol::state& lua);
    virtual ~Engine();
    static Engine* instance();

    virtual void startRendering();
    void stopRendering();
    void screenshot();
    void toggleFullscreen();

    Ogre::SceneManager* getSceneManager() const;
    Ogre::RenderWindow* getRenderWindow() const;
    SDL_Window*         getSDLWindow()    const { return mWindow; }
    InputManager*       getInputManager() const { return mInputManager; }

    void setOfflineMode(int fps);
    void setOnlineMode();
    bool isOfflineMode() const { return mOfflineMode; }
    float getOfflineDt() const { return mOfflineDt; }
    void setVideoExporter(VideoExporter* exporter);
    void clearVideoExporter(VideoExporter* exporter = nullptr);

protected:
    // Two-phase init constructor: SDL3 window created, OGRE deferred.
    // Used by StudioEngine to interleave SDL3 GL context creation before OGRE.
    Engine(sol::state& lua, SDL_WindowFlags extraWindowFlags, bool deferOgreInit);

    // Phase 2: OGRE init. Called immediately by the standard ctor.
    // StudioEngine calls this manually after creating the SDL3 GL context.
    void initOGRE(bool externalGLContext = false);

    void loadOgrePlugins();
    void loadResources();
    void fillWindowParams(Ogre::NameValuePairList& params, bool externalGLContext = false);

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

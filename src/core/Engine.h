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

private:
    static Engine* sInstance;
};

} // namespace bbfx

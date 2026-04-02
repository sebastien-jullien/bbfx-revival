#pragma once
#include <OgreSceneManager.h>
#include <OgreManualObject.h>
#include <OgreSceneNode.h>

namespace bbfx {

/// Renders a 3D grid on the XZ plane and provides an axis indicator.
class ViewportGrid {
public:
    explicit ViewportGrid(Ogre::SceneManager* sm);
    ~ViewportGrid();

    void setVisible(bool v);
    bool isVisible() const { return mVisible; }

private:
    void buildGrid();
    void createMaterial();

    Ogre::SceneManager* mScene;
    Ogre::ManualObject* mGrid = nullptr;
    Ogre::SceneNode* mGridNode = nullptr;
    bool mVisible = true;
};

} // namespace bbfx

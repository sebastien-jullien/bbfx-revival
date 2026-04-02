#pragma once
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreMovableObject.h>
#include <functional>
#include <string>
#include <vector>

namespace bbfx {

/// Handles 3D object picking via OGRE RaySceneQuery and manages selection state.
class ViewportPicker {
public:
    /// Query mask for scene objects (pickable). Gizmos and grid use different masks.
    static constexpr uint32_t SCENE_QUERY_MASK = 0x01;
    static constexpr uint32_t GIZMO_QUERY_MASK = 0x02;

    ViewportPicker(Ogre::SceneManager* sm, Ogre::Camera* cam);
    ~ViewportPicker();

    /// Cast a ray from normalized viewport coordinates [0,1] and return the first hit.
    Ogre::MovableObject* pick(float normalizedX, float normalizedY);

    /// Select a specific movable object.
    void select(Ogre::MovableObject* obj);

    /// Select by DAG node name (for sync from NodeEditor).
    void selectByDAGName(const std::string& dagName);

    /// Clear selection.
    void deselect();

    /// Currently selected movable object (or nullptr).
    Ogre::MovableObject* getSelected() const { return mSelected; }

    /// Name of the DAG node corresponding to the selected object.
    const std::string& getSelectedNodeName() const { return mSelectedDAGNode; }

    /// Set callback fired when selection changes. Empty string = deselection.
    void setSelectionCallback(std::function<void(const std::string&)> cb) {
        mSelectionCallback = std::move(cb);
    }

private:
    Ogre::SceneManager* mScene;
    Ogre::Camera* mCamera;
    Ogre::RaySceneQuery* mRayQuery = nullptr;
    Ogre::MovableObject* mSelected = nullptr;
    std::string mSelectedDAGNode;
    std::function<void(const std::string&)> mSelectionCallback;

    /// Find which DAG node owns the given OGRE MovableObject.
    std::string findDAGNodeForEntity(Ogre::MovableObject* obj) const;

    /// Apply/remove selection highlight on the selected entity.
    void applyHighlight();
    void removeHighlight();
    Ogre::Entity* mHighlightEntity = nullptr;  // wireframe clone for selection overlay
};

} // namespace bbfx

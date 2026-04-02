#pragma once

#include <string>
#include <functional>

namespace bbfx {

class Animator;
class AnimationNode;

/// Displays a filtered tree of scene nodes (SceneObject, Light, Particle, Camera, etc.).
/// Provides selection sync, visibility toggle, lock toggle, and context menu.
class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Animator* animator);
    void render();

    void setSelectionCallback(std::function<void(const std::string&)> cb) { mSelectionCallback = std::move(cb); }
    void selectNodeFromExternal(const std::string& name);

    const std::string& getSelectedNode() const { return mSelectedNode; }

private:
    Animator* mAnimator;
    std::string mSelectedNode;
    std::function<void(const std::string&)> mSelectionCallback;
    bool mUpdatingFromExternal = false;

    bool isSceneNode(AnimationNode* node) const;
    const char* getIconForType(const std::string& typeName) const;
    void renderItem(AnimationNode* node, const std::string& name);
};

} // namespace bbfx

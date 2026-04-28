#pragma once

#include "../core/AnimationNode.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <string>
#include <vector>

namespace Ogre { class Entity; }

namespace bbfx {

/// HSV color shift effect applied to a linked SceneObjectNode's materials.
/// Target entity is resolved dynamically via the "entity" input port (same
/// pattern as ShaderFxNode / PerlinFxNode).
class ColorShiftNode : public AnimationNode {
public:
    explicit ColorShiftNode(const std::string& nodeName = "ColorShiftNode");
    ~ColorShiftNode() override;
    void update() override;
    void cleanup() override;
    void onLinkChanged() override;
    void setEnabled(bool en) override;
    std::string getTypeName() const override { return "ColorShiftNode"; }

private:
    void resolveTarget();
    void applyToEntity(Ogre::Entity* entity);
    void detachFromEntity();
    static Ogre::ColourValue hsvToRgb(float h, float s, float v);

    Ogre::Entity* mEntity = nullptr;
    std::string mTargetNodeName;
    int mEntityVersion = -1;       // SceneObjectNode entity version when last attached
    std::vector<std::string> mMaterialNames;    // per sub-entity material names
    struct SavedColors { Ogre::ColourValue emissive, diffuse, ambient; };
    std::vector<SavedColors> mOriginalColors;
    bool mSavedOriginal = false;
};

} // namespace bbfx

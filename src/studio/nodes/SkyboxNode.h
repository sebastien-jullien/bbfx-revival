#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
namespace bbfx {
class SkyboxNode : public AnimationNode {
public:
    SkyboxNode(const std::string& name, Ogre::SceneManager* scene);
    ~SkyboxNode() override;   // C7 — désactive le ciel + libère matériaux/texture runtime
    void update() override;
    std::string getTypeName() const override { return "SkyboxNode"; }
private:
    Ogre::SceneManager* mScene;
    ParamSpec mSpec;
    // D11 — état appliqué (dirty-tracking pour ne reconstruire qu'au changement).
    std::string mAppliedType;
    std::string mAppliedMaterial;
    float mAppliedR = -1, mAppliedG = -1, mAppliedB = -1;
    std::string mColorMatName;     // matériau couleur unie runtime
    std::string mGradientMatName;  // matériau gradient (skydome) runtime
    std::string mGradientTexName;  // texture gradient runtime
    void ensureColorMaterial(float r, float g, float b);
    void ensureGradientMaterial(float r, float g, float b);
};
} // namespace bbfx

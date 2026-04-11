#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreEntity.h>

namespace bbfx {

/// Creates a visible 3D object in the OGRE scene.
/// Supports mesh, billboard, light, particle system, and plane types.
/// Position, scale, and rotation are animatable via DAG ports.
class SceneObjectNode : public AnimationNode {
public:
    SceneObjectNode(const std::string& name, Ogre::SceneManager* scene);
    ~SceneObjectNode() override;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void setUserVisible(bool v) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "SceneObjectNode"; }
    Ogre::SceneNode* getSceneNode() const { return mSceneNode; }
    Ogre::Entity* getEntity() const { return dynamic_cast<Ogre::Entity*>(mMovable); }
    std::string getMeshName() const { return mCurrentMesh; }

    // ── Transform offsets (v3.3) ──────────────────────────────────────────
    // Offsets are applied ON TOP of DAG port values:
    //   final_position = port_position + offsetPos
    //   final_rotation = port_rotation + offsetRot (Euler degrees)
    //   final_scale    = port_scale * offsetScale
    // This allows the gizmo to reposition objects without disrupting DAG animations.
    void setOffsetPos(const Ogre::Vector3& v)   { mOffsetPos = v; }
    void setOffsetRot(const Ogre::Vector3& v)   { mOffsetRot = v; }
    void setOffsetScale(const Ogre::Vector3& v) { mOffsetScale = v; }
    const Ogre::Vector3& getOffsetPos()   const { return mOffsetPos; }
    const Ogre::Vector3& getOffsetRot()   const { return mOffsetRot; }
    const Ogre::Vector3& getOffsetScale() const { return mOffsetScale; }
    void resetOffsets() { mOffsetPos = Ogre::Vector3::ZERO; mOffsetRot = Ogre::Vector3::ZERO; mOffsetScale = Ogre::Vector3::UNIT_SCALE; }
    void resetOffsetPos()   { mOffsetPos = Ogre::Vector3::ZERO; }
    void resetOffsetRot()   { mOffsetRot = Ogre::Vector3::ZERO; }
    void resetOffsetScale() { mOffsetScale = Ogre::Vector3::UNIT_SCALE; }

    // DAG Priority: when enabled, offsets on axes that have an incoming DAG link are zeroed.
    // E.g., if rotation.y has a link (animation), offsetRot.y is forced to 0.
    bool isDAGPriority() const { return mDAGPriority; }
    void setDAGPriority(bool v) { mDAGPriority = v; }

    /// Combined intended render visibility: enabled + userVisible + ParamSpec BOOL / DAG port.
    /// Used by FX clones (PerlinFxNode, WaveVertexShader) to sync their clone visibility.
    bool isNodeVisible() const;

private:
    Ogre::SceneManager* mScene;
    Ogre::SceneNode* mSceneNode = nullptr;
    Ogre::MovableObject* mMovable = nullptr;
    ParamSpec mSpec;
    std::string mCurrentMesh;
    Ogre::Vector3 mOffsetPos   = Ogre::Vector3::ZERO;
    Ogre::Vector3 mOffsetRot   = Ogre::Vector3::ZERO;
    Ogre::Vector3 mOffsetScale = Ogre::Vector3::UNIT_SCALE;
    bool mDAGPriority = true; // default ON: DAG animations override offsets on linked axes
    // Cached: which ports have an incoming DAG link with enabled source
    bool mLinkedPosX = false, mLinkedPosY = false, mLinkedPosZ = false;
    bool mLinkedRotX = false, mLinkedRotY = false, mLinkedRotZ = false;
    bool mLinkedSclX = false, mLinkedSclY = false, mLinkedSclZ = false;
    bool mLinkedVis  = false;
    int mLinkRefreshCounter = 0; // refresh link cache every N frames

    void createDefaultObject();
};

} // namespace bbfx

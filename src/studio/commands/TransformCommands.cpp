#include "TransformCommands.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../nodes/SceneObjectNode.h"
#include <OgreSceneNode.h>
#include <OgreQuaternion.h>
#include <OgreMath.h>

namespace bbfx {

TransformNodeCommand::TransformNodeCommand(
    const std::string& nodeName, const std::string& desc,
    float px, float py, float pz,
    float sx, float sy, float sz,
    float rx, float ry, float rz,
    float npx, float npy, float npz,
    float nsx, float nsy, float nsz,
    float nrx, float nry, float nrz)
    : mNodeName(nodeName), mDesc(desc)
{
    mOld[0]=px;  mOld[1]=py;  mOld[2]=pz;
    mOld[3]=sx;  mOld[4]=sy;  mOld[5]=sz;
    mOld[6]=rx;  mOld[7]=ry;  mOld[8]=rz;
    mNew[0]=npx; mNew[1]=npy; mNew[2]=npz;
    mNew[3]=nsx; mNew[4]=nsy; mNew[5]=nsz;
    mNew[6]=nrx; mNew[7]=nry; mNew[8]=nrz;
}

void TransformNodeCommand::execute()
{
    applyValues(mNew);
}

void TransformNodeCommand::undo()
{
    applyValues(mOld);
}

void TransformNodeCommand::applyValues(const float vals[9])
{
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (!node) return;

    auto& inputs = node->getInputs();
    auto setPort = [&](const std::string& portName, float val) {
        auto it = inputs.find(portName);
        if (it != inputs.end()) it->second->setValue(val);
    };

    setPort("position.x", vals[0]);
    setPort("position.y", vals[1]);
    setPort("position.z", vals[2]);
    setPort("scale.x",    vals[3]);
    setPort("scale.y",    vals[4]);
    setPort("scale.z",    vals[5]);
    setPort("rotation.x", vals[6]);
    setPort("rotation.y", vals[7]);
    setPort("rotation.z", vals[8]);

    // Also update the OGRE SceneNode directly for immediate visual feedback
    if (auto* soNode = dynamic_cast<SceneObjectNode*>(node)) {
        auto* sn = soNode->getSceneNode();
        if (sn) {
            sn->setPosition(vals[0], vals[1], vals[2]);
            sn->setScale(vals[3], vals[4], vals[5]);
            Ogre::Quaternion qx(Ogre::Degree(vals[6]), Ogre::Vector3::UNIT_X);
            Ogre::Quaternion qy(Ogre::Degree(vals[7]), Ogre::Vector3::UNIT_Y);
            Ogre::Quaternion qz(Ogre::Degree(vals[8]), Ogre::Vector3::UNIT_Z);
            sn->setOrientation(qy * qx * qz);
        }
    }
}

} // namespace bbfx

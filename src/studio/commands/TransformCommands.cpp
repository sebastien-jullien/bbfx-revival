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

    // Apply as OFFSETS on SceneObjectNode (v3.3 offset system).
    // This preserves DAG animation — offsets are added on top of port values.
    auto* soNode = dynamic_cast<SceneObjectNode*>(node);
    if (soNode) {
        soNode->setOffsetPos(Ogre::Vector3(vals[0], vals[1], vals[2]));
        soNode->setOffsetScale(Ogre::Vector3(vals[3], vals[4], vals[5]));
        soNode->setOffsetRot(Ogre::Vector3(vals[6], vals[7], vals[8]));
    }
}

} // namespace bbfx

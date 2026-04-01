#pragma once

#include "../core/AnimationNode.h"
#include "PerlinVertexShader.h"
#include <memory>

namespace bbfx {

class PerlinFxNode : public AnimationNode {
public:
    PerlinFxNode(const string& meshName, const string& cloneName);
    virtual ~PerlinFxNode();
    void update() override;
    std::string getTypeName() const override { return "PerlinFxNode"; }
    void enable();
    void disable();
    void cleanup() override;

    /// Set by the Studio factory so cleanup() can destroy the OGRE objects
    void setStudioSceneNode(const std::string& entityName, const std::string& sceneNodeName) {
        mStudioEntityName = entityName;
        mStudioSceneNodeName = sceneNodeName;
    }

    /// Called by update() to create the Entity once the clone mesh is ready
    void createDeferredEntity();

private:
    std::unique_ptr<PerlinVertexShader> mShader;
    std::string mStudioEntityName;
    std::string mStudioSceneNodeName;
    std::string mCloneMeshName;
    bool mEntityCreated = false;
};

} // namespace bbfx

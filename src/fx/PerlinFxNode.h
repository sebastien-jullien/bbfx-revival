#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include "PerlinVertexShader.h"
#include <memory>
#include <map>
#include <vector>

namespace bbfx {

class PerlinFxNode : public AnimationNode {
public:
    PerlinFxNode(const string& defaultMesh, const string& clonePrefix, const std::string& nodeName = "PerlinFxNode");
    virtual ~PerlinFxNode();
    void update() override;
    void setEnabled(bool en) override;
    std::string getTypeName() const override { return "PerlinFxNode"; }
    void enable();
    void disable();
    void cleanup() override;
    void onLinkChanged() override;

    /// Set by the Studio factory so cleanup() can destroy the OGRE objects
    void setStudioSceneNode(const std::string& entityName, const std::string& sceneNodeName) {
        mDefaultEntityName = entityName;
        mDefaultSceneNodeName = sceneNodeName;
    }

    /// Called by update() to create Entities once clone meshes are ready
    void createDeferredEntities();

private:
    /// Per-target clone data
    struct FxClone {
        std::unique_ptr<PerlinVertexShader> shader;
        std::string cloneMeshName;
        std::string entityName;
        std::string sceneNodeName;
        bool entityCreated = false;
    };

    std::map<std::string, FxClone> mClones;  // key = target node name
    std::vector<std::string> mTargetNodeNames;
    std::string mClonePrefix;
    std::string mDefaultMesh;
    std::string mDefaultEntityName;
    std::string mDefaultSceneNodeName;
    ParamSpec mSpec;

    void resolveTargets();
    void addCloneForTarget(const std::string& targetName);
    void removeClone(const std::string& targetName);
    void setCloneVisible(const std::string& targetName, bool vis);
    class SceneObjectNode* findTargetSceneObj(const std::string& targetName);
};

} // namespace bbfx

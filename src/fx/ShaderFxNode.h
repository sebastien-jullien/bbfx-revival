#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreMaterial.h>
#include <OgreGpuProgramParams.h>
#include <string>
#include <vector>

namespace Ogre {
    class SceneManager;
    class Entity;
}

namespace bbfx {

/// GPU shader effect node.
/// Loads a GLSL vertex shader, creates an OGRE material,
/// and exposes each float uniform as a DAG input port.
/// Each frame, update() pushes port values to GPU uniforms.
/// Target entity is resolved by name from a linked SceneObjectNode.
class ShaderFxNode : public AnimationNode {
public:
    /// @param name        Node name for the DAG
    /// @param vertPath    Path to the vertex shader .glsl (relative to resources)
    /// @param fragPath    Path to the fragment shader .frag (relative to resources, can be empty for default)
    /// @param scene       OGRE SceneManager
    ShaderFxNode(const std::string& name,
                 const std::string& vertPath,
                 const std::string& fragPath,
                 Ogre::SceneManager* scene);

    ~ShaderFxNode() override = default;
    void update() override;
    void setEnabled(bool en) override;
    void cleanup() override;
    std::string getTypeName() const override { return "ShaderFxNode"; }

    Ogre::MaterialPtr getMaterial() const { return mMaterial; }

private:
    void loadShader(const std::string& vertPath, const std::string& fragPath);
    void parseUniforms(const std::string& shaderSource);
    void resolveTarget();
    void applyToEntity(Ogre::Entity* entity);
    void detachFromEntity();

    Ogre::SceneManager* mScene;
    Ogre::Entity* mEntity = nullptr;
    std::string mTargetNodeName;
    Ogre::MaterialPtr mMaterial;
    Ogre::GpuProgramParametersSharedPtr mVertParams;
    Ogre::GpuProgramParametersSharedPtr mFragParams;
    ParamSpec mSpec;

    struct UniformInfo {
        std::string name;
        float defaultValue;
    };
    std::vector<UniformInfo> mUniforms;

    float mTime = 0.0f;
    std::vector<std::string> mOriginalMaterials; // per sub-entity
};

} // namespace bbfx

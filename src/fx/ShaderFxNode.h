#pragma once

#include "../core/AnimationNode.h"
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
class ShaderFxNode : public AnimationNode {
public:
    /// @param name        Node name for the DAG
    /// @param vertPath    Path to the vertex shader .glsl (relative to resources)
    /// @param fragPath    Path to the fragment shader .frag (relative to resources, can be empty for default)
    /// @param scene       OGRE SceneManager
    /// @param entity      Target entity to apply the material to
    ShaderFxNode(const std::string& name,
                 const std::string& vertPath,
                 const std::string& fragPath,
                 Ogre::SceneManager* scene,
                 Ogre::Entity* entity);

    ~ShaderFxNode() override = default;
    void update() override;
    std::string getTypeName() const override { return "ShaderFxNode"; }

    Ogre::MaterialPtr getMaterial() const { return mMaterial; }

private:
    void loadShader(const std::string& vertPath, const std::string& fragPath);
    void parseUniforms(const std::string& shaderSource);

    Ogre::SceneManager* mScene;
    Ogre::Entity* mEntity;
    Ogre::MaterialPtr mMaterial;
    Ogre::GpuProgramParametersSharedPtr mVertParams;
    Ogre::GpuProgramParametersSharedPtr mFragParams;

    struct UniformInfo {
        std::string name;
        float defaultValue;
    };
    std::vector<UniformInfo> mUniforms;

    float mTime = 0.0f;
};

} // namespace bbfx

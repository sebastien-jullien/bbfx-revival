#include "MeshImporter.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>

#include <OgreEntity.h>
#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreSubMesh.h>

#ifdef BBFX_HAS_ASSIMP
#  include <assimp/Importer.hpp>
#  include <assimp/postprocess.h>
#  include <assimp/scene.h>
#endif

namespace bbfx {

bool MeshImporter::isAvailable() {
#ifdef BBFX_HAS_ASSIMP
    return true;
#else
    return false;
#endif
}

namespace {
std::unordered_map<std::string, std::string>& cache() {
    static std::unordered_map<std::string, std::string> m;
    return m;
}
} // anonymous

std::string MeshImporter::import(const std::string& path) {
#ifndef BBFX_HAS_ASSIMP
    (void)path;
    std::cerr << "[MeshImporter] Assimp not compiled in (BBFX_HAS_ASSIMP); "
                 "3D model import disabled." << std::endl;
    return {};
#else
    if (path.empty()) return {};
    if (auto it = cache().find(path); it != cache().end()) return it->second;

    Assimp::Importer importer;
    unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                     aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs;
    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->HasMeshes()) {
        std::cerr << "[MeshImporter] import(" << path << ") failed: "
                   << importer.GetErrorString() << std::endl;
        return {};
    }

    std::string stem = std::filesystem::path(path).stem().string();
    std::string meshName = "bbfx_imported_" + stem;

    try {
        Ogre::ManualObject mo(meshName);
        // One section per submesh.
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh* m = scene->mMeshes[i];
            if (!m || m->mNumVertices == 0) continue;
            std::string mat = "BaseWhiteNoLighting";  // default
            mo.begin(mat, Ogre::RenderOperation::OT_TRIANGLE_LIST);
            for (unsigned v = 0; v < m->mNumVertices; ++v) {
                const auto& p = m->mVertices[v];
                mo.position(p.x, p.y, p.z);
                if (m->HasNormals()) {
                    const auto& n = m->mNormals[v];
                    mo.normal(n.x, n.y, n.z);
                }
                if (m->HasTextureCoords(0)) {
                    const auto& t = m->mTextureCoords[0][v];
                    mo.textureCoord(t.x, t.y);
                } else {
                    mo.textureCoord(0.0f, 0.0f);
                }
            }
            for (unsigned f = 0; f < m->mNumFaces; ++f) {
                const aiFace& face = m->mFaces[f];
                if (face.mNumIndices != 3) continue;
                mo.triangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            }
            mo.end();
        }
        auto mesh = mo.convertToMesh(meshName,
                      Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (mesh) {
            cache()[path] = meshName;
            return meshName;
        }
    } catch (const std::exception& e) {
        std::cerr << "[MeshImporter] OGRE conversion failed: " << e.what() << std::endl;
    }
    return {};
#endif
}

} // namespace bbfx

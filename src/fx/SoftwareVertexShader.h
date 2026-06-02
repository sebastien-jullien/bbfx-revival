#pragma once

#include "../bbfx.h"
#include <OgreFrameListener.h>
#include <OgreMesh.h>
#include <OgreSubMesh.h>
#include <OgreMeshManager.h>
#include <OgreHardwareBufferManager.h>
#include <OgreRoot.h>
#include <vector>

namespace bbfx {

using namespace Ogre;

/// CPU-side copy of vertex/index data for reading without GPU locks
struct CpuMeshData {
    std::vector<float> positions;   // 3 floats per vertex
    std::vector<uint32_t> indices;  // triangle indices
    size_t vertexCount = 0;
    size_t indexCount = 0;
};

class SoftwareVertexShader : public Ogre::FrameListener {
public:
    SoftwareVertexShader(const String& meshName, const String& cloneName);
    virtual ~SoftwareVertexShader();

    bool frameStarted(const Ogre::FrameEvent& e) override;

    void enable();
    void disable();

    virtual void renderOneFrame(Real dt) = 0;

    /// Mode d'avance temporelle. Par défaut (false), le FrameListener OGRE appelle
    /// renderOneFrame(dt_réel) chaque frame (auto-animé). Quand un node DAG veut
    /// piloter le temps depuis son port `dt` (pause/scrub/speed), il passe ce flag
    /// à true : frameStarted() n'avance plus, et le node appelle renderOneFrame(dt)
    /// lui-même. Évite le double-avancement.
    void setDagDrivenTime(bool b) { mDagDrivenTime = b; }
    bool isDagDrivenTime() const { return mDagDrivenTime; }

protected:
    String mMeshName;
    MeshPtr originalMesh;
    MeshPtr clonedMesh;
    bool mCloneReady = false; // deferred: clone prepared at first frameStarted()
    bool mDagDrivenTime = false; // true = un node DAG pilote renderOneFrame() (cf. setDagDrivenTime)

    // CPU copies of original vertex data per submesh (+ shared)
    CpuMeshData mSharedCpuData;
    std::vector<CpuMeshData> mSubMeshCpuData;

    void _loadMesh(const String& meshName);
    void _prepareClonedMesh();
    VertexData* _prepareVertexData(VertexData* data0);
    HardwareVertexBufferSharedPtr _cloneBuffer(HardwareVertexBufferSharedPtr buf0);
    void _reorganizeVertexBuffers(VertexData* data0);
    bool _isDynamic(VertexElementSemantic semantic);

    void _extractCpuData(VertexData* vdata, IndexData* idata, CpuMeshData& out);
    void _extractPositions(VertexData* vdata, CpuMeshData& out);
    void _fillCloneFromCpu(VertexData* cloneData, VertexData* origData, const CpuMeshData& cpuData);
};

} // namespace bbfx

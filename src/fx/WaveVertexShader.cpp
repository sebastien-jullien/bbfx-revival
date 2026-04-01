#include "WaveVertexShader.h"
#include "../core/Engine.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
#include <cmath>
#include <cstring>
#include <iostream>

namespace bbfx {

using namespace Ogre;

WaveVertexShader::WaveVertexShader(const String& meshName, const String& cloneName)
    : SoftwareVertexShader(meshName, cloneName)
    , AnimationNode("WaveVertexShader")
    , mCloneMeshName(cloneName)
{
    AnimationNode::addInput(new AnimationPort("dt", 0.016f));
    AnimationNode::addInput(new AnimationPort("amplitude", 5.0f));
    AnimationNode::addInput(new AnimationPort("frequency", 2.0f));
    AnimationNode::addInput(new AnimationPort("speed", 1.0f));
    AnimationNode::addInput(new AnimationPort("axis", 1.0f));
    AnimationNode::addOutput(new AnimationPort("mesh_dirty", 0.0f));
}

WaveVertexShader::~WaveVertexShader() = default;

void WaveVertexShader::createDeferredEntity() {
    if (mEntityCreated || mStudioEntityName.empty()) return;
    auto cloneMesh = Ogre::MeshManager::getSingleton().getByName(mCloneMeshName);
    if (!cloneMesh) return; // clone not ready yet
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr) return;
    try {
        auto* entity = sceneMgr->createEntity(mStudioEntityName, mCloneMeshName);
        auto* sceneNode = sceneMgr->getRootSceneNode()->createChildSceneNode(mStudioSceneNodeName);
        sceneNode->attachObject(entity);
        mEntityCreated = true;
    } catch (...) {}
}

void WaveVertexShader::cleanup() {
    SoftwareVertexShader::disable();
    if (!mStudioSceneNodeName.empty()) {
        try {
            auto* engine = Engine::instance();
            auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
            if (sceneMgr && sceneMgr->hasSceneNode(mStudioSceneNodeName)) {
                sceneMgr->getSceneNode(mStudioSceneNodeName)->setVisible(false);
            }
        } catch (...) {}
    }
}

void WaveVertexShader::update() {
    createDeferredEntity();
    if (!mCloneReady) return; // Wait for SoftwareVertexShader to prepare the clone in frameStarted()

    auto& inputs = AnimationNode::getInputs();
    float dt = inputs.at("dt")->getValue();
    if (dt <= 0.0f) dt = 0.016f; // fallback
    amplitude = inputs.at("amplitude")->getValue();
    frequency = inputs.at("frequency")->getValue();
    speed = inputs.at("speed")->getValue();
    axis = static_cast<int>(inputs.at("axis")->getValue());
    if (axis < 0) axis = 0;
    if (axis > 2) axis = 2;

    renderOneFrame(dt);
    AnimationNode::getOutputs().at("mesh_dirty")->setValue(1.0f);
    AnimationNode::fireUpdate();
}

void WaveVertexShader::_applyWave(VertexData* data, const CpuMeshData& cpuData) {
    const float* srcPos = cpuData.positions.data();
    size_t nVerts = cpuData.vertexCount;
    if (nVerts == 0) return;

    size_t needed = nVerts * 3;
    if (mDstPos.size() < needed) {
        mDstPos.resize(needed);
        mNormals.resize(needed);
    }

    float* dstPos = mDstPos.data();
    for (size_t i = 0; i < needed; i += 3) {
        dstPos[i]   = srcPos[i];
        dstPos[i+1] = srcPos[i+1];
        dstPos[i+2] = srcPos[i+2];
        // Apply wave displacement on the chosen axis
        dstPos[i + axis] += amplitude * std::sin(frequency * srcPos[i] + speed * time);
    }

    // Recalculate normals
    float* norms = mNormals.data();
    std::memset(norms, 0, needed * sizeof(float));
    const uint32_t* indices = cpuData.indices.data();
    size_t numFaces = cpuData.indexCount / 3;
    for (size_t f = 0; f < numFaces; f++) {
        uint32_t p0 = indices[f*3], p1 = indices[f*3+1], p2 = indices[f*3+2];
        Vector3 v0(dstPos[3*p0], dstPos[3*p0+1], dstPos[3*p0+2]);
        Vector3 v1(dstPos[3*p1], dstPos[3*p1+1], dstPos[3*p1+2]);
        Vector3 v2(dstPos[3*p2], dstPos[3*p2+1], dstPos[3*p2+2]);
        Vector3 fn = (v1 - v2).crossProduct(v1 - v0);
        norms[3*p0] += fn.x; norms[3*p0+1] += fn.y; norms[3*p0+2] += fn.z;
        norms[3*p1] += fn.x; norms[3*p1+1] += fn.y; norms[3*p1+2] += fn.z;
        norms[3*p2] += fn.x; norms[3*p2+1] += fn.y; norms[3*p2+2] += fn.z;
    }
    for (size_t i = 0; i < needed; i += 3) {
        Vector3 n(norms[i], norms[i+1], norms[i+2]);
        n.normalise();
        norms[i] = n.x; norms[i+1] = n.y; norms[i+2] = n.z;
    }

    // Write back
    const auto* posElem = data->vertexDeclaration->findElementBySemantic(VES_POSITION);
    const auto* normElem = data->vertexDeclaration->findElementBySemantic(VES_NORMAL);
    if (!posElem) return;

    auto posBuf = data->vertexBufferBinding->getBuffer(posElem->getSource());
    size_t vertexSize = posBuf->getVertexSize();
    size_t posOffset = posElem->getOffset();

    uint8_t* raw = static_cast<uint8_t*>(posBuf->lock(HardwareBuffer::HBL_DISCARD));
    for (size_t v = 0; v < nVerts; v++) {
        float* dst = reinterpret_cast<float*>(raw + v * vertexSize + posOffset);
        dst[0] = dstPos[v*3]; dst[1] = dstPos[v*3+1]; dst[2] = dstPos[v*3+2];
    }

    if (normElem && normElem->getSource() == posElem->getSource()) {
        size_t normOffset = normElem->getOffset();
        for (size_t v = 0; v < nVerts; v++) {
            float* dst = reinterpret_cast<float*>(raw + v * vertexSize + normOffset);
            dst[0] = norms[v*3]; dst[1] = norms[v*3+1]; dst[2] = norms[v*3+2];
        }
    }
    posBuf->unlock();

    if (normElem && normElem->getSource() != posElem->getSource()) {
        auto normBuf = data->vertexBufferBinding->getBuffer(normElem->getSource());
        size_t nVertexSize = normBuf->getVertexSize();
        size_t nOff = normElem->getOffset();
        uint8_t* nraw = static_cast<uint8_t*>(normBuf->lock(HardwareBuffer::HBL_DISCARD));
        for (size_t v = 0; v < nVerts; v++) {
            float* dst = reinterpret_cast<float*>(nraw + v * nVertexSize + nOff);
            dst[0] = norms[v*3]; dst[1] = norms[v*3+1]; dst[2] = norms[v*3+2];
        }
        normBuf->unlock();
    }
}

void WaveVertexShader::renderOneFrame(Real dt) {
    for (unsigned m = 0; m < clonedMesh->getNumSubMeshes(); m++) {
        auto* subMesh = clonedMesh->getSubMesh(m);
        VertexData* vdata = subMesh->useSharedVertices
            ? clonedMesh->sharedVertexData : subMesh->vertexData;
        if (vdata) {
            _applyWave(vdata, mSubMeshCpuData[m]);
        }
    }
    time += dt;
}

} // namespace bbfx

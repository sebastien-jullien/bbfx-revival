#include "SceneCommands.h"
#include "NodeCommands.h"
#include "../../core/Animator.h"
#include "../NodeTypeRegistry.h"
#include "../nodes/SceneObjectNode.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sol/sol.hpp>

namespace bbfx {

// ─── SceneObjectNamer ────────────────────────────────────────────────────────

static std::string meshToBaseName(const std::string& meshFile) {
    // Known mapping table
    static const std::map<std::string, std::string> kMapping = {
        {"ogrehead.mesh",       "Ogre"},
        {"geosphere4500.mesh",  "Geosphere"},
        {"geosphere8000.mesh",  "Geosphere"},
        {"torus.mesh",          "Torus"},
        {"cylinder.mesh",       "Cylinder"},
        {"cone.mesh",           "Cone"},
        {"plane_1m.mesh",       "Plane"},
        {"ninja.mesh",          "Ninja"},
        {"torusknot.mesh",      "TorusKnot"},
        {"cube_1m.mesh",        "Cube"},
    };

    auto it = kMapping.find(meshFile);
    if (it != kMapping.end()) return it->second;

    // Fallback: extract filename without extension, capitalize first letter
    std::string base = meshFile;
    auto dotPos = base.rfind('.');
    if (dotPos != std::string::npos) base = base.substr(0, dotPos);
    // Remove trailing digits
    while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back())))
        base.pop_back();
    // Remove trailing underscore
    while (!base.empty() && base.back() == '_')
        base.pop_back();
    if (base.empty()) base = "Object";
    // Capitalize first letter
    base[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[0])));
    return base;
}

std::string generateSceneObjectName(const std::string& meshFile, Animator* animator) {
    std::string baseName = meshToBaseName(meshFile);

    if (!animator) return baseName;

    // Check if base name is available
    if (!animator->getRegisteredNode(baseName)) return baseName;

    // Find first available suffix .001, .002, ...
    for (int i = 1; i < 1000; ++i) {
        std::ostringstream oss;
        oss << baseName << "." << std::setw(3) << std::setfill('0') << i;
        std::string candidate = oss.str();
        if (!animator->getRegisteredNode(candidate)) return candidate;
    }

    // Extreme fallback
    static int sFallbackCounter = 0;
    return baseName + "_" + std::to_string(++sFallbackCounter);
}

// ─── DuplicateNodeCommand ────────────────────────────────────────────────────

DuplicateNodeCommand::DuplicateNodeCommand(const std::string& sourceNodeName, sol::state& lua)
    : mSourceName(sourceNodeName), mLua(lua)
{
}

void DuplicateNodeCommand::execute() {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* sourceNode = animator->getRegisteredNode(mSourceName);
    if (!sourceNode) return;

    mTypeName = sourceNode->getTypeName();

    // Save params from source
    if (!mExecuted && sourceNode->getParamSpec()) {
        mParams = sourceNode->getParamSpec()->getParams();
    }

    // Generate name on first execution
    if (!mExecuted) {
        std::string meshFile = "geosphere4500.mesh";
        if (sourceNode->getParamSpec()) {
            auto* meshParam = sourceNode->getParamSpec()->getParam("mesh_file");
            if (meshParam && !meshParam->stringVal.empty())
                meshFile = meshParam->stringVal;
        }
        mNewName = generateSceneObjectName(meshFile, animator);
        mExecuted = true;
    }

    // Create the node
    auto* newNode = NodeTypeRegistry::instance().create(mTypeName, mNewName, mLua);
    if (!newNode) return;

    // Copy params
    if (newNode->getParamSpec()) {
        for (auto& pd : mParams) {
            auto* param = newNode->getParamSpec()->getParam(pd.name);
            if (param) *param = pd;
        }
    }

    // Offset position
    auto& inputs = const_cast<AnimationNode::Ports&>(newNode->getInputs());
    auto posIt = inputs.find("position.x");
    if (posIt != inputs.end()) {
        float srcX = 0;
        auto& srcInputs = sourceNode->getInputs();
        auto srcPosIt = srcInputs.find("position.x");
        if (srcPosIt != srcInputs.end()) srcX = srcPosIt->second->getValue();
        posIt->second->setValue(srcX + 2.0f);
    }
    // Copy Y and Z from source
    for (const char* port : {"position.y", "position.z"}) {
        auto dstIt = inputs.find(port);
        auto srcIt = sourceNode->getInputs().find(port);
        if (dstIt != inputs.end() && srcIt != sourceNode->getInputs().end())
            dstIt->second->setValue(srcIt->second->getValue());
    }
}

void DuplicateNodeCommand::undo() {
    if (mNewName.empty()) return;
    // Queue for deferred deletion (same pattern as DeleteNodeCommand)
    gPendingDeletes.push_back(mNewName);
}

std::string DuplicateNodeCommand::description() const {
    return "Duplicate " + mSourceName + " → " + mNewName;
}

// ─── ReparentNodeCommand ─────────────────────────────────────────────────────

ReparentNodeCommand::ReparentNodeCommand(const std::string& childName,
                                         const std::string& newParentName,
                                         const std::string& oldParentName)
    : mChildName(childName), mNewParent(newParentName), mOldParent(oldParentName)
{
}

static void doReparent(const std::string& childName, const std::string& parentName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* childNode = dynamic_cast<SceneObjectNode*>(animator->getRegisteredNode(childName));
    if (!childNode || !childNode->getSceneNode()) return;

    auto* childSN = childNode->getSceneNode();

    if (parentName.empty()) {
        // Unparent: move back to root
        auto* currentParent = childSN->getParentSceneNode();
        if (currentParent && currentParent != childNode->getSceneNode()->getCreator()->getRootSceneNode()) {
            // Convert local to world
            auto worldPos = childSN->_getDerivedPosition();
            auto worldOri = childSN->_getDerivedOrientation();
            auto worldScale = childSN->_getDerivedScale();
            currentParent->removeChild(childSN);
            childNode->getSceneNode()->getCreator()->getRootSceneNode()->addChild(childSN);
            childSN->setPosition(worldPos);
            childSN->setOrientation(worldOri);
            childSN->setScale(worldScale);
        }
        // Clear parent param
        if (childNode->getParamSpec()) {
            auto* p = childNode->getParamSpec()->getParam("parent_node");
            if (p) p->stringVal = "";
        }
    } else {
        auto* parentNode = dynamic_cast<SceneObjectNode*>(animator->getRegisteredNode(parentName));
        if (!parentNode || !parentNode->getSceneNode()) return;
        auto* parentSN = parentNode->getSceneNode();

        // Save world transform
        auto worldPos = childSN->_getDerivedPosition();
        auto worldOri = childSN->_getDerivedOrientation();

        // Detach from current parent
        auto* currentParent = childSN->getParentSceneNode();
        if (currentParent) currentParent->removeChild(childSN);

        // Attach to new parent
        parentSN->addChild(childSN);

        // Convert world to local
        auto parentWorldOri = parentSN->_getDerivedOrientation();
        auto parentWorldPos = parentSN->_getDerivedPosition();
        auto parentWorldScale = parentSN->_getDerivedScale();

        auto localPos = parentWorldOri.Inverse() * (worldPos - parentWorldPos);
        if (parentWorldScale.x > 0.001f) localPos.x /= parentWorldScale.x;
        if (parentWorldScale.y > 0.001f) localPos.y /= parentWorldScale.y;
        if (parentWorldScale.z > 0.001f) localPos.z /= parentWorldScale.z;
        auto localOri = parentWorldOri.Inverse() * worldOri;

        childSN->setPosition(localPos);
        childSN->setOrientation(localOri);

        // Set parent param
        if (childNode->getParamSpec()) {
            auto* p = childNode->getParamSpec()->getParam("parent_node");
            if (p) p->stringVal = parentName;
        }
    }

    // Update DAG ports to match local position
    auto& inputs = const_cast<AnimationNode::Ports&>(childNode->getInputs());
    auto pos = childSN->getPosition();
    if (inputs.count("position.x")) inputs["position.x"]->setValue(pos.x);
    if (inputs.count("position.y")) inputs["position.y"]->setValue(pos.y);
    if (inputs.count("position.z")) inputs["position.z"]->setValue(pos.z);
}

void ReparentNodeCommand::execute() {
    doReparent(mChildName, mNewParent);
}

void ReparentNodeCommand::undo() {
    doReparent(mChildName, mOldParent);
}

std::string ReparentNodeCommand::description() const {
    if (mNewParent.empty())
        return "Unparent " + mChildName;
    return "Reparent " + mChildName + " under " + mNewParent;
}

} // namespace bbfx

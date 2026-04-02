#include "SceneHierarchyPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../nodes/SceneObjectNode.h"
#include "../commands/CommandManager.h"
#include "../commands/NodeCommands.h"
#include "../commands/SceneCommands.h"
#include <imgui.h>
#include <algorithm>
#include <vector>

namespace bbfx {

SceneHierarchyPanel::SceneHierarchyPanel(Animator* animator)
    : mAnimator(animator)
{
}

bool SceneHierarchyPanel::isSceneNode(AnimationNode* node) const {
    if (!node) return false;
    auto type = node->getTypeName();
    return type == "SceneObjectNode" || type == "LightNode" ||
           type == "ParticleNode" || type == "CameraNode" ||
           type == "SkyboxNode" || type == "FogNode" ||
           type == "CompositorNode";
}

const char* SceneHierarchyPanel::getIconForType(const std::string& typeName) const {
    if (typeName == "SceneObjectNode") return "[M]";
    if (typeName == "LightNode")       return "[L]";
    if (typeName == "ParticleNode")    return "[P]";
    if (typeName == "CameraNode")      return "[C]";
    if (typeName == "SkyboxNode")      return "[S]";
    if (typeName == "FogNode")         return "[F]";
    if (typeName == "CompositorNode")  return "[X]";
    return "[?]";
}

void SceneHierarchyPanel::selectNodeFromExternal(const std::string& name) {
    mUpdatingFromExternal = true;
    mSelectedNode = name;
    mUpdatingFromExternal = false;
}

void SceneHierarchyPanel::renderItem(AnimationNode* node, const std::string& name) {
    if (!node) return;

    auto type = node->getTypeName();
    const char* icon = getIconForType(type);

    ImGui::PushID(name.c_str());

    // Visibility toggle
    bool visible = node->isUserVisible();
    if (ImGui::SmallButton(visible ? "O" : "o")) {
        node->setUserVisible(!visible);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(visible ? "Hide" : "Show");
    ImGui::SameLine();

    // Lock toggle
    bool locked = node->isLocked();
    if (ImGui::SmallButton(locked ? "#" : ".")) {
        node->setLocked(!locked);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(locked ? "Unlock" : "Lock");
    ImGui::SameLine();

    // Selectable item
    bool isSelected = (mSelectedNode == name);
    char label[256];
    snprintf(label, sizeof(label), "%s %s", icon, name.c_str());

    if (ImGui::Selectable(label, isSelected)) {
        mSelectedNode = name;
        if (!mUpdatingFromExternal && mSelectionCallback) {
            mSelectionCallback(name);
        }
    }

    // Drag source for reparenting
    if (type == "SceneObjectNode" && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("HIERARCHY_NODE", name.c_str(), name.size() + 1);
        ImGui::Text("Move %s", name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target for reparenting
    if (type == "SceneObjectNode" && ImGui::BeginDragDropTarget()) {
        if (auto* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
            std::string childName(static_cast<const char*>(payload->Data));
            if (childName != name) {
                // Get old parent
                std::string oldParent;
                auto* childNode = mAnimator->getRegisteredNode(childName);
                if (childNode && childNode->getParamSpec()) {
                    auto* p = childNode->getParamSpec()->getParam("parent_node");
                    if (p) oldParent = p->stringVal;
                }
                CommandManager::instance().execute(
                    std::make_unique<ReparentNodeCommand>(childName, name, oldParent));
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Focus")) {
            mSelectedNode = name;
            if (mSelectionCallback) mSelectionCallback(name);
            // Focus is handled by StudioApp via the selection callback
        }
        if (ImGui::MenuItem(visible ? "Hide" : "Show")) {
            node->setUserVisible(!visible);
        }
        if (ImGui::MenuItem(locked ? "Unlock" : "Lock")) {
            node->setLocked(!locked);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            mSelectedNode = name;
            if (mSelectionCallback) mSelectionCallback(name);
            // Use deferred deletion
            gPendingDeletes.push_back(name);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void SceneHierarchyPanel::render() {
    ImGui::Begin("Scene Hierarchy");

    if (!mAnimator) {
        ImGui::TextDisabled("No animator");
        ImGui::End();
        return;
    }

    auto names = mAnimator->getRegisteredNodeNames();
    std::sort(names.begin(), names.end());

    // Drop target on empty area — unparent
    // First render root-level nodes (no parent or empty parent)
    bool hasAny = false;
    for (auto& name : names) {
        auto* node = mAnimator->getRegisteredNode(name);
        if (!node || !isSceneNode(node)) continue;

        // Check if this is a root node (no parent)
        std::string parentName;
        if (node->getParamSpec()) {
            auto* p = node->getParamSpec()->getParam("parent_node");
            if (p) parentName = p->stringVal;
        }

        if (!parentName.empty()) {
            // Check if parent still exists
            if (mAnimator->getRegisteredNode(parentName)) continue; // will be rendered under parent
            // Parent doesn't exist, treat as root
        }

        hasAny = true;
        renderItem(node, name);

        // Render children indented
        for (auto& childName : names) {
            auto* childNode = mAnimator->getRegisteredNode(childName);
            if (!childNode || !isSceneNode(childNode)) continue;
            std::string childParent;
            if (childNode->getParamSpec()) {
                auto* p = childNode->getParamSpec()->getParam("parent_node");
                if (p) childParent = p->stringVal;
            }
            if (childParent == name) {
                ImGui::Indent(20.0f);
                renderItem(childNode, childName);
                ImGui::Unindent(20.0f);
            }
        }
    }

    if (!hasAny) {
        ImGui::TextDisabled("No scene objects");
    }

    // Drop target on empty area — unparent to root
    if (ImGui::BeginDragDropTarget()) {
        if (auto* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
            std::string childName(static_cast<const char*>(payload->Data));
            auto* childNode = mAnimator->getRegisteredNode(childName);
            std::string oldParent;
            if (childNode && childNode->getParamSpec()) {
                auto* p = childNode->getParamSpec()->getParam("parent_node");
                if (p) oldParent = p->stringVal;
            }
            if (!oldParent.empty()) {
                CommandManager::instance().execute(
                    std::make_unique<ReparentNodeCommand>(childName, "", oldParent));
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

} // namespace bbfx

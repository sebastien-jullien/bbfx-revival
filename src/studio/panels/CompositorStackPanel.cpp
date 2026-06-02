#include "CompositorStackPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../commands/NodeCommands.h"  // gPendingDeletes
#include <OgreCompositorManager.h>
#include <OgreViewport.h>
#include <imgui.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

void CompositorStackPanel::syncStackOrder() {
    if (!mAnimator) return;

    // Scan DAG for all CompositorNodes
    std::vector<std::string> dagComps;
    for (auto& name : mAnimator->getRegisteredNodeNames()) {
        auto* node = mAnimator->getRegisteredNode(name);
        if (node && node->getTypeName() == "CompositorNode") {
            dagComps.push_back(name);
        }
    }
    // Add new ones not in stack
    for (auto& name : dagComps) {
        if (std::find(mStackOrder.begin(), mStackOrder.end(), name) == mStackOrder.end()) {
            mStackOrder.push_back(name);
            mDirty = true;
        }
    }
    // Remove deleted ones from stack
    auto oldSize = mStackOrder.size();
    mStackOrder.erase(std::remove_if(mStackOrder.begin(), mStackOrder.end(),
        [&](const std::string& s) {
            return std::find(dagComps.begin(), dagComps.end(), s) == dagComps.end();
        }), mStackOrder.end());
    if (mStackOrder.size() != oldSize) mDirty = true;
}

void CompositorStackPanel::render() {
    ImGui::Begin("Compositor Stack");

    if (!mAnimator) {
        ImGui::TextDisabled("No animator");
        ImGui::End();
        return;
    }

    syncStackOrder();

    if (mStackOrder.empty()) {
        ImGui::TextDisabled("No compositors in scene");
        // Drop target for adding compositors
        if (ImGui::BeginDragDropTarget()) {
            if (auto* payload = ImGui::AcceptDragDropPayload("COMPOSITOR_NAME")) {
                (void)payload; // Node creation handled by source
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Compositor Chain (%zu)", mStackOrder.size());

    for (int i = 0; i < static_cast<int>(mStackOrder.size()); ++i) {
        auto& nodeName = mStackOrder[i];
        auto* node = mAnimator->getRegisteredNode(nodeName);
        if (!node) continue;

        auto* spec = node->getParamSpec();
        if (!spec) continue;

        auto* compParam = spec->getParam("compositor");
        auto* enabledParam = spec->getParam("enabled");
        std::string compName = compParam ? compParam->stringVal : nodeName;
        bool enabled = enabledParam ? enabledParam->boolVal : true;

        ImGui::PushID(i);

        // Drag source for reorder
        ImGui::Button("::", {20, 0});
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("COMP_REORDER", &i, sizeof(int));
            ImGui::Text("Move: %s", compName.c_str());
            ImGui::EndDragDropSource();
        }
        // Drop target for reorder
        if (ImGui::BeginDragDropTarget()) {
            if (auto* payload = ImGui::AcceptDragDropPayload("COMP_REORDER")) {
                int srcIdx = *static_cast<const int*>(payload->Data);
                if (srcIdx != i && srcIdx >= 0 && srcIdx < static_cast<int>(mStackOrder.size())) {
                    std::string moved = mStackOrder[srcIdx];
                    mStackOrder.erase(mStackOrder.begin() + srcIdx);
                    int insertAt = (srcIdx < i) ? i - 1 : i;
                    mStackOrder.insert(mStackOrder.begin() + insertAt, moved);
                    mDirty = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();

        // Eye icon (enable/bypass) — Shift+click = solo
        bool eyeClicked = false;
        if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, {0.4f, 0.4f, 0.4f, 1.0f});
        if (ImGui::SmallButton(enabled ? "O##eye" : "-##eye")) {
            eyeClicked = true;
        }
        if (!enabled) ImGui::PopStyleColor();

        if (eyeClicked) {
            bool shiftHeld = ImGui::GetIO().KeyShift;
            if (shiftHeld) {
                // Solo mode
                if (!mSoloActive) {
                    // Save all states and solo this one
                    mSavedStates.clear();
                    for (auto& sn : mStackOrder) {
                        auto* n = mAnimator->getRegisteredNode(sn);
                        if (n && n->getParamSpec()) {
                            auto* ep = n->getParamSpec()->getParam("enabled");
                            mSavedStates.push_back({sn, ep ? ep->boolVal : true});
                            if (ep) ep->boolVal = (sn == nodeName);
                        }
                    }
                    mSoloActive = true;
                } else {
                    // Restore saved states
                    for (auto& [sn, val] : mSavedStates) {
                        auto* n = mAnimator->getRegisteredNode(sn);
                        if (n && n->getParamSpec()) {
                            auto* ep = n->getParamSpec()->getParam("enabled");
                            if (ep) ep->boolVal = val;
                        }
                    }
                    mSoloActive = false;
                    mSavedStates.clear();
                }
            } else {
                if (enabledParam) enabledParam->boolVal = !enabled;
            }
            mDirty = true;
        }

        ImGui::SameLine();

        // Compositor name
        if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.5f, 0.5f, 1.0f});
        ImGui::Text("%s", compName.c_str());
        if (!enabled) ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);

        // Delete button
        if (ImGui::SmallButton("X##del")) {
            // M9 — supprimer réellement le node du DAG. Avant, on ne vidait que
            // mStackOrder, mais syncStackOrder() le ré-ajoutait la frame suivante
            // (le node existait toujours) → bouton mort. On passe par gPendingDeletes
            // (même mécanisme que la suppression UI standard).
            gPendingDeletes.push_back(nodeName);
            mStackOrder.erase(mStackOrder.begin() + i);
            mDirty = true;
            ImGui::PopID();
            break; // Restart loop next frame
        }

        // Inline params — show up to 3 float params
        if (enabled) {
            int paramCount = 0;
            for (auto& pdef : spec->getParams()) {
                if (pdef.type == ParamType::FLOAT && paramCount < 3 &&
                    pdef.name != "enabled") {
                    auto* mp = spec->getParam(pdef.name);
                    if (mp) {
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::SliderFloat(("##" + pdef.name).c_str(), &mp->floatVal,
                                           mp->minVal, mp->maxVal, "%.2f");
                        paramCount++;
                    }
                }
            }
        }

        ImGui::PopID();
        ImGui::Separator();
    }

    // Drop target at bottom of stack for adding compositors from browser
    ImGui::TextDisabled("Drop compositor here to add...");
    if (ImGui::BeginDragDropTarget()) {
        if (auto* payload = ImGui::AcceptDragDropPayload("COMPOSITOR_NAME")) {
            std::string compName(static_cast<const char*>(payload->Data));
            // The compositor node should already exist in the DAG (created by browser drag)
            // Just ensure it's in our stack order
            if (std::find(mStackOrder.begin(), mStackOrder.end(), compName) == mStackOrder.end()) {
                mStackOrder.push_back(compName);
                mDirty = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

void CompositorStackPanel::applyToViewport(Ogre::Viewport* viewport) {
    if (!viewport || !mAnimator) return;

    // Sync stack with DAG (essential when panel is not visible)
    syncStackOrder();

    // Only re-apply when dirty or first application
    bool needsApply = mDirty || mAppliedCompositors.empty();

    // Diagnostic log (once)
    static bool sLoggedOnce = false;
    if (!sLoggedOnce && !mStackOrder.empty()) {
        std::cout << "[CompositorStack::applyToViewport] stackOrder=" << mStackOrder.size()
                  << " applied=" << mAppliedCompositors.size()
                  << " dirty=" << mDirty << " needsApply=" << needsApply << std::endl;
        for (auto& n : mStackOrder) {
            auto* node = mAnimator->getRegisteredNode(n);
            std::string compName = "?";
            if (node && node->getParamSpec()) {
                auto* p = node->getParamSpec()->getParam("compositor");
                if (p) compName = p->stringVal;
            }
            std::cout << "  - " << n << " → compositor='" << compName << "'" << std::endl;
        }
        sLoggedOnce = true;
    }

    // Also check if stack is empty and compositors were applied → need to remove
    if (mStackOrder.empty() && !mAppliedCompositors.empty()) {
        removeFromViewport(viewport);
        return;
    }

    if (!needsApply) return;
    if (mDirty) mDirty = false;

    // Remove previously applied compositors
    for (auto it = mAppliedCompositors.rbegin(); it != mAppliedCompositors.rend(); ++it) {
        try {
            Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, *it, false);
            Ogre::CompositorManager::getSingleton().removeCompositor(viewport, *it);
        } catch (...) {}
    }
    mAppliedCompositors.clear();

    // Apply compositors from stack in order
    for (auto& nodeName : mStackOrder) {
        auto* node = mAnimator->getRegisteredNode(nodeName);
        if (!node || !node->getParamSpec()) continue;

        auto* enabledParam = node->getParamSpec()->getParam("enabled");
        if (enabledParam && !enabledParam->boolVal) continue;

        auto* compParam = node->getParamSpec()->getParam("compositor");
        if (!compParam || compParam->stringVal.empty()) continue;

        try {
            auto* inst = Ogre::CompositorManager::getSingleton().addCompositor(viewport, compParam->stringVal);
            if (inst) {
                Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, compParam->stringVal, true);
                mAppliedCompositors.push_back(compParam->stringVal);
                std::cout << "[CompositorStack] Applied '" << compParam->stringVal << "'" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[CompositorStack] Failed '" << compParam->stringVal << "': " << e.what() << std::endl;
        }
    }
}

void CompositorStackPanel::removeFromViewport(Ogre::Viewport* viewport) {
    if (!viewport) return;
    for (auto it = mAppliedCompositors.rbegin(); it != mAppliedCompositors.rend(); ++it) {
        try {
            Ogre::CompositorManager::getSingleton().setCompositorEnabled(viewport, *it, false);
            Ogre::CompositorManager::getSingleton().removeCompositor(viewport, *it);
        } catch (...) {}
    }
    mAppliedCompositors.clear();
}

} // namespace bbfx

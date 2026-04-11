#include "OscBrowserPanel.h"
#include <imgui.h>
#include <algorithm>
#include <functional>
#include <map>
#include <sstream>

namespace bbfx {

void OscBrowserPanel::addAddress(const std::string& address) {
    if (std::find(mAddresses.begin(), mAddresses.end(), address) == mAddresses.end())
        mAddresses.push_back(address);
}

void OscBrowserPanel::render() {
    ImGui::Begin("OSC Browser");

    ImGui::Text("Discovered addresses: %d", static_cast<int>(mAddresses.size()));
    if (ImGui::SmallButton("Clear")) mAddresses.clear();

    ImGui::Separator();

    if (mAddresses.empty()) {
        ImGui::TextDisabled("No OSC messages received yet");
        ImGui::End();
        return;
    }

    // Build tree from addresses
    // Each address like "/foo/bar/baz" becomes a tree node
    struct TreeNode {
        std::map<std::string, TreeNode> children;
        std::string fullPath;
    };
    TreeNode root;
    for (auto& addr : mAddresses) {
        std::istringstream ss(addr);
        std::string segment;
        TreeNode* current = &root;
        std::string path;
        while (std::getline(ss, segment, '/')) {
            if (segment.empty()) continue;
            path += "/" + segment;
            current = &current->children[segment];
            current->fullPath = path;
        }
    }

    // Render tree recursively
    std::function<void(const std::string&, const TreeNode&)> renderTree;
    renderTree = [&](const std::string& name, const TreeNode& node) {
        if (node.children.empty()) {
            // Leaf node
            if (ImGui::Selectable(node.fullPath.c_str())) {
                ImGui::SetClipboardText(node.fullPath.c_str());
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy: %s", node.fullPath.c_str());
        } else {
            bool open = ImGui::TreeNode(name.c_str());
            if (open) {
                for (auto& [childName, childNode] : node.children) {
                    renderTree(childName, childNode);
                }
                ImGui::TreePop();
            }
        }
    };

    for (auto& [childName, childNode] : root.children) {
        renderTree(childName, childNode);
    }

    ImGui::End();
}

} // namespace bbfx

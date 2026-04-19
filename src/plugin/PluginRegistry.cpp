#include "plugin/PluginRegistry.h"

#include <algorithm>

namespace bbfx {

const std::vector<std::string> PluginRegistry::kEmpty;

PluginRegistry& PluginRegistry::instance() {
    static PluginRegistry inst;
    return inst;
}

void PluginRegistry::trackNodeType(const std::string& pluginId, const std::string& nodeTypeId) {
    auto& entry = mByPlugin[pluginId];
    if (std::find(entry.nodeTypes.begin(), entry.nodeTypes.end(), nodeTypeId) == entry.nodeTypes.end()) {
        entry.nodeTypes.push_back(nodeTypeId);
    }
    mNodeTypeOwner[nodeTypeId] = pluginId;
}

void PluginRegistry::trackPreset(const std::string& pluginId, const std::string& presetId) {
    auto& entry = mByPlugin[pluginId];
    if (std::find(entry.presets.begin(), entry.presets.end(), presetId) == entry.presets.end()) {
        entry.presets.push_back(presetId);
    }
    mPresetOwner[presetId] = pluginId;
}

void PluginRegistry::trackInspectorWidget(const std::string& pluginId, const std::string& portType) {
    auto& entry = mByPlugin[pluginId];
    if (std::find(entry.inspectorWidgets.begin(), entry.inspectorWidgets.end(), portType) == entry.inspectorWidgets.end()) {
        entry.inspectorWidgets.push_back(portType);
    }
}

void PluginRegistry::trackPanel(const std::string& pluginId, const std::string& title) {
    auto& entry = mByPlugin[pluginId];
    if (std::find(entry.panels.begin(), entry.panels.end(), title) == entry.panels.end()) {
        entry.panels.push_back(title);
    }
}

const std::vector<std::string>& PluginRegistry::nodeTypesOf(const std::string& pluginId) const {
    auto it = mByPlugin.find(pluginId);
    return it == mByPlugin.end() ? kEmpty : it->second.nodeTypes;
}

const std::vector<std::string>& PluginRegistry::presetsOf(const std::string& pluginId) const {
    auto it = mByPlugin.find(pluginId);
    return it == mByPlugin.end() ? kEmpty : it->second.presets;
}

const std::vector<std::string>& PluginRegistry::inspectorWidgetsOf(const std::string& pluginId) const {
    auto it = mByPlugin.find(pluginId);
    return it == mByPlugin.end() ? kEmpty : it->second.inspectorWidgets;
}

const std::vector<std::string>& PluginRegistry::panelsOf(const std::string& pluginId) const {
    auto it = mByPlugin.find(pluginId);
    return it == mByPlugin.end() ? kEmpty : it->second.panels;
}

std::string PluginRegistry::ownerOfNodeType(const std::string& nodeTypeId) const {
    auto it = mNodeTypeOwner.find(nodeTypeId);
    return it == mNodeTypeOwner.end() ? std::string() : it->second;
}

std::string PluginRegistry::ownerOfPreset(const std::string& presetId) const {
    auto it = mPresetOwner.find(presetId);
    return it == mPresetOwner.end() ? std::string() : it->second;
}

void PluginRegistry::clearPlugin(const std::string& pluginId) {
    auto it = mByPlugin.find(pluginId);
    if (it == mByPlugin.end()) return;

    for (const auto& t : it->second.nodeTypes) mNodeTypeOwner.erase(t);
    for (const auto& p : it->second.presets)   mPresetOwner.erase(p);
    mByPlugin.erase(it);
}

} // namespace bbfx

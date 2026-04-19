#pragma once

#include <map>
#include <string>
#include <vector>

namespace bbfx {

// Global registry tracking which plugin owns which node type / preset /
// inspector widget. Distinct from the PluginManager (which tracks the plugins
// themselves) — the registry is the source of truth used when unloading a
// plugin to tear down all the things it contributed.
class PluginRegistry {
public:
    static PluginRegistry& instance();

    // Record that plugin `pluginId` registered a node type `nodeTypeId`.
    void trackNodeType(const std::string& pluginId, const std::string& nodeTypeId);
    // Record that plugin `pluginId` registered a preset `presetId`.
    void trackPreset(const std::string& pluginId, const std::string& presetId);
    // Record that plugin `pluginId` registered an inspector widget for
    // `portType`.
    void trackInspectorWidget(const std::string& pluginId, const std::string& portType);
    // Record that plugin `pluginId` registered a custom UI panel `title`.
    void trackPanel(const std::string& pluginId, const std::string& title);

    const std::vector<std::string>& nodeTypesOf(const std::string& pluginId) const;
    const std::vector<std::string>& presetsOf(const std::string& pluginId) const;
    const std::vector<std::string>& inspectorWidgetsOf(const std::string& pluginId) const;
    const std::vector<std::string>& panelsOf(const std::string& pluginId) const;

    // Returns the plugin id that owns `nodeTypeId`, or empty if builtin.
    std::string ownerOfNodeType(const std::string& nodeTypeId) const;
    std::string ownerOfPreset(const std::string& presetId) const;

    // Remove all tracking for a plugin. Caller is responsible for actually
    // unregistering the contributions from NodeTypeRegistry/PresetSystem.
    void clearPlugin(const std::string& pluginId);

private:
    PluginRegistry() = default;

    struct PerPlugin {
        std::vector<std::string> nodeTypes;
        std::vector<std::string> presets;
        std::vector<std::string> inspectorWidgets;
        std::vector<std::string> panels;
    };

    std::map<std::string, PerPlugin> mByPlugin;
    std::map<std::string, std::string> mNodeTypeOwner;
    std::map<std::string, std::string> mPresetOwner;

    static const std::vector<std::string> kEmpty;
};

} // namespace bbfx

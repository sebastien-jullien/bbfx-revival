#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace bbfx {

struct ParamDef;       // forward (src/core/ParamSpec.h)

/// Registry of custom InspectorPanel widgets contributed by plugins.
///
/// Populated by `bbfx.ui.registerInspectorWidget(portType, callback)` in
/// Lot P, but wired up here (Lot C) so that `InspectorPanel::renderParamSpec`
/// calls `tryDraw` before falling back to the built-in widget for every port.
///
/// Keying: a widget can be registered against a specific `<nodeType>.<portName>`
/// pair (narrow) or just a `<portName>` (wildcard across node types). The
/// wildcard lookup happens as a second-chance when no exact match is found.
class InspectorWidgetRegistry {
public:
    // Returns `true` if the callback drew the widget for this port; in that
    // case the InspectorPanel must skip its default rendering for that param.
    using Callback = std::function<bool(const std::string& nodeName,
                                        const std::string& portName,
                                        struct ParamDef&)>;

    static InspectorWidgetRegistry& instance();

    void registerForPort(const std::string& pluginId,
                         const std::string& nodeType,
                         const std::string& portName,
                         Callback cb);

    // Wildcard registration (all node types that have a port named `portName`).
    void registerForPortWildcard(const std::string& pluginId,
                                 const std::string& portName,
                                 Callback cb);

    void unregisterByPlugin(const std::string& pluginId);

    // Returns true if a registered widget drew the UI for this port.
    bool tryDraw(const std::string& nodeType,
                 const std::string& nodeName,
                 const std::string& portName,
                 struct ParamDef& param);

private:
    InspectorWidgetRegistry() = default;

    struct Entry {
        std::string pluginId;
        Callback cb;
    };
    // "<nodeType>.<portName>" -> Entry
    std::map<std::string, Entry> mExact;
    // "<portName>" -> Entry
    std::map<std::string, Entry> mWildcard;
};

} // namespace bbfx

#include "plugin/InspectorWidgetRegistry.h"

#include "core/ParamSpec.h"

namespace bbfx {

InspectorWidgetRegistry& InspectorWidgetRegistry::instance() {
    static InspectorWidgetRegistry inst;
    return inst;
}

void InspectorWidgetRegistry::registerForPort(const std::string& pluginId,
                                               const std::string& nodeType,
                                               const std::string& portName,
                                               Callback cb) {
    mExact[nodeType + "." + portName] = { pluginId, std::move(cb) };
}

void InspectorWidgetRegistry::registerForPortWildcard(const std::string& pluginId,
                                                      const std::string& portName,
                                                      Callback cb) {
    mWildcard[portName] = { pluginId, std::move(cb) };
}

void InspectorWidgetRegistry::unregisterByPlugin(const std::string& pluginId) {
    for (auto it = mExact.begin(); it != mExact.end();) {
        if (it->second.pluginId == pluginId) it = mExact.erase(it);
        else ++it;
    }
    for (auto it = mWildcard.begin(); it != mWildcard.end();) {
        if (it->second.pluginId == pluginId) it = mWildcard.erase(it);
        else ++it;
    }
}

bool InspectorWidgetRegistry::tryDraw(const std::string& nodeType,
                                      const std::string& nodeName,
                                      const std::string& portName,
                                      ParamDef& param) {
    auto exactKey = nodeType + "." + portName;
    auto itExact = mExact.find(exactKey);
    if (itExact != mExact.end() && itExact->second.cb) {
        return itExact->second.cb(nodeName, portName, param);
    }
    auto itWild = mWildcard.find(portName);
    if (itWild != mWildcard.end() && itWild->second.cb) {
        return itWild->second.cb(nodeName, portName, param);
    }
    return false;
}

} // namespace bbfx

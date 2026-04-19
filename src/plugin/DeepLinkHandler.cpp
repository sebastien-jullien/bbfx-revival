#include "plugin/DeepLinkHandler.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace bbfx {

DeepLinkHandler& DeepLinkHandler::instance() {
    static DeepLinkHandler inst;
    return inst;
}

namespace {
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string part;
    while (std::getline(iss, part, delim)) if (!part.empty()) out.push_back(part);
    return out;
}
} // anonymous

DeepLinkHandler::Action DeepLinkHandler::parse(const std::string& url) {
    Action a;
    a.rawUrl = url;

    const std::string prefix = "bbfx://";
    if (url.rfind(prefix, 0) != 0) return a;

    std::string rest = url.substr(prefix.size());
    // Split into at most 3 segments: action / pluginId / optional-extra.
    auto parts = split(rest, '/');
    if (parts.empty()) return a;

    const std::string& kind = parts[0];
    if (kind == "install" && parts.size() >= 2) {
        a.kind = Action::Kind::Install;
        a.pluginId = parts[1];
    } else if (kind == "enable" && parts.size() >= 2) {
        a.kind = Action::Kind::Enable;
        a.pluginId = parts[1];
    } else if (kind == "disable" && parts.size() >= 2) {
        a.kind = Action::Kind::Disable;
        a.pluginId = parts[1];
    } else if (kind == "run" && parts.size() >= 3) {
        a.kind = Action::Kind::Run;
        a.pluginId = parts[1];
        a.extra    = parts[2];
    }
    return a;
}

void DeepLinkHandler::handle(const Action& a) {
    switch (a.kind) {
        case Action::Kind::Install:
            if (onInstall) onInstall(a.pluginId);
            else std::cerr << "[DeepLink] no handler for install: " << a.rawUrl << std::endl;
            return;
        case Action::Kind::Enable:
            if (onEnable)  onEnable(a.pluginId);
            else std::cerr << "[DeepLink] no handler for enable: "  << a.rawUrl << std::endl;
            return;
        case Action::Kind::Disable:
            if (onDisable) onDisable(a.pluginId);
            else std::cerr << "[DeepLink] no handler for disable: " << a.rawUrl << std::endl;
            return;
        case Action::Kind::Run:
            if (onRun) onRun(a.pluginId, a.extra);
            else std::cerr << "[DeepLink] no handler for run: " << a.rawUrl << std::endl;
            return;
        case Action::Kind::Unknown:
        default:
            std::cerr << "[DeepLink] unparsed URL: " << a.rawUrl << std::endl;
            return;
    }
}

} // namespace bbfx

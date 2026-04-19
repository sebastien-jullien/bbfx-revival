#pragma once

#include <functional>
#include <string>

namespace bbfx {

/// v3.5 Lot I — parses and dispatches `bbfx://` deep links.
///
/// Supported URL schemes:
///   - bbfx://install/<plugin-id>       — opens Community Browser on the
///                                         entry, or installs directly if
///                                         the id is known in the index
///   - bbfx://enable/<plugin-id>        — enable a plugin already installed
///                                         on the machine
///   - bbfx://disable/<plugin-id>       — symmetric to enable
///   - bbfx://run/<plugin-id>/<type>    — enable plugin + instantiate a
///                                         node of `<pluginId>.<type>` in
///                                         the current graph (Studio only)
///
/// The handler is UI-framework-agnostic — it exposes callbacks that the
/// Studio wires to its panel flags. bbfx.exe (headless) uses the same
/// parser through a minimal CLI flag (`--deep-link <url>`).
class DeepLinkHandler {
public:
    static DeepLinkHandler& instance();

    struct Action {
        enum class Kind { Unknown, Install, Enable, Disable, Run };
        Kind        kind = Kind::Unknown;
        std::string pluginId;
        std::string extra;     // node type for Run; empty otherwise
        std::string rawUrl;
    };

    // Parse a bbfx://... URL into an Action. Returns an Action with
    // kind=Unknown if the scheme or path is malformed.
    static Action parse(const std::string& url);

    // Handle a parsed action. If callbacks are not set, the action is
    // logged and dropped with a Toast warning (Studio) or stderr log
    // (headless).
    void handle(const Action& a);

    // Wire-up hooks the Studio populates in StudioApp constructor.
    std::function<void(const std::string& id)> onInstall;
    std::function<void(const std::string& id)> onEnable;
    std::function<void(const std::string& id)> onDisable;
    std::function<void(const std::string& id, const std::string& nodeType)> onRun;

private:
    DeepLinkHandler() = default;
};

} // namespace bbfx

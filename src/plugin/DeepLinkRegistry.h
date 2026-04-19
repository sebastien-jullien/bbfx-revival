#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot I — one-shot registration of the `bbfx://` URL scheme in the
/// operating system, so clicking `bbfx://install/<id>` in a browser
/// launches the Studio with the URL as argv[1].
///
/// Windows : HKCU\Software\Classes\bbfx
/// Linux   : ~/.local/share/applications/bbfx.desktop + xdg-mime default
/// macOS   : (not supported in v3.5)
///
/// The registration is idempotent — calling it every startup is cheap and
/// re-points the scheme to the current executable (useful after a move).
class DeepLinkRegistry {
public:
    /// Register the `bbfx://` scheme pointing at the given executable.
    /// Returns true on success. Silently no-ops (returns false) on
    /// platforms we don't support.
    static bool registerScheme(const std::string& exePath);

    /// Returns true if the current user has the scheme registered and
    /// pointing at some executable (not necessarily this one).
    static bool isSchemeRegistered();
};

} // namespace bbfx

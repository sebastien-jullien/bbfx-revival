#pragma once

#include <sol/sol.hpp>

namespace bbfx {

/// v3.5 Lot P — register `bbfx.ui.*` ImGui widget bindings.
///
/// Called by StudioApp once the Studio ImGui context is ready. In
/// headless builds bbfx.ui stays absent (bindings live in bbfx-studio
/// only).
///
/// The bindings themselves are permission-free at the raw-state level;
/// PluginSandboxApi gates the namespace per plugin using the `ui`
/// permission (same mechanism as midi / osc / fs / http).
void registerImguiBindings(sol::state& lua);

} // namespace bbfx

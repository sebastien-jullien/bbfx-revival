#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  BBFx — source of truth for the application version string.
//
//  All version-bearing surfaces (window title, .bbfx-project schema stamp,
//  MIDI mapping profile schema, network handshake) read from here. Bump this
//  single header when releasing a new version — CMakeLists.txt also pulls the
//  same major.minor.patch via project(BBFx VERSION X.Y.Z) so CPack stays
//  in sync.
//
//  v3.5.2 Sprint S8 Lot AU (2026-05-11) — introduced to retire the dispersed
//  hardcoded strings ("3.4" in ProjectSerializer, "3.4" in MappingProfile,
//  "3.4" in SyncManager, "3.5.0" in CMake project(), "3.3.0" in CPack vars,
//  "3.5.2" hardcoded in StudioApp window title).
// ─────────────────────────────────────────────────────────────────────────────

#define BBFX_VERSION_MAJOR 3
#define BBFX_VERSION_MINOR 5
#define BBFX_VERSION_PATCH 2

#define BBFX_VERSION_STRING "3.5.2"
#define BBFX_VERSION_NAME   "VJ Reference Edition"

namespace bbfx {

constexpr const char* kVersionString = BBFX_VERSION_STRING;
constexpr const char* kVersionName   = BBFX_VERSION_NAME;
constexpr int         kVersionMajor  = BBFX_VERSION_MAJOR;
constexpr int         kVersionMinor  = BBFX_VERSION_MINOR;
constexpr int         kVersionPatch  = BBFX_VERSION_PATCH;

} // namespace bbfx

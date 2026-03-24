cmake_minimum_required(VERSION 3.20)
project(BBFx VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dépendances via vcpkg ──────────────────────────────────────────────────────
find_package(OGRE  REQUIRED COMPONENTS Main Overlay)
find_package(SDL3  REQUIRED CONFIG)
find_package(sol2  REQUIRED CONFIG)
find_package(Lua   REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph)

# ── Exécutable principal ───────────────────────────────────────────────────────
add_executable(bbfx src/main.cpp)

target_link_libraries(bbfx PRIVATE
    OgreMain
    OgreOverlay
    SDL3::SDL3
    sol2::sol2
    ${LUA_LIBRARIES}
    Boost::graph
)

target_include_directories(bbfx PRIVATE
    ${LUA_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Informations de build ──────────────────────────────────────────────────────
message(STATUS "BBFx v${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}")
message(STATUS "OGRE   : ${OGRE_VERSION}")
message(STATUS "Lua    : ${LUA_VERSION_STRING}")

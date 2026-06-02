#pragma once
// BBFx — logging debug hot-path centralisé.
//
// Les nodes/inputs temps-réel (Gamepad, JoystickRouter, TextureBlend/Set,
// ShaderFx, TheoraClip…) émettaient des traces `std::cout` permanentes dans
// leur boucle update() — utiles à la mise au point mais polluant stdout en
// production et coûtant un flush par frame. Ce helper les route via un flag
// runtime lu UNE seule fois (env var `BBFX_DEBUG_LOG`), OFF par défaut.
//
// Activation :  set BBFX_DEBUG_LOG=1   (Windows)  /  export BBFX_DEBUG_LOG=1
// Usage      :  BBFX_DLOG("[GP-EDGE] btn " << idx << " -> " << val);
//
// Quand le flag est OFF, l'expression n'est même pas évaluée (court-circuit),
// donc aucun coût de formatage en production.
#include <cstdlib>
#include <iostream>

namespace bbfx {

inline bool debugLogEnabled() {
    static const bool on = [] {
        const char* e = std::getenv("BBFX_DEBUG_LOG");
        return e && e[0] != '\0' && e[0] != '0';
    }();
    return on;
}

} // namespace bbfx

#define BBFX_DLOG(expr)                                            \
    do {                                                          \
        if (::bbfx::debugLogEnabled()) {                          \
            std::cout << expr << std::endl;                       \
        }                                                         \
    } while (0)

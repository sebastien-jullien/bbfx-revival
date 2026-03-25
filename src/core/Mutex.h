#pragma once

#include <mutex>

namespace bbfx {

using Mutex = std::recursive_mutex;
using Lock = std::lock_guard<Mutex>;

static_assert(sizeof(Mutex) > 0, "Mutex type must be valid");

} // namespace bbfx

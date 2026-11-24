#pragma once

#include "../engine/image.h"
#include "../vk/mem.hpp"
#include "../engine/state.hpp"
#include <filesystem>

namespace racecar::geometry {

/// Generates a 3D textured checkered block.
engine::Image generate_test_3D();
engine::Image generate_glint_noise();

}
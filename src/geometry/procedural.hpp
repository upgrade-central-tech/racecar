#pragma once

#include "../engine/rwimage.hpp"
#include "../vk/mem.hpp"
#include "../engine/state.hpp"
#include <filesystem>

namespace racecar::geometry {

/// Generates a 3D textured checkered block.
engine::RWImage generate_test_3D();
engine::RWImage generate_glint_noise();

}
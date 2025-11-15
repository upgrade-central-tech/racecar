#pragma once

#include "../vk/mem.hpp"
#include "../engine/state.hpp"

namespace racecar::geometry {

/// Generates a 3D textured checkered block.
vk::mem::AllocatedImage generate_test_3D( vk::Common& vulkan, engine::State& engine );

}
#pragma once

#include "../vk/mem.hpp"
#include "../engine/state.hpp"
#include <filesystem>

namespace racecar::geometry {

/// Generates a 3D textured checkered block.
vk::mem::AllocatedImage generate_test_3D( vk::Common& vulkan, engine::State& engine );
vk::mem::AllocatedImage generate_glint_noise( vk::Common& vulkan, engine::State& engine );

vk::mem::AllocatedImage load_image( std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine, int chnanels, VkFormat image_format );

vk::mem::AllocatedImage create_cubemap ( std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine );
vk::mem::AllocatedImage create_prefiltered_cubemap ( vk::Common& vulkan, engine::State& engine, int roughness_levels );

}
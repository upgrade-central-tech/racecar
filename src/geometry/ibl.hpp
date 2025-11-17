#pragma once

#include "../engine/state.hpp"
#include "../vk/mem.hpp"

#include <filesystem>

namespace racecar::geometry {

std::vector<glm::vec3> generate_diffuse_sh( std::filesystem::path file_path );

vk::mem::AllocatedImage generate_diffuse_irradiance(
    std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine );

vk::mem::AllocatedImage create_cubemap(
    std::filesystem::path file_path, vk::Common& vulkan, engine::State& engine );

template <typename T>
void load_cubemap( vk::Common& vulkan, engine::State& engine,
    std::vector<std::vector<T>>& face_data, vk::mem::AllocatedImage& cm_image, VkExtent3D extent,
    VkFormat format );

vk::mem::AllocatedImage allocate_cube_map( vk::Common& vulkan, VkExtent3D extent, VkFormat format,
    uint32_t mip_levels, bool readOnly = true );

}

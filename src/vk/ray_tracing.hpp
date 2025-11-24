#pragma once

#include <volk.h>
namespace racecar::vk::rt { 

struct RayTracingProperties {
    int shader_group_base_alignment = 0;
    int min_acceleration_structure_scratch_offset_alignment = 0;
    int shader_group_handle_size = 0;
    int shader_group_handle_alignment = 0;
};

RayTracingProperties query_rt_properties(VkPhysicalDevice physicalDevice);

}

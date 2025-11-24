#include "ray_tracing.hpp"

namespace racecar::vk::rt {

RayTracingProperties query_rt_properties( VkPhysicalDevice physicalDevice )
{
    VkPhysicalDeviceAccelerationStructurePropertiesKHR as_properties
        = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
              .pNext = nullptr };

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties
        = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
              .pNext = &as_properties };

    VkPhysicalDeviceProperties2 device_properties_2
        = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
              .pNext = &rt_pipeline_properties };

    vkGetPhysicalDeviceProperties2( physicalDevice, &device_properties_2 );

    RayTracingProperties result = {};

    result.shader_group_handle_size = int( rt_pipeline_properties.shaderGroupHandleSize );
    result.shader_group_handle_alignment = int( rt_pipeline_properties.shaderGroupHandleAlignment );
    result.shader_group_base_alignment = int( rt_pipeline_properties.shaderGroupBaseAlignment );

    result.min_acceleration_structure_scratch_offset_alignment
        = int( as_properties.minAccelerationStructureScratchOffsetAlignment );

    return result;
}

}

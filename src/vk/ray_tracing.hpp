#pragma once

#include "vma.hpp"
#include "../engine/destructor_stack.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <volk.h>


namespace racecar::vk::rt {

struct RayTracingProperties {
    uint64_t shader_group_base_alignment = 0;
    uint64_t min_acceleration_structure_scratch_offset_alignment = 0;
    uint64_t shader_group_handle_size = 0;
    uint64_t shader_group_handle_alignment = 0;
};

RayTracingProperties query_rt_properties( VkPhysicalDevice physical_device );

inline uint64_t align_up( uint64_t value, uint64_t alignment )
{
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

struct AccelerationStructure {
    enum class Type { TLAS, BLAS } type;

    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceAddress device_address = 0;
};

struct MeshData {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
    VkDeviceAddress vertex_buffer_address; // Retrieved via vkGetBufferDeviceAddress
    VkDeviceAddress index_buffer_address; // Retrieved via vkGetBufferDeviceAddress
};

VkAccelerationStructureGeometryKHR create_acceleration_structure_from_geometry(
    const MeshData& mesh );

AccelerationStructure build_blas( VkDevice device, VmaAllocator allocator,
    RayTracingProperties& rt_props, MeshData mesh, VkCommandBuffer cmd_buf, DestructorStack& destructor_stack );

struct Object {
    AccelerationStructure* blas;
    glm::mat4 transform;
};

AccelerationStructure build_tlas(
    VkDevice device, VmaAllocator allocator,
    const RayTracingProperties& rt_props, const std::vector<Object>& objects, 
    VkCommandBuffer cmd_buf, DestructorStack& destructor_stack );

}

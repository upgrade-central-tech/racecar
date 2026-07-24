#pragma once

#include "../engine/destructor_stack.hpp"
#include "vma.hpp"

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

    // Build descriptors filled in by alloc_blas/alloc_tlas and consumed by build_blas/build_tlas
    VkAccelerationStructureGeometryKHR geometry {};
    VkAccelerationStructureBuildGeometryInfoKHR build_info {};
    VkAccelerationStructureBuildRangeInfoKHR range_info {};
};

struct MeshData {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    uint32_t max_vertex;
    uint32_t index_count;
    uint32_t vertex_offset;
    uint32_t index_offset;
    VkDeviceAddress vertex_buffer_address; // Retrieved via vkGetBufferDeviceAddress
    VkDeviceAddress index_buffer_address; // Retrieved via vkGetBufferDeviceAddress
    uint32_t vertex_stride;
};

VkAccelerationStructureGeometryKHR
create_acceleration_structure_from_geometry( const MeshData& mesh );

void alloc_blas(
    VkDevice device,
    VmaAllocator allocator,
    AccelerationStructure& blas,
    RayTracingProperties& rt_props,
    MeshData mesh,
    DestructorStack& destructor_stack
);

void build_blas(
    VkDevice device,
    VmaAllocator allocator,
    AccelerationStructure& blas,
    RayTracingProperties& rt_props,
    MeshData mesh,
    VkCommandBuffer cmd_buf,
    DestructorStack& destructor_stack
);

struct Object {
    AccelerationStructure* blas;
    glm::mat4 transform;
};

void alloc_tlas(
    VkDevice device,
    VmaAllocator allocator,
    AccelerationStructure& tlas,
    RayTracingProperties& rt_props,
    const std::vector<Object>& objects,
    DestructorStack& destructor_stack
);

void build_tlas(
    VkDevice device,
    VmaAllocator allocator,
    AccelerationStructure& tlas,
    RayTracingProperties& rt_props,
    const std::vector<Object>& objects,
    VkCommandBuffer cmd_buf,
    DestructorStack& destructor_stack
);

}

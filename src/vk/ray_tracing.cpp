#include "ray_tracing.hpp"

namespace racecar::vk::rt {

RayTracingProperties query_rt_properties( VkPhysicalDevice physical_device )
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

    vkGetPhysicalDeviceProperties2( physical_device, &device_properties_2 );

    RayTracingProperties result = {};

    result.shader_group_handle_size = int( rt_pipeline_properties.shaderGroupHandleSize );
    result.shader_group_handle_alignment = int( rt_pipeline_properties.shaderGroupHandleAlignment );
    result.shader_group_base_alignment = int( rt_pipeline_properties.shaderGroupBaseAlignment );

    result.min_acceleration_structure_scratch_offset_alignment
        = int( as_properties.minAccelerationStructureScratchOffsetAlignment );

    return result;
}

VkAccelerationStructureGeometryKHR create_acceleration_structure_from_geometry(
    const MeshData& mesh )
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = mesh.vertex_buffer_address },
        .vertexStride = sizeof( float ) * 3,
        .maxVertex = mesh.vertex_count,

        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = mesh.index_buffer_address },
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    return geometry;
}

AccelerationStructure build_blas( VkDevice device, VmaAllocator allocator,
    const RayTracingProperties& rt_props, MeshData& mesh, VkCommandBuffer cmd_buf )
{
    VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = mesh.vertex_buffer;
    mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &info);

    info.buffer = mesh.index_buffer;
    mesh.index_buffer_address = vkGetBufferDeviceAddress(device, &info);

    VkAccelerationStructureGeometryKHR geometry
        = create_acceleration_structure_from_geometry( mesh );

    uint32_t max_primitive_count = mesh.index_count / 3;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = { .deviceAddress = 0 },
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo
        = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = { .primitiveCount = max_primitive_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0 };

    vkGetAccelerationStructureBuildSizesKHR( device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &max_primitive_count,
        &sizeInfo );

    AccelerationStructure blas = {};

    VkBufferCreateInfo blas_buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeInfo.accelerationStructureSize,
        .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT // Required for AS
    };
    VmaAllocationCreateInfo blas_alloc_create_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateBuffer( allocator, &blas_buffer_create_info, &blas_alloc_create_info, &blas.buffer,
        &blas.allocation, nullptr );

    VkBufferDeviceAddressInfo blas_address_info
        = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = blas.buffer };
    blas.device_address = vkGetBufferDeviceAddress( device, &blas_address_info );

    uint64_t aligned_scratch_size = sizeInfo.buildScratchSize
        + static_cast<VkDeviceSize>( rt_props.min_acceleration_structure_scratch_offset_alignment );

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VmaAllocation scratchAllocation = VK_NULL_HANDLE;

    VkBufferCreateInfo scratchBufferCI = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = aligned_scratch_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };
    VmaAllocationCreateInfo scratchAllocCI = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateBuffer(
        allocator, &scratchBufferCI, &scratchAllocCI, &scratchBuffer, &scratchAllocation, nullptr );

    VkBufferDeviceAddressInfo scratchAddressInfo
        = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer };
    VkDeviceAddress rawScratchAddress = vkGetBufferDeviceAddress( device, &scratchAddressInfo );

    VkDeviceAddress alignedScratchAddress = align_up( rawScratchAddress,
        static_cast<uint64_t>( rt_props.min_acceleration_structure_scratch_offset_alignment ) );

    VkAccelerationStructureCreateInfoKHR asCI
        = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
              .buffer = blas.buffer,
              .size = sizeInfo.accelerationStructureSize,
              .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
              .deviceAddress = blas.device_address };
    vkCreateAccelerationStructureKHR( device, &asCI, nullptr, &blas.handle );

    buildInfo.dstAccelerationStructure = blas.handle;
    buildInfo.scratchData.deviceAddress = alignedScratchAddress; // The aligned address

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    vkCmdBuildAccelerationStructuresKHR( cmd_buf, 1, &buildInfo, &pRangeInfo );

    // vmaDestroyBuffer( allocator, scratchBuffer, scratchAllocation );
    // scratchbuffer NEEDS to be destroyed

    blas.type = AccelerationStructure::Type::BLAS;

    return blas;
}

}

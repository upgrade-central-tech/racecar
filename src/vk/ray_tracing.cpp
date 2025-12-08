#include "ray_tracing.hpp"
#include "mem.hpp"
#include "../log.hpp"

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

    result.shader_group_handle_size = rt_pipeline_properties.shaderGroupHandleSize;
    result.shader_group_handle_alignment = rt_pipeline_properties.shaderGroupHandleAlignment;
    result.shader_group_base_alignment = rt_pipeline_properties.shaderGroupBaseAlignment;

    result.min_acceleration_structure_scratch_offset_alignment
        = as_properties.minAccelerationStructureScratchOffsetAlignment;

    return result;
}

VkAccelerationStructureGeometryKHR create_acceleration_structure_from_geometry(
    const MeshData& mesh )
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = mesh.vertex_buffer_address + mesh.vertex_offset * mesh.vertex_stride },
        .vertexStride = mesh.vertex_stride,
        .maxVertex = mesh.max_vertex,

        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = mesh.index_buffer_address + mesh.index_offset * sizeof(uint32_t) },
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
    RayTracingProperties& rt_props, MeshData mesh, VkCommandBuffer cmd_buf, [[maybe_unused]] DestructorStack& destructor_stack )
{
    log::info("Building BLAS with vertex stride {}", mesh.vertex_stride);

    if ( mesh.vertex_buffer == VK_NULL_HANDLE ) {
        throw Exception("[Build BLAS] MeshData vertex buffer is null");
    }
    if ( mesh.index_buffer == VK_NULL_HANDLE ) {
        throw Exception("[Build BLAS] MeshData index buffer is null");
    }

    VkBufferDeviceAddressInfo vertex_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh.vertex_buffer
    };
    mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &vertex_info);

    VkBufferDeviceAddressInfo index_info {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh.index_buffer
    };
    mesh.index_buffer_address = vkGetBufferDeviceAddress(device, &index_info);

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
    destructor_stack.push_free_vmabuffer(allocator, {
        .handle = blas.buffer,
        .allocation = blas.allocation,
    });
 
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
    destructor_stack.push_free_vmabuffer(allocator, {
        .handle = scratchBuffer,
        .allocation = scratchAllocation,
    });


    VkBufferDeviceAddressInfo scratchAddressInfo
        = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer };
    VkDeviceAddress rawScratchAddress = vkGetBufferDeviceAddress( device, &scratchAddressInfo );

    VkDeviceAddress alignedScratchAddress = align_up( rawScratchAddress,
        static_cast<uint64_t>( rt_props.min_acceleration_structure_scratch_offset_alignment ) );

    VkAccelerationStructureCreateInfoKHR asCI
        = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
              .createFlags = VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR,
              .buffer = blas.buffer,
              .size = sizeInfo.accelerationStructureSize,
              .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
              .deviceAddress = blas.device_address,
        };
    vk::check( vkCreateAccelerationStructureKHR( device, &asCI, nullptr, &blas.handle ), "Failed to call vkCreateAccelerationStructureKHR for BLAS");
    destructor_stack.push(device, blas.handle, vkDestroyAccelerationStructureKHR);

    buildInfo.dstAccelerationStructure = blas.handle;
    buildInfo.scratchData.deviceAddress = alignedScratchAddress; // The aligned address

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    vkCmdBuildAccelerationStructuresKHR( cmd_buf, 1, &buildInfo, &pRangeInfo );

    // vmaDestroyBuffer( allocator, scratchBuffer, scratchAllocation );
    // scratchbuffer NEEDS to be destroyed

    blas.type = AccelerationStructure::Type::BLAS;

    // this makes it static
    VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
    .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
    };

    // Transition between the Ray Tracing/Compute stages
    vkCmdPipelineBarrier(
        cmd_buf,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, // Source stage
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, // Destination stages (ready for TLAS build or Ray Trace)
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);

    return blas;
}

AccelerationStructure build_tlas(
    VkDevice device, VmaAllocator allocator,
    const RayTracingProperties& rt_props, const std::vector<Object>& objects, 
    VkCommandBuffer cmd_buf, DestructorStack& destructor_stack )
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(objects.size());
    
    for (size_t i = 0; i < objects.size(); ++i) {
        const Object& obj = objects[i];
        
        bool nonzero = false;

        for (int a = 0; a < 4; a++) {
            for (int b = 0; b < 4; b++) {
                if ( obj.transform[a][b] != 0.0f) {
                    nonzero = true;
                }
            }
        }


        if (!nonzero) {
            log::warn("[Build TLAS] The provided object at index {} has a zeroed transform matrix. This will cause the BLAS to completely not appear in the TLAS", i);
        }

        VkTransformMatrixKHR transform_matrix;
        memcpy(&transform_matrix.matrix, glm::value_ptr(obj.transform), sizeof(float) * 12);
        
        VkAccelerationStructureInstanceKHR instance = {};
        instance.transform = transform_matrix;
        
        instance.accelerationStructureReference = obj.blas->device_address;
        
        instance.instanceCustomIndex = static_cast<uint32_t>(i); 
        
        instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
        instance.instanceShaderBindingTableRecordOffset = 0; 
        instance.mask = 0xFF;
        
        instances.push_back(instance);
    }

    uint32_t instance_count = static_cast<uint32_t>(instances.size());

    if (instance_count == 0) {
        return {}; 
    }

    VkBuffer instance_buffer = VK_NULL_HANDLE;
    VmaAllocation instance_allocation = VK_NULL_HANDLE;

    VkBufferCreateInfo instanceBufferCI = { 
       .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
       .size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
       .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
    };
    VmaAllocationCreateInfo instanceAllocCI = { 
       .usage = VMA_MEMORY_USAGE_CPU_TO_GPU 
    };

    vmaCreateBuffer(allocator, &instanceBufferCI, &instanceAllocCI, 
                    &instance_buffer, &instance_allocation, nullptr);
    destructor_stack.push_free_vmabuffer(allocator, {
        .handle = instance_buffer,
        .allocation = instance_allocation,
    });


    void* data;
    vmaMapMemory(allocator, instance_allocation, &data);
    memcpy(data, instances.data(), instanceBufferCI.size);
    vmaUnmapMemory(allocator, instance_allocation);
    
    VkBufferDeviceAddressInfo instanceAddressInfo = {
       .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, 
       .buffer = instance_buffer 
    };
    VkDeviceAddress instance_buffer_address = vkGetBufferDeviceAddress(device, &instanceAddressInfo);


    
    VkAccelerationStructureGeometryKHR tlas_geometry = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .pNext = nullptr,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = {
          .instances = {
              .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
              .pNext = nullptr,
              .arrayOfPointers = VK_FALSE,
              .data = {.deviceAddress = instance_buffer_address }
           }
       },
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .pNext = nullptr,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, 
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &tlas_geometry,
      .scratchData = {.deviceAddress = 0 }, // Placeholder
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instance_count, // Use instance_count here, not primitive_count
        &sizeInfo);

    AccelerationStructure tlas = {};
    
    // Allocate TLAS Result Buffer
    VkBufferCreateInfo tlasBufferCI = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = sizeInfo.accelerationStructureSize,
      .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };
    VmaAllocationCreateInfo tlasAllocCI = {.usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateBuffer(allocator, &tlasBufferCI, &tlasAllocCI, 
                    &tlas.buffer, &tlas.allocation, nullptr);
    destructor_stack.push_free_vmabuffer(allocator, {
        .handle = tlas.buffer,
        .allocation = tlas.allocation,
    });

    
    VkBufferDeviceAddressInfo tlasAddressInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = tlas.buffer
    };
    tlas.device_address = vkGetBufferDeviceAddress(device, &tlasAddressInfo);

    uint64_t aligned_scratch_size = sizeInfo.buildScratchSize
        + static_cast<VkDeviceSize>( rt_props.min_acceleration_structure_scratch_offset_alignment );

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VmaAllocation scratchAllocation = VK_NULL_HANDLE;

    // 1. Define Creation Info for the Scratch Buffer
    VkBufferCreateInfo scratchBufferCI = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        // Use the padded size
       .size = aligned_scratch_size, 
        // Required usage flags for scratch memory: Storage buffer + Device Address
       .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
    };
    VmaAllocationCreateInfo scratchAllocCI = {.usage = VMA_MEMORY_USAGE_GPU_ONLY };
    
    // 2. Allocate the Scratch Buffer using VMA
    vmaCreateBuffer(
        allocator, &scratchBufferCI, &scratchAllocCI, &scratchBuffer, &scratchAllocation, nullptr );
    destructor_stack.push_free_vmabuffer(allocator, {
        .handle = scratchBuffer,
        .allocation = scratchAllocation,
    });


    // 3. Get the raw device address
    VkBufferDeviceAddressInfo scratchAddressInfo
        = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = scratchBuffer };
    VkDeviceAddress rawScratchAddress = vkGetBufferDeviceAddress( device, &scratchAddressInfo );

    // 4. Align the address upwards to meet hardware requirements (CRITICAL STEP)
    VkDeviceAddress alignedScratchAddress = align_up( rawScratchAddress,
        static_cast<uint64_t>( rt_props.min_acceleration_structure_scratch_offset_alignment ) );

    // must cleanup these buffers later
    // 6. The aligned address is now ready to be used in the buildInfo:
    
    VkAccelerationStructureCreateInfoKHR asCI = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .createFlags = VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR,
      .buffer = tlas.buffer,
      .size = sizeInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .deviceAddress = tlas.device_address
    };
    vk::check(vkCreateAccelerationStructureKHR(device, &asCI, nullptr, &tlas.handle), "Failed to call vkCreateAccelerationStructureKHR for TLAS");
    destructor_stack.push(device, tlas.handle, vkDestroyAccelerationStructureKHR);

    // 7. Update Build Info and Execute Build Command
    buildInfo.dstAccelerationStructure = tlas.handle;
    buildInfo.scratchData.deviceAddress = alignedScratchAddress; 

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = { 
       .primitiveCount = instance_count, // Primitive count is instance count for TLAS
       .primitiveOffset = 0,
       .firstVertex = 0,
       .transformOffset = 0 
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
    vkCmdBuildAccelerationStructuresKHR( cmd_buf, 1, &buildInfo, &pRangeInfo );

    // 8. Synchronization and Cleanup
    
    // Memory Barrier: Ensure TLAS build output is readable by Ray Tracing Shaders
    VkMemoryBarrier barrier = {
       .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
       .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
       .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(
        cmd_buf,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, // Source Stage (TLAS build)
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,           // Destination Stage (Ready for traceRayEXT)
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
        
    // Instance buffer and scratch buffer cleanup must be deferred until the command buffer execution completes
    // You should store instance_buffer, instance_allocation, scratchBuffer, and scratchAllocation 
    // and destroy them after synchronization.

    tlas.type = AccelerationStructure::Type::TLAS;

    return tlas;
}

}

#include "create.hpp"

#include <SDL3/SDL.h>

#include <fstream>
#include <ios>

namespace racecar::vk::create {

VkCommandPoolCreateInfo command_pool_info( uint32_t queue_family_index,
                                           VkCommandPoolCreateFlags flags ) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queue_family_index,
    };
}

VkCommandBufferAllocateInfo command_buffer_allocate_info( VkCommandPool pool, uint32_t count ) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count,
    };
}

VkFenceCreateInfo fence_info( VkFenceCreateFlags flags ) {
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };
}

VkSemaphoreCreateInfo semaphore_info() {
    return { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
}

VkCommandBufferBeginInfo command_buffer_begin_info( VkCommandBufferUsageFlags flags ) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
}

VkImageSubresourceRange image_subresource_range( VkImageAspectFlags aspect_mask ) {
    return {
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

VkSemaphoreSubmitInfo semaphore_submit_info( VkPipelineStageFlags2 stage_mask,
                                             VkSemaphore semaphore ) {
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stage_mask,
        .deviceIndex = 0,
    };
}

VkCommandBufferSubmitInfo command_buffer_submit_info( VkCommandBuffer command_buffer ) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = command_buffer,
        .deviceMask = 0,
    };
}

VkSubmitInfo2 submit_info( VkCommandBufferSubmitInfo* command_buffer_info,
                           VkSemaphoreSubmitInfo* signal_semaphore_info,
                           VkSemaphoreSubmitInfo* wait_semaphore_info ) {
    return {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = wait_semaphore_info ? 1U : 0U,
        .pWaitSemaphoreInfos = wait_semaphore_info,
        .commandBufferInfoCount = 1U,
        .pCommandBufferInfos = command_buffer_info,
        .signalSemaphoreInfoCount = signal_semaphore_info ? 1U : 0U,
        .pSignalSemaphoreInfos = signal_semaphore_info,
    };
}

VkPipelineShaderStageCreateInfo pipeline_shader_stage_info( VkShaderStageFlagBits flags,
                                                            VkShaderModule shader_module,
                                                            std::string_view name ) {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = flags,
        .module = shader_module,
        .pName = name.data(),
    };
}

std::optional<VkShaderModule> shader_module( const Common& vulkan,
                                             std::filesystem::path shader_path ) {
    std::ifstream file( shader_path.string(), std::ios::ate | std::ios::binary );

    if ( !file.is_open() ) {
        SDL_Log( "[Shader] Could not open file \"%s\"",
                 std::filesystem::absolute( shader_path ).string().c_str() );
        return {};
    }

    // Use read position to determine size of file and pre-allocate buffer
    std::size_t file_size = static_cast<std::size_t>( file.tellg() );
    std::vector<char> shader_buffer( file_size );

    file.seekg( 0 );
    file.read( shader_buffer.data(), static_cast<std::streamsize>( file_size ) );
    file.close();

    // The pointer is of type `uint32_t`, so we have to cast it. std::vector's default allocator
    // ensures the data satisfies the alignment requirements
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_buffer.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>( shader_buffer.data() );

    VkShaderModule shader_module;
    RACECAR_VK_CHECK( vkCreateShaderModule( vulkan.device, &create_info, nullptr, &shader_module ),
                      "Failed to create shader module" );

    return shader_module;
}

}  // namespace racecar::vk::create

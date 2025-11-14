#include "create.hpp"

#include <SDL3/SDL.h>

#include <fstream>
#include <ios>

namespace racecar::vk::create {

VkCommandPoolCreateInfo command_pool_info(
    uint32_t queue_family_index, VkCommandPoolCreateFlags flags )
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queue_family_index,
    };
}

VkCommandBufferAllocateInfo command_buffer_allocate_info( VkCommandPool pool, uint32_t count )
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count,
    };
}

VkFenceCreateInfo fence_info( VkFenceCreateFlags flags )
{
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };
}

VkSemaphoreCreateInfo semaphore_info()
{
    return { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
}

VkCommandBufferBeginInfo command_buffer_begin_info( VkCommandBufferUsageFlags flags )
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
}

VkImageSubresourceRange image_subresource_range( VkImageAspectFlags aspect_mask )
{
    return {
        .aspectMask = aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

VkImageCreateInfo image_info(
    VkFormat format, VkImageType image_type, VkImageUsageFlags usage_flags, VkExtent3D extent )
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = image_type,

        .format = format,
        .extent = extent,

        .mipLevels = 1,
        .arrayLayers = 1,

        // Samples is used for MSAA
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage_flags,
    };
}

VkImageViewCreateInfo image_view_info(
    VkFormat format, VkImage image, VkImageViewType image_view, VkImageAspectFlags aspect_flags )
{
    VkImageViewCreateInfo info = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,

        .viewType = image_view,
        .format = format,

        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }, 
    };

    return info;
}

VkSemaphoreSubmitInfo semaphore_submit_info(
    VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore )
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stage_mask,
        .deviceIndex = 0,
    };
}

VkCommandBufferSubmitInfo command_buffer_submit_info( VkCommandBuffer command_buffer )
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = command_buffer,
        .deviceMask = 0,
    };
}

VkSubmitInfo2 submit_info( VkCommandBufferSubmitInfo* command_buffer_info,
    VkSemaphoreSubmitInfo* signal_semaphore_info, VkSemaphoreSubmitInfo* wait_semaphore_info )
{
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

VkPipelineShaderStageCreateInfo pipeline_shader_stage_info(
    VkShaderStageFlagBits flags, VkShaderModule shader_module, std::string_view name )
{
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = flags,
        .module = shader_module,
        .pName = name.data(),
    };
}

VkShaderModule shader_module( Common& vulkan, std::filesystem::path shader_path )
{
    std::string absolute = std::filesystem::absolute( shader_path ).string();

    if ( !std::filesystem::exists( shader_path ) ) {
        throw Exception( "[Shader] File \"{}\" does not exist", absolute );
    }

    std::ifstream file( shader_path.string(), std::ios::ate | std::ios::binary );

    if ( !file.is_open() ) {
        throw Exception( "[Shader] Could not open file \"{}\"", absolute );
    }

    // Use read position to determine size of file and pre-allocate buffer
    size_t file_size = static_cast<size_t>( file.tellg() );
    std::vector<char> shader_buffer( file_size );

    file.seekg( 0 );
    file.read( shader_buffer.data(), static_cast<std::streamsize>( file_size ) );
    file.close();

    // The pointer is of type `uint32_t`, so we have to cast it. std::vector's default allocator
    // ensures the data satisfies the alignment requirements
    VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>( shader_buffer.data() ),
    };

    VkShaderModule shader_module;

    // We don't immediately add the shader module to the destructor stack because we destroy it at
    // the end of the pipeline creation instead
    vk::check( vkCreateShaderModule( vulkan.device, &create_info, nullptr, &shader_module ),
        "[Shader] Failed to create shader module" );
    vulkan.destructor_stack.push( vulkan.device, shader_module, vkDestroyShaderModule );

    return shader_module;
}

AllSubmitInfo all_submit_info( CreateSubmitInfoDescriptor submit_info_descriptor )
{
    // Prepare to submit our command to the graphics queue
    VkSemaphoreSubmitInfo wait_info = vk::create::semaphore_submit_info(
        submit_info_descriptor.wait_flag_bits, submit_info_descriptor.wait_semaphore );
    VkSemaphoreSubmitInfo signal_info = vk::create::semaphore_submit_info(
        submit_info_descriptor.signal_flag_bits, submit_info_descriptor.signal_semaphore );
    VkCommandBufferSubmitInfo command_info
        = vk::create::command_buffer_submit_info( submit_info_descriptor.command_buffer );

    return { .wait_info = wait_info, .signal_info = signal_info, .command_info = command_info };
}

VkSubmitInfo2 submit_info_from_all( AllSubmitInfo& all_submit_info )
{
    return vk::create::submit_info(
        &all_submit_info.command_info, &all_submit_info.signal_info, &all_submit_info.wait_info );
}

} // namespace racecar::vk::create

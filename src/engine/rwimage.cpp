#include "rwimage.hpp"

#include "../log.hpp"
#include "images.hpp"
#include "imm_submit.hpp"
#include "../vk/utility.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

RWImage allocate_rwimage(
    vk::Common& vulkan,
    const engine::State& engine,
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags,
    uint32_t mip_levels,
    bool mipmapped
)
{
    RWImage rwimage;

    try {
        for ( size_t i = 0; i < engine.swapchain_images.size(); ++i ) {
            rwimage.images.push_back( allocate_image(
                vulkan,
                extent,
                format,
                image_type,
                mip_levels,
                1,
                samples,
                usage_flags,
                mipmapped
            ) );
        }
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

RWImage create_rwimage(
    vk::Common& vulkan,
    const engine::State& engine,
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags
)
{
    return allocate_rwimage(
        vulkan,
        engine,
        extent,
        format,
        image_type,
        samples,
        usage_flags,
        1,
        false
    );
}

RWImage create_gbuffer_image(
    vk::Common& vulkan, const engine::State& engine, VkFormat format, VkSampleCountFlagBits samples
)
{
    return engine::create_rwimage(
        vulkan,
        engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        format,
        VkImageType::VK_IMAGE_TYPE_2D,
        samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
}

RWImage create_rwimage_mips(
    vk::Common& vulkan,
    const engine::State& engine,
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags,
    uint32_t mip_levels
)
{
    return allocate_rwimage(
        vulkan,
        engine,
        extent,
        format,
        image_type,
        samples,
        usage_flags,
        mip_levels,
        true
    );
}

void initialize_rwimage_layout(
    vk::Common& vulkan, engine::State& engine, RWImage& image, VkImageLayout layout
)
{
    engine::immediate_submit(
        vulkan,
        engine.immediate_submit,
        [&]( VkCommandBuffer command_buffer ) {
            for ( vk::mem::AllocatedImage& frame_image : image.images ) {
                vk::utility::transition_image(
                    command_buffer,
                    frame_image.image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    layout,
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT
                );
            }
        }
    );
}

} // namespace racecar::engine

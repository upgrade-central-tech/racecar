#include "rwimage.hpp"

#include "../log.hpp"
#include "../vk/utility.hpp"
#include "images.hpp"
#include "imm_submit.hpp"

#include <SDL3/SDL.h>

namespace racecar::engine {

RWImage allocate_rwimage(
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
        rwimage.image = allocate_image(
            extent,
            format,
            image_type,
            mip_levels,
            1,
            samples,
            usage_flags,
            mipmapped
        );
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

RWImage create_rwimage(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags
)
{
    return allocate_rwimage( extent, format, image_type, samples, usage_flags, 1, false );
}

RWImage create_gbuffer_image( VkFormat format, VkSampleCountFlagBits samples )
{
    const engine::State& engine = engine::State::GetConst();
    return engine::create_rwimage(
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        format,
        VkImageType::VK_IMAGE_TYPE_2D,
        samples,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );
}

RWImage create_rwimage_mips(
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkSampleCountFlagBits samples,
    VkImageUsageFlags usage_flags,
    uint32_t mip_levels
)
{
    return allocate_rwimage( extent, format, image_type, samples, usage_flags, mip_levels, true );
}

RWImage create_rwimage_from_data(
    void* data,
    VkExtent3D extent,
    VkFormat format,
    VkImageType image_type,
    VkImageUsageFlags usage_flags,
    bool mipmapped
)
{
    RWImage rwimage;

    try {
        rwimage.image = create_image( data, extent, format, image_type, usage_flags, mipmapped );
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

RWImage create_rwimage_from_load(
    std::filesystem::path file_path,
    size_t desired_channels,
    VkFormat image_format,
    bool is_mipmapped
)
{
    RWImage rwimage;

    try {
        rwimage.image = load_image( file_path, desired_channels, image_format, is_mipmapped );
    } catch ( const Exception& ex ) {
        log::error( "[RWImage] Error occurred: {}", ex.what() );
        throw;
    }

    return rwimage;
}

void initialize_rwimage_layout( RWImage& image, VkImageLayout layout )
{
    const engine::State& engine = engine::State::GetConst();
    engine::immediate_submit( engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
        vk::utility::transition_image(
            command_buffer,
            image.image.vk_image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            layout,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    } );
}

} // namespace racecar::engine

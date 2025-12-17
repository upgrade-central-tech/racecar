#include "deferred.hpp"

namespace racecar::deferred {

GBuffers initialize_GBuffers( vk::Common& vulkan, engine::State& engine )
{
    GBuffers gbuffers;

    // deferred rendering
    gbuffers.GBuffer_Normal = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_Position = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_Tangent = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_UV = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_Albedo = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_Packed_Data = engine::create_gbuffer_image(
        vulkan, engine, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT );

    gbuffers.GBuffer_Depth = engine::create_rwimage( vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_D32_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    gbuffers.GBuffer_DepthMS = engine::create_rwimage( vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_D32_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_4_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    gbuffers.GBuffer_Velocity = engine::create_rwimage( vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    gbuffers.desc_set = engine::generate_descriptor_set( vulkan, engine,
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // NORMAL
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // POSITION
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // TANGENT
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // UV
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // ALBEDO
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // PACKED_DATA
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // DEPTH
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // VELOCITY
        },
        VK_SHADER_STAGE_FRAGMENT_BIT );

    return gbuffers;
}

std::vector<engine::RWImage> get_color_attachments( GBuffers& gbuffers )
{
    return {
        gbuffers.GBuffer_Position,
        gbuffers.GBuffer_Normal,
        gbuffers.GBuffer_Tangent,
        gbuffers.GBuffer_UV,
        gbuffers.GBuffer_Albedo,
        gbuffers.GBuffer_Packed_Data,
        gbuffers.GBuffer_Velocity,
    };
}

engine::RWImage get_depth_image( GBuffers& gbuffers ) { return gbuffers.GBuffer_Depth; }

engine::PipelineBarrierDescriptor initialize_barrier_desc( deferred::GBuffers& gbuffers )
{
    return { .buffer_barriers = {},
        .image_barriers = { init_to_color_write( gbuffers.GBuffer_Normal ),
            init_to_color_write( gbuffers.GBuffer_Position ),
            init_to_color_write( gbuffers.GBuffer_Velocity ),
            init_to_color_write( gbuffers.GBuffer_Tangent ),
            init_to_color_write( gbuffers.GBuffer_UV ),
            init_to_color_write( gbuffers.GBuffer_Albedo ),
            init_to_color_write( gbuffers.GBuffer_Packed_Data ),
            engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .src_access = VK_ACCESS_2_NONE,
                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .image = gbuffers.GBuffer_Depth,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
            engine::ImageBarrier {
                .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .src_access = VK_ACCESS_2_NONE,
                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .image = gbuffers.GBuffer_DepthMS,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH,
            } } };
}

engine::ImageBarrier init_to_color_write( engine::RWImage& image )
{
    return {
        .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .src_access = VK_ACCESS_2_NONE,
        .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = image,
        .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
    };
}

engine::ImageBarrier color_write_to_frag_read( engine::RWImage& image )
{
    return {
        .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
        .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .image = image,
        .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
    };
};

void update_desc_sets( vk::Common& vulkan, engine::State& engine, GBuffers& gbuffers )
{
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set,
        gbuffers.GBuffer_Position, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set,
        gbuffers.GBuffer_Normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set,
        gbuffers.GBuffer_Tangent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2 );
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set, gbuffers.GBuffer_UV,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3 );
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set,
        gbuffers.GBuffer_Albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4 );
    engine::update_descriptor_set_depth_image(
        vulkan, engine, gbuffers.desc_set, gbuffers.GBuffer_Depth, 5 );
    engine::update_descriptor_set_depth_image(
        vulkan, engine, gbuffers.desc_set, gbuffers.GBuffer_DepthMS, 6 );
    engine::update_descriptor_set_rwimage( vulkan, engine, gbuffers.desc_set,
        gbuffers.GBuffer_Packed_Data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 7 );
}

}

#include "atmosphere_baker.hpp"

#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "lut_sets.hpp"
#include "vk/create.hpp"
#include "vk/utility.hpp"

#include <string_view>

namespace racecar::atmosphere {

static constexpr std::string_view BAKE_ATMS_SHADER_PATH
    = "../shaders/atmosphere/sky/bake_atmosphere.spv";
static constexpr std::string_view BAKE_ATMS_MIPS_SHADER_PATH
    = "../shaders/atmosphere/sky/bake_atmosphere_mips.spv";

static const uint32_t mip0_size = 512;
static const uint32_t mip_levels = 5;

void initialize_atmosphere_baker(
    AtmosphereBaker& atms_baker, const volumetric::Volumetric& volumetric
)
{
    const vk::Common& vulkan = vk::Common::GetConst();
    const engine::State& engine = engine::State::GetConst();
    uint32_t octahedral_sky_size = 512;

    atms_baker.octahedral_sky = engine::allocate_image(
        { octahedral_sky_size, octahedral_sky_size, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        false
    );

    atms_baker.octahedral_write_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    atms_baker.volumetrics_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_write_image(
        atms_baker.octahedral_write_desc_set,
        atms_baker.octahedral_sky,
        0
    );

    engine::update_descriptor_set_image(
        atms_baker.volumetrics_desc_set,
        volumetric.cumulus_map,
        0
    );
    engine::update_descriptor_set_image(
        atms_baker.volumetrics_desc_set,
        volumetric.low_freq_noise,
        1
    );
    engine::update_descriptor_set_sampler(
        atms_baker.volumetrics_desc_set,
        vulkan.global_samplers.linear_mirrored_repeat_sampler,
        2
    );
    engine::update_descriptor_set_uniform(
        atms_baker.volumetrics_desc_set,
        volumetric.uniform_buffer,
        3
    );

    atms_baker.cs_bake_atmosphere_pipeline = engine::create_compute_pipeline(
        {
            atms_baker.atmosphere->uniform_desc_set.layouts[0],
            atms_baker.atmosphere->lut_desc_set.layouts[0],
            atms_baker.atmosphere->sampler_desc_set.layouts[0],
            atms_baker.octahedral_write_desc_set.layouts[0],
            atms_baker.volumetrics_desc_set.layouts[0],
        },
        vk::create::shader_module( BAKE_ATMS_SHADER_PATH ),
        "cs_bake_atmosphere"
    );

    atms_baker.cs_octahedral_mip_pipeline = engine::create_compute_pipeline(
        {
            // Repeated code everywhere, is there a way to cache this vector?
            atms_baker.atmosphere->uniform_desc_set.layouts[0],
            atms_baker.atmosphere->lut_desc_set.layouts[0],
            atms_baker.atmosphere->sampler_desc_set.layouts[0],
            atms_baker.octahedral_write_desc_set.layouts[0],
            atms_baker.volumetrics_desc_set.layouts[0],
        },
        vk::create::shader_module( BAKE_ATMS_MIPS_SHADER_PATH ),
        "cs_bake_atmosphere_mips"
    );

    // TODO: Refactor this mip generation somewhere else.
    // Mip generation itself should be abstracted away.
    atms_baker.octahedral_sky_mips = engine::create_rwimage_mips(
        { mip0_size, mip0_size, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        mip_levels
    );

    atms_baker.mip_data.resize( mip_levels );
    for ( size_t mip = 0; mip < mip_levels; mip++ ) {
        atms_baker.mip_data[mip]
            = create_uniform_buffer<ub_data::OctahedralData>( { }, engine.frame_overlap );
    }
}

void atmosphere_baker_precompute( AtmosphereBaker& atms_baker, VkCommandBuffer precompute_cmdbuf )
{
    vk::utility::transition_image(
        precompute_cmdbuf,
        atms_baker.octahedral_sky.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
}

void compute_octahedral_sky( AtmosphereBaker& atms_baker, engine::TaskList& task_list )
{
    Atmosphere& atms = *atms_baker.atmosphere;
    const vk::mem::AllocatedImage& octahedral_sky = atms_baker.octahedral_sky;

    uint32_t x_groups = ( static_cast<uint32_t>( octahedral_sky.image_extent.width ) + 7 ) / 8;
    uint32_t y_groups = ( static_cast<uint32_t>( octahedral_sky.image_extent.width ) + 7 ) / 8;

    engine::ComputeTask cs_bake_atmosphere_task = {
        atms_baker.cs_bake_atmosphere_pipeline,
        {
            &atms.uniform_desc_set,
            &atms.lut_desc_set,
            &atms.sampler_desc_set,
            &atms_baker.octahedral_write_desc_set,
            &atms_baker.volumetrics_desc_set,
        },
        glm::ivec3( x_groups, y_groups, 1 ),
    };

    engine::add_cs_task( task_list, cs_bake_atmosphere_task );
}

void compute_octahedral_sky_mips( AtmosphereBaker& atms_baker, engine::TaskList& task_list )
{
    Atmosphere& atms = *atms_baker.atmosphere;

    for ( size_t mip = 0; mip < mip_levels; mip++ ) {
        atms_baker.octahedral_mip_writes.push_back(
            engine::generate_descriptor_set(
                {
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                },
                VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            )
        );

        engine::update_descriptor_set_write_image(
            atms_baker.octahedral_mip_writes[mip],
            atms_baker.octahedral_sky,
            0
        );
        engine::update_descriptor_set_rwimage_mip(
            atms_baker.octahedral_mip_writes[mip],
            atms_baker.octahedral_sky_mips,
            VK_IMAGE_LAYOUT_GENERAL,
            1,
            mip
        );

        float roughness = (float)mip / (float)( mip_levels - 1 );
        atms_baker.mip_data[mip].set_data( { glm::vec4( mip, roughness, 0.0f, 0.0f ) } );

        atms_baker.mip_data[mip].update_all();

        engine::update_descriptor_set_uniform(
            atms_baker.octahedral_mip_writes[mip],
            atms_baker.mip_data[mip],
            2
        );
    }

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = { },
                                            .image_barriers = { engine::ImageBarrier {
                                                .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                                .src_access = VK_ACCESS_2_NONE,
                                                .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                .dst_access = VK_ACCESS_SHADER_WRITE_BIT,
                                                .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                                                .image = &atms_baker.octahedral_sky_mips,
                                                .range = {
                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                    .baseMipLevel = 0,
                                                    .levelCount = mip_levels,
                                                    .baseArrayLayer = 0,
                                                    .layerCount = 1,
                                                } } } }
    );

    for ( size_t mip = 0; mip < mip_levels; mip++ ) {
        // Swap the descriptor set to write to each mip level...
        // Need to change what mip level we're writing to
        uint32_t mip_size = mip0_size >> mip;

        glm::ivec2 dims = {
            ( mip_size + 7 ) / 8,
            ( mip_size + 7 ) / 8,
        };

        engine::ComputeTask cs_mip_task = { atms_baker.cs_octahedral_mip_pipeline,
                                            {
                                                &atms.uniform_desc_set,
                                                &atms.lut_desc_set,
                                                &atms.sampler_desc_set,
                                                &atms_baker.octahedral_mip_writes[mip],
                                                &atms_baker.volumetrics_desc_set,
                                            },
                                            glm::ivec3( dims, 1 ) };

        engine::add_cs_task( task_list, cs_mip_task );
    }

    engine::add_pipeline_barrier(
        task_list,
        engine::PipelineBarrierDescriptor {
            .buffer_barriers = { },
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                       .src_access = VK_ACCESS_SHADER_WRITE_BIT,
                                       .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                                       .dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                       .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                                       .dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       .image = &atms_baker.octahedral_sky_mips,
                                       .range = {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel = 0,
                                           .levelCount = mip_levels,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1,
                                       } } } }
    );
}

/// Runs the atmosphere baker, populating the octahedral sky, irradiance, and mip textures
/// respectively. After population, this function populates their respective LUTS.
// TODO: the atmosphere baking heavily relies on populated volumetric LUTs, making this tightly
// coupled. Need some way to skip volumetrics in the atmosphere baker if volumetrics are disabled.
void dispatch_atmosphere_baker(
    engine::TaskList& task_list,
    engine::DescriptorSet& lut_sets,
    atmosphere::AtmosphereBaker& atms_baker
)
{
    atmosphere::compute_octahedral_sky( atms_baker, task_list );

    atmosphere::compute_octahedral_sky_mips( atms_baker, task_list );

    engine::update_descriptor_set_image(
        lut_sets,
        atms_baker.octahedral_sky,
        LUT_INDEX::OCTAHEDRAL_SKY
    );
    engine::update_descriptor_set_rwimage(
        lut_sets,
        atms_baker.octahedral_sky_mips,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        LUT_INDEX::OCTAHEDRAL_MIPS
    );
}

}

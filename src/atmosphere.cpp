#include "atmosphere.hpp"

#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "engine/ub_data.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "vk/create.hpp"
#include "vk/utility.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace racecar::atmosphere {

constexpr std::string_view IRRADIANCE_DAT_PATH = "../assets/atmosphere/irradiance.dat";
constexpr std::string_view SCATTERING_DAT_PATH = "../assets/atmosphere/scattering.dat";
constexpr std::string_view TRANSMITTANCE_DAT_PATH = "../assets/atmosphere/transmittance.dat";

namespace {

std::vector<float> read_data( std::string_view path )
{
    std::string absolute = std::filesystem::absolute( path ).string();

    if ( !std::filesystem::exists( absolute ) ) {
        throw Exception( "[atmosphere] File \"{}\" does not exist", absolute );
    }

    std::ifstream file( absolute, std::ios::ate | std::ios::binary );

    if ( !file.is_open() ) {
        throw Exception( "[atmosphere] Could not open file \"{}\"", absolute );
    }

    std::streampos file_size = file.tellg();
    size_t num_floats = static_cast<size_t>( file_size ) / sizeof( float );
    std::vector<float> dat_buffer( num_floats );

    file.seekg( 0, std::ios::beg );
    file.read( reinterpret_cast<char*>( dat_buffer.data() ), file_size );
    file.close();

    return dat_buffer;
}

}

Atmosphere initialize( vk::Common& vulkan, engine::State& engine )
{
    Atmosphere atms;

    // Fill in the unchanging parameters
    ub_data::Atmosphere atms_ub = {
        .sun_size_x = std::tan( atmosphere::SUN_ANGULAR_RADIUS ),
        .white_point = atmosphere::WHITE_POINT,
        .sun_size_y = std::cos( atmosphere::SUN_ANGULAR_RADIUS ),
        .earth_center = glm::vec4( atmosphere::EARTH_CENTER, 0.f ),
    };

    atms.uniform_buffer = create_uniform_buffer<ub_data::Atmosphere>(
        vulkan, atms_ub, static_cast<size_t>( engine.frame_overlap ) );
    atms.uniform_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );
    engine::update_descriptor_set_uniform(
        vulkan, engine, atms.uniform_desc_set, atms.uniform_buffer, 0 );

    try {
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        {
            std::vector<float> irradiance = read_data( IRRADIANCE_DAT_PATH );
            atms.irradiance = engine::create_image( vulkan, engine,
                static_cast<void*>( irradiance.data() ), { .width = 64, .height = 16, .depth = 1 },
                format, VK_IMAGE_TYPE_2D, usage_flags, false );
            log::info( "[atmosphere] Created irradiance image" );
        }

        {
            std::vector<float> scattering = read_data( SCATTERING_DAT_PATH );
            atms.scattering
                = engine::create_image( vulkan, engine, static_cast<void*>( scattering.data() ),
                    { .width = 256, .height = 128, .depth = 32 }, format, VK_IMAGE_TYPE_3D,
                    usage_flags, false );
            log::info( "[atmosphere] Created scattering image" );
        }

        {
            std::vector<float> transmittance = read_data( TRANSMITTANCE_DAT_PATH );
            atms.transmittance
                = engine::create_image( vulkan, engine, static_cast<void*>( transmittance.data() ),
                    { .width = 256, .height = 64, .depth = 1 }, format, VK_IMAGE_TYPE_2D,
                    usage_flags, false );
            log::info( "[atmosphere] Created irradiance image" );
        }
    } catch ( const Exception& ex ) {
        log::error( "[atmosphere] Failed to create LUTs: {}", ex.what() );
        throw;
    }

    atms.lut_desc_set = engine::generate_descriptor_set( vulkan, engine,
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );
    engine::update_descriptor_set_image( vulkan, engine, atms.lut_desc_set, atms.irradiance, 0 );
    engine::update_descriptor_set_image( vulkan, engine, atms.lut_desc_set, atms.scattering, 1 );
    engine::update_descriptor_set_image( vulkan, engine, atms.lut_desc_set, atms.transmittance, 2 );

    VkSampler sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        vk::check( vkCreateSampler( vulkan.device, &sampler_info, nullptr, &sampler ),
            "Failed to create atmosphere LUT sampler" );
        vulkan.destructor_stack.push( vulkan.device, sampler, vkDestroySampler );
    }
    atms.sampler_desc_set
        = engine::generate_descriptor_set( vulkan, engine, { VK_DESCRIPTOR_TYPE_SAMPLER },
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT );
    engine::update_descriptor_set_sampler( vulkan, engine, atms.sampler_desc_set, sampler, 0 );

    // Some default states
    atms.sun_azimuth = 2.9f;
    atms.sun_zenith = 1.49f;
    atms.exposure = 10.f;

    return atms;
}

glm::vec3 compute_sun_direction( const Atmosphere& atms )
{
    return {
        std::cos( atms.sun_azimuth ) * std::sin( atms.sun_zenith ),
        std::sin( atms.sun_azimuth ) * std::sin( atms.sun_zenith ),
        std::cos( atms.sun_zenith ),
    };
}

vk::mem::AllocatedImage generate_octahedral_sky(
    const Atmosphere& atms, vk::Common& vulkan, engine::State& engine )
{
    uint32_t octahedral_sky_size = 1024;

    vk::mem::AllocatedImage octahedral_sky = engine::allocate_image( vulkan,
        { octahedral_sky_size, octahedral_sky_size, 1 }, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D, 1, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false );

    {
        engine::DescriptorSet octahedral_write = engine::generate_descriptor_set(
            vulkan, engine, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_SHADER_STAGE_COMPUTE_BIT );

        engine::update_descriptor_set_write_image(
            vulkan, engine, octahedral_write, octahedral_sky, 0 );

        VkShaderModule bake_atmosphere_module
            = vk::create::shader_module( vulkan, "../shaders/atmosphere/bake_atmosphere.spv" );

        std::vector<engine::DescriptorSet> descs
            = { atms.uniform_desc_set, atms.lut_desc_set, atms.sampler_desc_set, octahedral_write };

        engine::Pipeline compute_pipeline = engine::create_compute_pipeline( vulkan,
            { atms.uniform_desc_set.layouts[0], atms.lut_desc_set.layouts[0],
                atms.sampler_desc_set.layouts[0], octahedral_write.layouts[0] },
            bake_atmosphere_module, "cs_bake_atmosphere" );

        engine::immediate_submit(
            vulkan, engine.immediate_submit, [&]( VkCommandBuffer command_buffer ) {
                // RW cubemap transition first
                vk::utility::transition_image( command_buffer, octahedral_sky.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT );

                vkCmdBindPipeline(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle );

                std::vector<VkDescriptorSet> bind_descs = {
                    atms.uniform_desc_set.descriptor_sets[0], atms.lut_desc_set.descriptor_sets[0],
                    atms.sampler_desc_set.descriptor_sets[0], octahedral_write.descriptor_sets[0]
                };

                vkCmdBindDescriptorSets( command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline.layout, 0, 4, bind_descs.data(), 0, nullptr );

                uint32_t x_groups = ( static_cast<uint32_t>( octahedral_sky_size ) + 7 ) / 8;
                uint32_t y_groups = ( static_cast<uint32_t>( octahedral_sky_size ) + 7 ) / 8;

                vkCmdDispatch( command_buffer, x_groups, y_groups, 1 );

                vk::utility::transition_image( command_buffer, octahedral_sky.image,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT );
            } );
    }

    return octahedral_sky;
}

}

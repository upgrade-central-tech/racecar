#include "atmosphere.hpp"

#define GLM_ENABLE_EXPERIMENTAL // Necessary for glm::lerp
#include "camera_data.hpp"
#include "engine/images.hpp"
#include "engine/ub_data.hpp"
#include "exception.hpp"
#include "geometry/quad.hpp"
#include "gui.hpp"
#include "log.hpp"
#include "vk/create.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/compatibility.hpp>

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

    static_assert( sizeof( float ) == 4, "float data type is not 32 bits (somehow)" );

    std::streampos file_size = file.tellg();
    size_t num_floats = static_cast<size_t>( file_size ) / sizeof( float );
    std::vector<float> dat_buffer( num_floats );

    file.seekg( 0, std::ios::beg );
    file.read( reinterpret_cast<char*>( dat_buffer.data() ), file_size );
    file.close();

    return dat_buffer;
}

}

Atmosphere initialize()
{
    vk::Common& vulkan = vk::Common::GetMut();
    const engine::State& engine = engine::State::GetConst();
    Atmosphere atms;

    atms.uniform_buffer = create_uniform_buffer<ub_data::Atmosphere>(
        { },
        static_cast<size_t>( engine.frame_overlap )
    );
    atms.uniform_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_uniform( atms.uniform_desc_set, atms.uniform_buffer, 0 );

    try {
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        {
            std::vector<float> irradiance = read_data( IRRADIANCE_DAT_PATH );
            atms.irradiance = engine::create_image(
                static_cast<void*>( irradiance.data() ),
                { .width = 64, .height = 16, .depth = 1 },
                format,
                VK_IMAGE_TYPE_2D,
                usage_flags,
                false
            );
        }

        {
            std::vector<float> scattering = read_data( SCATTERING_DAT_PATH );
            atms.scattering = engine::create_image(
                static_cast<void*>( scattering.data() ),
                { .width = 256, .height = 128, .depth = 32 },
                format,
                VK_IMAGE_TYPE_3D,
                usage_flags,
                false
            );
        }

        {
            std::vector<float> transmittance = read_data( TRANSMITTANCE_DAT_PATH );
            atms.transmittance = engine::create_image(
                static_cast<void*>( transmittance.data() ),
                { .width = 256, .height = 64, .depth = 1 },
                format,
                VK_IMAGE_TYPE_2D,
                usage_flags,
                false
            );
        }
    } catch ( const Exception& ex ) {
        log::error( "[atmosphere] Failed to create LUTs: {}", ex.what() );
        throw;
    }

    atms.lut_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_image( atms.lut_desc_set, atms.irradiance, 0 );
    engine::update_descriptor_set_image( atms.lut_desc_set, atms.scattering, 1 );
    engine::update_descriptor_set_image( atms.lut_desc_set, atms.transmittance, 2 );

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
        vk::check(
            vkCreateSampler( vulkan.device, &sampler_info, nullptr, &sampler ),
            "Failed to create atmosphere LUT sampler"
        );
        vulkan.destructor_stack.push( vulkan.device, sampler, vkDestroySampler );
    }
    atms.sampler_desc_set = engine::generate_descriptor_set(
        { VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );
    engine::update_descriptor_set_sampler( atms.sampler_desc_set, sampler, 0 );

    // Some default states
    atms.sun_azimuth = 2.9f;
    atms.sun_zenith = 1.3f;

    return atms;
}

glm::vec3 compute_sun_direction( const Atmosphere& atms )
{
    return {
        std::cos( atms.sun_azimuth ) * std::sin( atms.sun_zenith ),
        std::cos( atms.sun_zenith ),
        std::sin( atms.sun_azimuth ) * std::sin( atms.sun_zenith ),
    };
}

void draw_atmosphere(
    engine::TaskList& task_list, atmosphere::Atmosphere& atms, engine::RWImage& out_color
)
{
    const engine::State& engine = engine::State::GetConst();
    engine::GfxTask atmosphere_gfx_task = {
        .clear_color = { { { 0.f, 1.f, 0.f, 1.f } } },
        .clear_depth = 1.f,
        .color_attachments = { &out_color },
        .extent = engine.swapchain.extent,
    };

    engine::Pipeline atmosphere_pipeline;
    geometry::quad::Mesh& quad_mesh = geometry::quad::Mesh::get_instance();

    try {
        atmosphere_pipeline = engine::create_gfx_pipeline(
            engine::get_vertex_input_state_create_info( geometry::quad::Mesh::get_instance() ),
            {
                atms.uniform_desc_set.layouts[0],
                atms.lut_desc_set.layouts[0],
                atms.sampler_desc_set.layouts[0],
            },
            {
                VK_FORMAT_R16G16B16A16_SFLOAT,
            },
            VK_SAMPLE_COUNT_1_BIT,
            false,
            true,
            vk::create::shader_module( atmosphere::SHADER_PATH ),
            false
        );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create atmosphere graphics pipeline: {}", ex.what() );
        throw;
    }

    atmosphere_gfx_task.draw_tasks.push_back( {
                .draw_resource_descriptor = {
                        .vertex_buffers = { quad_mesh.mesh_buffers.vertex_buffer.handle },
                        .index_buffer = quad_mesh.mesh_buffers.index_buffer.handle,
                        .vertex_buffer_offsets = { 0 },
                        .index_count = static_cast<uint32_t>( quad_mesh.indices.size() ),
                },
                .descriptor_sets = {
                    &atms.uniform_desc_set,
                    &atms.lut_desc_set,
                    &atms.sampler_desc_set,
                },
                .pipeline = atmosphere_pipeline,
            } );

    engine::add_gfx_task( task_list, atmosphere_gfx_task );
}

void update_atmosphere_uniform_buffer(
    gui::Gui& gui, atmosphere::Atmosphere& atms, const CameraData& camera_data
)
{
    const engine::State& engine = engine::State::GetConst();
    if ( gui.atms.animate_zenith ) {
        float sin = std::sin( static_cast<float>( engine.time ) * gui.atms.animate_zenith_speed );
        float t = ( sin + 1.f ) * 0.5f;
        atms.sun_zenith = glm::lerp( -glm::half_pi<float>(), glm::half_pi<float>(), t );
    }

    glm::vec3 atmosphere_position = {
        camera_data.position.x,
        // A y-value of 9 means the camera is 9 km above the surface. This is pretty
        // ridiculous so we manually adjust it here. Now y needs to be 900.
        camera_data.position.y * 0.01f,
        camera_data.position.z,
    };

    ub_data::Atmosphere atms_ub = atms.uniform_buffer.get_data();
    atms_ub.inverse_proj = glm::inverse( camera_data.projection );
    atms_ub.inverse_view = glm::inverse( camera_data.view );
    atms_ub.camera_position = atmosphere_position;
    atms_ub.sun_direction = atmosphere::compute_sun_direction( atms );
    atms_ub.radiance_exposure = gui.atms.radiance_exposure;

    atms.uniform_buffer.set_data( atms_ub );
    atms.uniform_buffer.update( engine.get_frame_index() );
}

}

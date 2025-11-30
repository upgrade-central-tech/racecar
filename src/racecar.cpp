#include "racecar.hpp"

#define ENABLE_VOLUMETRICS 0
#define ENABLE_TERRAIN 1
#define ENABLE_DEFERRED_AA 1

#include "atmosphere.hpp"
#include "atmosphere_baker.hpp"
#include "constants.hpp"
#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/execute.hpp"
#include "engine/images.hpp"
#include "engine/pipeline.hpp"
#include "engine/post/ao.hpp"
#include "engine/post/bloom.hpp"
#include "engine/prepass.hpp"
#include "engine/state.hpp"
#include "engine/task_list.hpp"
#include "engine/uniform_buffer.hpp"
#include "geometry/ibl.hpp"
#include "geometry/procedural.hpp"
#include "geometry/quad.hpp"
#include "gui.hpp"
#include "scene/scene.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

#if ENABLE_TERRAIN
#include "terrain/terrain.hpp"
#endif

#if ENABLE_VOLUMETRICS
#include "volumetrics.hpp"
#endif

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL // Necessary for glm::lerp
#include <glm/gtx/compatibility.hpp>

#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>

namespace racecar {

namespace {

constexpr std::string_view GLTF_FILE_PATH = "../assets/smoother_suzanne.glb";
constexpr std::string_view SHADER_MODULE_PATH = "../shaders/deferred/prepass.spv";
constexpr std::string_view LIGHTING_PASS_SHADER_MODULE_PATH = "../shaders/deferred/lighting.spv";
constexpr std::string_view TEST_CUBEMAP_PATH = "../assets/cubemaps/test";
constexpr std::string_view BRDF_LUT_PATH = "../assets/LUT/BRDF.bmp";

constexpr std::string_view DEPTH_PREPASS_SHADER_MODULE_PATH
    = "../shaders/deferred/depth_prepass.spv";

}

void run( bool use_fullscreen )
{
    Context ctx;
    ctx.window = sdl::initialize( constant::SCREEN_W, constant::SCREEN_H, use_fullscreen ),
    ctx.vulkan = vk::initialize( ctx.window );

    engine::State engine = engine::initialize( ctx );
    gui::Gui gui = gui::initialize( ctx, engine );

    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::scene::Mesh scene_mesh;
    scene::load_gltf(
        ctx.vulkan, engine, GLTF_FILE_PATH, scene, scene_mesh.vertices, scene_mesh.indices );
    geometry::scene::generate_tangents( scene_mesh );
    scene_mesh.mesh_buffers = geometry::scene::upload_mesh(
        ctx.vulkan, engine, scene_mesh.indices, scene_mesh.vertices );

    UniformBuffer camera_buffer = create_uniform_buffer<ub_data::Camera>(
        ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );
    UniformBuffer debug_buffer = create_uniform_buffer<ub_data::Debug>(
        ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );
    // UniformBuffer raymarch_buffer = create_uniform_buffer<ub_data::RaymarchBufferData>(
    //     ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    engine::DescriptorSet uniform_desc_set = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

    engine::update_descriptor_set_uniform( ctx.vulkan, engine, uniform_desc_set, camera_buffer, 0 );
    engine::update_descriptor_set_uniform( ctx.vulkan, engine, uniform_desc_set, debug_buffer, 1 );

    // Simple set up for linear sampler
    VkSampler linear_sampler = VK_NULL_HANDLE;
    VkSampler point_sampler = VK_NULL_HANDLE;
    engine::DescriptorSet sampler_desc_set;
    {
        VkSamplerCreateInfo sampler_linear_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
        };

        vk::check( vkCreateSampler(
                       ctx.vulkan.device, &sampler_linear_create_info, nullptr, &linear_sampler ),
            "Failed to create sampler" );
        ctx.vulkan.destructor_stack.push( ctx.vulkan.device, linear_sampler, vkDestroySampler );

        VkSamplerCreateInfo sampler_nearest_create_info
            = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                  .magFilter = VK_FILTER_NEAREST,
                  .minFilter = VK_FILTER_NEAREST };

        vk::check( vkCreateSampler(
                       ctx.vulkan.device, &sampler_nearest_create_info, nullptr, &point_sampler ),
            "Failed to create nearest smapler!" );
        ctx.vulkan.destructor_stack.push( ctx.vulkan.device, point_sampler, vkDestroySampler );

        sampler_desc_set = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

        engine::update_descriptor_set_sampler(
            ctx.vulkan, engine, sampler_desc_set, linear_sampler, 0 );

        engine::update_descriptor_set_sampler(
            ctx.vulkan, engine, sampler_desc_set, point_sampler, 1 );
    }

    size_t num_materials = scene.materials.size();
    std::vector<engine::DescriptorSet> material_desc_sets( num_materials );
    std::vector<UniformBuffer<ub_data::Material>> material_uniform_buffers( num_materials );

    for ( size_t i = 0; i < num_materials; i++ ) {
        // Generate a separate texture descriptor set for each of the materials
        material_desc_sets[i] = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

        scene::Material& mat = scene.materials[i];

        UniformBuffer material_buffer = create_uniform_buffer<ub_data::Material>(
            ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );
        ub_data::Material material_ub = {
            .has_base_color_texture = mat.base_color_texture_index.has_value() ? 1 : 0,
            .has_metallic_roughness_texture
            = mat.metallic_roughness_texture_index.has_value() ? 1 : 0,
            .has_normal_texture = mat.normal_texture_index.has_value() ? 1 : 0,
            .has_emmisive_texture = mat.emmisive_texture_index.has_value() ? 1 : 0,

            .base_color = glm::vec4( mat.base_color, 1.0 ),

            .metallic = mat.metallic,
            .roughness = mat.roughness,
            .normal_texture_weight = (float)mat.normal_texture_weight,
            .ior = mat.ior,

            .specular_tint = mat.specular_tint,
            .specular = mat.specular,

            .sheen_tint = mat.sheen_tint,
            .sheen_roughness = mat.sheen_roughness,

            .sheen_weight = mat.sheen_weight,
            .transmission = mat.transmission,
            .clearcoat = mat.clearcoat,
            .clearcoat_roughness = mat.clearcoat_roughness,

            .emissive = mat.emissive,
            .unlit = mat.unlit,
        };

        material_buffer.set_data( material_ub );
        material_buffer.update( ctx.vulkan, engine.get_frame_index() );

        engine::update_descriptor_set_uniform(
            ctx.vulkan, engine, material_desc_sets[i], material_buffer, 3 );
        material_uniform_buffers[i] = std::move(material_buffer);
    }

    engine::DescriptorSet raymarch_tex_sets;
    {
        raymarch_tex_sets = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

        vk::mem::AllocatedImage test_data_3D = geometry::generate_test_3D( ctx.vulkan, engine );
        engine::update_descriptor_set_image(
            ctx.vulkan, engine, raymarch_tex_sets, test_data_3D, 0 );
    }

    engine::DescriptorSet lut_sets;
    {
        lut_sets = engine::generate_descriptor_set( ctx.vulkan, engine,
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );

        vk::mem::AllocatedImage test_cubemap
            = geometry::create_cubemap( TEST_CUBEMAP_PATH, ctx.vulkan, engine );
        vk::mem::AllocatedImage lut_brdf
            = engine::load_image( BRDF_LUT_PATH, ctx.vulkan, engine, 2, VK_FORMAT_R16G16_SFLOAT );

        UniformBuffer sh_buffer = create_uniform_buffer<ub_data::SHData>(
            ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

        vk::mem::AllocatedImage diffuse_irradiance
            = geometry::generate_diffuse_irradiance( TEST_CUBEMAP_PATH, ctx.vulkan, engine );

        vk::mem::AllocatedImage diffuse_irradiance_sh
            = geometry::cs_generate_diffuse_sh( test_cubemap, linear_sampler, ctx.vulkan, engine );

        vk::mem::AllocatedImage glint_noise = geometry::generate_glint_noise( ctx.vulkan, engine );

        engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, test_cubemap, 0 );
        engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, lut_brdf, 1 );
        engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, diffuse_irradiance, 2 );
        engine::update_descriptor_set_image(
            ctx.vulkan, engine, lut_sets, diffuse_irradiance_sh, 3 );
        engine::update_descriptor_set_image( ctx.vulkan, engine, lut_sets, glint_noise, 4 );

        // nothing in the lut for the sky.. YET
        engine::update_descriptor_set_uniform( ctx.vulkan, engine, lut_sets, sh_buffer, 6 );

        std::vector<glm::vec3> sh_coefficients = geometry::generate_diffuse_sh( TEST_CUBEMAP_PATH );
        for ( size_t i = 0; i < sh_coefficients.size(); ++i ) {
            log::info( "SH coeff: {}, {}, {}", sh_coefficients[i].x, sh_coefficients[i].y,
                sh_coefficients[i].z );
        }

        for ( size_t i = 0; i < engine.frame_overlap; i++ ) {
            ub_data::SHData SH_ub = {
                .coeff0 = glm::vec4( sh_coefficients[0].r, sh_coefficients[0].g,
                    sh_coefficients[0].b, sh_coefficients[1].r ),
                .coeff1 = glm::vec4( sh_coefficients[1].g, sh_coefficients[1].b,
                    sh_coefficients[2].r, sh_coefficients[2].g ),
                .coeff2 = glm::vec4( sh_coefficients[2].b, sh_coefficients[3].r,
                    sh_coefficients[3].g, sh_coefficients[3].b ),
                .coeff3 = glm::vec4( sh_coefficients[4].r, sh_coefficients[4].g,
                    sh_coefficients[4].b, sh_coefficients[5].r ),
                .coeff4 = glm::vec4( sh_coefficients[5].g, sh_coefficients[5].b,
                    sh_coefficients[6].r, sh_coefficients[6].g ),
                .coeff5 = glm::vec4( sh_coefficients[6].b, sh_coefficients[7].r,
                    sh_coefficients[7].g, sh_coefficients[7].b ),
                .coeff6 = glm::vec4(
                    sh_coefficients[8].r, sh_coefficients[8].g, sh_coefficients[8].b, 0.0f ),
            };

            sh_buffer.set_data( SH_ub );
            sh_buffer.update( ctx.vulkan, i );
        }
    }

    // Depth prepass setup
    engine::RWImage GBuffer_DepthMS = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_D32_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_4_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::DescriptorSet depth_uniform_desc_set = engine::generate_descriptor_set(
        ctx.vulkan, engine, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, VK_SHADER_STAGE_VERTEX_BIT );

    engine::update_descriptor_set_uniform(
        ctx.vulkan, engine, depth_uniform_desc_set, camera_buffer, 0 );

    // DEPTH_MS PRE-PASS
    engine::Pipeline depth_ms_pipeline;
    try {
        // Pipeline needs to support MSAA
        size_t frame_index = engine.get_frame_index();
        depth_ms_pipeline = create_gfx_pipeline( engine, ctx.vulkan,
            engine::get_vertex_input_state_create_info( scene_mesh ),
            { depth_uniform_desc_set.layouts[frame_index] }, {}, VK_SAMPLE_COUNT_4_BIT, false, true,
            vk::create::shader_module( ctx.vulkan, DEPTH_PREPASS_SHADER_MODULE_PATH ) );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create depth-MS-prepass pipeline: {}", ex.what() );
        throw;
    }

    engine::GfxTask depth_ms_gfx_task = {
        .clear_color = { { { 0.0f, 0.0f, 0.0f, 0.0f } } },
        .clear_depth = 1.f,
        .render_target_is_swapchain = false,
        .color_attachments = {},
        .depth_image = GBuffer_DepthMS,
        .extent = engine.swapchain.extent,
    };

    engine::DepthPrepassMS depth_prepass_ms = {
        depth_ms_gfx_task,
        { &depth_uniform_desc_set },
        depth_ms_pipeline,
    };

    engine::Pipeline scene_pipeline;

    try {
        size_t frame_index = engine.get_frame_index();
        scene_pipeline = create_gfx_pipeline( engine, ctx.vulkan,
            engine::get_vertex_input_state_create_info( scene_mesh ),
            {
                uniform_desc_set.layouts[frame_index],
                material_desc_sets[0].layouts[frame_index],
                lut_sets.layouts[frame_index],
                sampler_desc_set.layouts[frame_index],
            },
            {
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // POSITION
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // NORMAL
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // TANGENT
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // UV
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, // ALBEDO
                VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT // PACKED DATA (metallic, roughness,
                                                        // clearcoat roughness, clearcoat
                                                        // weight)
            },
            VK_SAMPLE_COUNT_1_BIT, false, true,
            vk::create::shader_module( ctx.vulkan, SHADER_MODULE_PATH ) );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create graphics pipeline: {}", ex.what() );
        throw;
    }

    geometry::quad::Mesh quad_mesh = geometry::quad::create( ctx.vulkan, engine );

    log::info( "[main] pre atmo3!" );
    engine::TaskList task_list;

    // deferred rendering
    engine::RWImage GBuffer_Normal = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_Position = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_Tangent = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_UV = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_Albedo = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_Packed_Data = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

    engine::RWImage GBuffer_Depth = engine::create_rwimage( ctx.vulkan, engine,
        VkExtent3D( engine.swapchain.extent.width, engine.swapchain.extent.height, 1 ),
        VkFormat::VK_FORMAT_D32_SFLOAT, VkImageType::VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT );

#if ENABLE_DEFERRED_AA
    // Render to an offscreen image, this is what we'll present to the swapchain.
    engine::RWImage screen_color = engine::create_rwimage( ctx.vulkan, engine,
        { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT );

    // Buffer needed, this is what the final compute pass will write to.
    // We will later copy the results of this back to `screen_color`, and blit it to the swapchain.
    engine::RWImage screen_buffer = engine::create_rwimage( ctx.vulkan, engine,
        { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT );
#endif

    // deferred transition tasks
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers = {
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Normal,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Position,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Tangent,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_UV,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Albedo,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Packed_Data,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_Depth,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .image = GBuffer_DepthMS,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
            } } );

    // ATMOSPHERE STUFF
    atmosphere::Atmosphere atms = atmosphere::initialize( ctx.vulkan, engine );
    atmosphere::AtmosphereBaker atms_baker = { .atmosphere = &atms };

    {
        geometry::quad::Mesh quad_mesh = geometry::quad::create( ctx.vulkan, engine );

        engine::GfxTask atmosphere_gfx_task = {
            .clear_color = { { { 0.f, 1.f, 0.f, 1.f } } },
            .clear_depth = 1.f,
            .render_target_is_swapchain = false,
            .color_attachments = { screen_color },
            .extent = engine.swapchain.extent,
        };

        engine::Pipeline atmosphere_pipeline;

        try {
            atmosphere_pipeline = engine::create_gfx_pipeline( engine, ctx.vulkan,
                engine::get_vertex_input_state_create_info( quad_mesh ),
                {
                    atms.uniform_desc_set.layouts[0],
                    atms.lut_desc_set.layouts[0],
                    atms.sampler_desc_set.layouts[0],
                },
                {
#if ENABLE_DEFERRED_AA
                    VK_FORMAT_R16G16B16A16_SFLOAT
#else
                    engine.swapchain.image_format
#endif
                },
                VK_SAMPLE_COUNT_1_BIT, false, true,
                vk::create::shader_module( ctx.vulkan, atmosphere::SHADER_PATH ) );
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

        atmosphere::initialize_atmosphere_baker( atms_baker, ctx.vulkan, engine );
        engine::update_descriptor_set_image(
            ctx.vulkan, engine, lut_sets, atms_baker.octahedral_sky, 5 );

        // funny business
        atmosphere::compute_octahedral_sky_irradiance( atms_baker, ctx.vulkan, engine, task_list );

        // FUNNIER business!
        atmosphere::compute_octahedral_sky_mips( atms_baker, ctx.vulkan, engine, task_list );
    }
    // END ATMOSPHERE STUFF

#if ENABLE_VOLUMETRICS
    // Volumetrics stuff
    volumetric::Volumetric volumetric = volumetric::initialize( ctx.vulkan, engine );
    volumetric::draw_volumetric( volumetric, ctx.vulkan, engine, task_list, screen_color );
#endif

    // GBUFFER PRE-PASS
    engine::GfxTask prepass_gfx_task = {
        .clear_color = { { { 0.f, 0.f, 0.f, 0.f } } },
        .clear_depth = 1.f,
        .render_target_is_swapchain = false,
        .color_attachments = { GBuffer_Position, GBuffer_Normal, GBuffer_Tangent, GBuffer_UV,
            GBuffer_Albedo, GBuffer_Packed_Data },
        .depth_image = GBuffer_Depth,
        .extent = engine.swapchain.extent,
    };

    // INITIAL PRECOMPUTE CMDBUFFER
    VkFenceCreateInfo fence_info = vk::create::fence_info( VK_FENCE_CREATE_SIGNALED_BIT );
    VkFence precompute_fence;
    vk::check( vkCreateFence( ctx.vulkan.device, &fence_info, nullptr, &precompute_fence ),
        "Failed to create precompute fence" );
    VkCommandBufferBeginInfo command_buffer_begin_info
        = vk::create::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    vkResetCommandBuffer( engine.frames[0].start_cmdbuf, 0 );
    vkResetFences( ctx.vulkan.device, 1, &precompute_fence );
    vkBeginCommandBuffer( engine.frames[0].start_cmdbuf, &command_buffer_begin_info );

    for ( const std::unique_ptr<scene::Node>& node : scene.nodes ) {
        if ( node->mesh.has_value() ) {
            const std::unique_ptr<scene::Mesh>& mesh = node->mesh.value();

            for ( const scene::Primitive& prim : mesh->primitives ) {
                const scene::Material& current_material
                    = scene.materials[static_cast<size_t>( prim.material_id )];
                std::vector<std::optional<scene::Texture>> textures_needed;

                switch ( current_material.type ) {
                case scene::MaterialType::PBR_ALBEDO_MAP: {
                    std::optional<int> albedo_index = current_material.base_color_texture_index;
                    std::optional<int> normal_index = current_material.normal_texture_index;
                    std::optional<int> metallic_roughness_index
                        = current_material.metallic_roughness_texture_index;

                    textures_needed.push_back( albedo_index
                            ? std::optional { (
                                  scene.textures[static_cast<size_t>( albedo_index.value() )] ) }
                            : std::nullopt );
                    textures_needed.push_back( normal_index
                            ? std::optional { (
                                  scene.textures[static_cast<size_t>( normal_index.value() )] ) }
                            : std::nullopt );
                    textures_needed.push_back( metallic_roughness_index
                            ? std::optional { ( scene.textures[static_cast<size_t>(
                                  metallic_roughness_index.value() )] ) }
                            : std::nullopt );

                    break;
                }

                default:
                    throw Exception( "[main_gfx_task] Unhandled material type" );
                }

                // TODO: Not yet connected with a layout
                if ( scene.hdri_index.has_value() ) {
                    textures_needed.push_back( scene.textures[scene.hdri_index.value()] );
                } else {
                    textures_needed.push_back( std::nullopt );
                }

                std::vector<vk::mem::AllocatedImage> textures_sent;

                for ( std::optional<scene::Texture>& texture : textures_needed ) {
                    if ( texture && ( textures_sent.size() < vk::binding::MAX_IMAGES_BINDED ) ) {
                        textures_sent.push_back( texture->data.value() );
                    }
                }

                // Actually bind the texture handle to the descriptorset
                for ( size_t i = 0; i < textures_sent.size(); i++ ) {
                    engine::update_descriptor_set_image( ctx.vulkan, engine,
                        material_desc_sets[static_cast<size_t>( prim.material_id )],
                        textures_sent[i], static_cast<int>( i ) );
                }

                engine::DrawResourceDescriptor draw_descriptor
                    = engine::DrawResourceDescriptor::from_mesh(
                        scene_mesh.mesh_buffers.vertex_buffer.handle,
                        scene_mesh.mesh_buffers.index_buffer.handle,
                        static_cast<uint32_t>( scene_mesh.indices.size() ), prim );

                // give the material descriptor set to the draw task
                prepass_gfx_task.draw_tasks.push_back( {
                        .draw_resource_descriptor = draw_descriptor,
                        .descriptor_sets = {
                            &uniform_desc_set,
                            &material_desc_sets[static_cast<size_t>( prim.material_id )],
                            &lut_sets,
                            &sampler_desc_set,
                        },
                        .pipeline = scene_pipeline,
                    } );
                depth_ms_gfx_task.draw_tasks.push_back( {
                    .draw_resource_descriptor = draw_descriptor,
                    .descriptor_sets = { &depth_uniform_desc_set },
                    .pipeline = depth_ms_pipeline,
                } );

                uint32_t max_idx = 0;
                for ( size_t x = 0; x < prim.ind_count; x++ ) {
                    size_t offset_x = x + size_t( prim.ind_offset );
                    uint32_t idx = scene_mesh.indices[offset_x];
                    max_idx = glm::max( max_idx, idx );
                }

                engine.blas.push_back( vk::rt::build_blas( ctx.vulkan.device, ctx.vulkan.allocator,
                    ctx.vulkan.ray_tracing_properties,
                    { .vertex_buffer = scene_mesh.mesh_buffers.vertex_buffer.handle,
                        .index_buffer = scene_mesh.mesh_buffers.index_buffer.handle,
                        .max_vertex = uint32_t( max_idx ) - 1,
                        .index_count = uint32_t( draw_descriptor.index_count ),
                        .vertex_offset = uint32_t( draw_descriptor.vertex_offset ),
                        .index_offset = uint32_t( draw_descriptor.index_offset ) },
                    engine.frames[0].start_cmdbuf, ctx.vulkan.destructor_stack ) );
            }
        }
    }

    std::vector<vk::rt::Object> objects;
    for ( auto& blas : engine.blas ) {
        objects.push_back(
            vk::rt::Object { .blas = &blas, .transform = glm::identity<glm::mat4>() } );
    }

    engine.tlas = vk::rt::build_tlas( ctx.vulkan.device, ctx.vulkan.allocator,
        ctx.vulkan.ray_tracing_properties, objects, engine.frames[0].start_cmdbuf,
        ctx.vulkan.destructor_stack );

    vkEndCommandBuffer( engine.frames[0].start_cmdbuf );
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &engine.frames[0].start_cmdbuf;
    vkQueueSubmit( ctx.vulkan.graphics_queue, 1, &submit_info, precompute_fence );
    vkWaitForFences( ctx.vulkan.device, 1, &precompute_fence, VK_TRUE, UINT64_MAX );
    vkResetFences( ctx.vulkan.device, 1, &precompute_fence );
    vkDestroyFence( ctx.vulkan.device, precompute_fence, VK_NULL_HANDLE );
    vkResetCommandBuffer( engine.frames[0].start_cmdbuf, 0 );

    engine::DescriptorSet as_desc_set = engine::generate_descriptor_set( ctx.vulkan, engine,
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR },
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_COMPUTE_BIT );

    engine::add_gfx_task( task_list, prepass_gfx_task );

#if ENABLE_TERRAIN
    // TODO: INSERT TERRAIN PRE-PASS DRAW HERE
    geometry::Terrain test_terrain;
    geometry::initialize_terrain( ctx.vulkan, engine, test_terrain );
    geometry::draw_terrain_prepass( test_terrain, ctx.vulkan, engine, GBuffer_Position,
        GBuffer_Normal, GBuffer_Depth, camera_buffer, depth_prepass_ms, task_list );
    test_terrain.accel_structure_desc_set = &as_desc_set;
#endif

    engine::add_gfx_task( task_list, depth_ms_gfx_task );

    // Lighting pass
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Position,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Normal,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Tangent,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_UV,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Albedo,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Packed_Data,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    .image = GBuffer_Depth,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH },
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                    .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                    .image = GBuffer_DepthMS,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_DEPTH } } } );

#if ENABLE_TERRAIN
    geometry::TerrainLightingInfo terrain_lighting_info = {
        &camera_buffer,
        &debug_buffer,
        &atms_baker,
        &GBuffer_Position,
        &GBuffer_Normal,
        &screen_color,
    };

    geometry::draw_terrain( test_terrain, ctx.vulkan, engine, task_list, terrain_lighting_info );

    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers
            = { engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .src_access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = screen_color,
                .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR } } } );
#endif

    // lighting pass
    geometry::quad::Mesh lighting_pass_quad_mesh = geometry::quad::create( ctx.vulkan, engine );

    engine::GfxTask lighting_pass_gfx_task = {
        .clear_depth = 1.0f,
        .render_target_is_swapchain = true,
        .extent = engine.swapchain.extent,
    };

#if ENABLE_DEFERRED_AA
    lighting_pass_gfx_task.render_target_is_swapchain = false;
    lighting_pass_gfx_task.color_attachments = { screen_color };
#endif

    engine::DescriptorSet gbuffer_descriptor_set
        = engine::generate_descriptor_set( ctx.vulkan, engine,
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            },
            VK_SHADER_STAGE_FRAGMENT_BIT );

    size_t frame_index = engine.get_frame_index();
    engine::Pipeline lighting_pass_gfx_pipeline;
    try {
        lighting_pass_gfx_pipeline = engine::create_gfx_pipeline( engine, ctx.vulkan,
            engine::get_vertex_input_state_create_info( lighting_pass_quad_mesh ),
            { uniform_desc_set.layouts[frame_index], material_desc_sets[0].layouts[frame_index],
                lut_sets.layouts[frame_index], sampler_desc_set.layouts[frame_index],
                gbuffer_descriptor_set.layouts[frame_index], as_desc_set.layouts[frame_index] },
            {
#if ENABLE_DEFERRED_AA
                VK_FORMAT_R16G16B16A16_SFLOAT
#else
                engine.swapchain.image_format
#endif
            },
            VK_SAMPLE_COUNT_1_BIT, true, false,
            vk::create::shader_module( ctx.vulkan, LIGHTING_PASS_SHADER_MODULE_PATH ) );
    } catch ( const Exception& ex ) {
        log::error( "Failed to create lighting pass graphics pipeline: {}", ex.what() );
        throw;
    }

    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set,
        GBuffer_Position, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0 );
    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set,
        GBuffer_Normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set,
        GBuffer_Tangent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2 );
    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set, GBuffer_UV,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3 );
    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set,
        GBuffer_Albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4 );
    engine::update_descriptor_set_depth_image(
        ctx.vulkan, engine, gbuffer_descriptor_set, GBuffer_Depth, 5 );
    engine::update_descriptor_set_depth_image(
        ctx.vulkan, engine, gbuffer_descriptor_set, GBuffer_DepthMS, 6 );
    engine::update_descriptor_set_rwimage( ctx.vulkan, engine, gbuffer_descriptor_set,
        GBuffer_Packed_Data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 7 );

    engine::update_descriptor_set_acceleration_structure(
        ctx.vulkan, engine, as_desc_set, engine.tlas.handle, 0 );

    lighting_pass_gfx_task.draw_tasks.push_back({
            .draw_resource_descriptor = {
                .vertex_buffers = { lighting_pass_quad_mesh.mesh_buffers.vertex_buffer.handle },
                .index_buffer = lighting_pass_quad_mesh.mesh_buffers.index_buffer.handle,
                .vertex_buffer_offsets = { 0 },
                .index_count = static_cast<uint32_t>( lighting_pass_quad_mesh.indices.size() ),
            },
            .descriptor_sets = {
                &uniform_desc_set,
                &material_desc_sets[static_cast<size_t>( 0 )], // THIS IS WRONG; NEEDS FIX
                &lut_sets,
                &sampler_desc_set,
                &gbuffer_descriptor_set,
                &as_desc_set
            },
            .pipeline = lighting_pass_gfx_pipeline,
        });

    engine::add_gfx_task( task_list, lighting_pass_gfx_task );

// TODO: Make this its own function
#if ENABLE_DEFERRED_AA
    // Convert the screen_color/rendered image to read-only.
    // Output screen buffer must be converted to write-only.
    // These are special pipeline barriers, since it is assumed that
    // the scene color was rendered to via gfx draw calls, hence the attachment.
    // We may need future helpers to convert from attachment to cs write, etc. and vice versa.
    //
    // Current limitation assumes that all post-processing calls are done via compute shader
    // hence the VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT stage.
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers = {
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    .image = screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
                engine::ImageBarrier {
                    .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .src_access = VK_ACCESS_2_NONE,
                    .src_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .dst_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dst_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .image = screen_buffer,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR,
                },
            } } );

    engine::post::BloomPass bloom_pass = engine::post::add_bloom(
        ctx.vulkan, engine, task_list, screen_color, screen_buffer, debug_buffer );

    engine::post::AoPass ao_pass = {
        &camera_buffer,
        &GBuffer_Normal,
        &GBuffer_Depth,
        &screen_buffer,
        &screen_color,
    };
    {
        engine::transition_cs_read_to_write( task_list, screen_color );
        engine::transition_cs_write_to_read( task_list, screen_buffer );

        add_ao( ao_pass, ctx.vulkan, engine, task_list );
    }

    // This is the final pipeline barrier necessary for transitioning the chosen out_color to the
    // screen.
    engine::add_pipeline_barrier( task_list,
        engine::PipelineBarrierDescriptor { .buffer_barriers = {},
            .image_barriers = {
                engine::ImageBarrier { .src_stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .src_access = VK_ACCESS_2_SHADER_WRITE_BIT,
                    .src_layout = VK_IMAGE_LAYOUT_GENERAL,
                    .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dst_access = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .dst_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = screen_color,
                    .range = engine::VK_IMAGE_SUBRESOURCE_RANGE_DEFAULT_COLOR },
            } } );

    engine::add_blit_task( task_list, { screen_color } );
#endif

    // TERRIBLY EVIL HACK. THIS IS BAD. DON'T BE DOING THIS GANG.
    task_list.junk_tasks.push_back(
        [&atms_baker]( engine::State&, Context&, engine::FrameData& frame ) {
            atmosphere::bake_octahedral_sky_task( atms_baker, frame.render_cmdbuf );
        } );

    bool will_quit = false;
    bool stop_drawing = false;
    SDL_Event event = {};

    while ( !will_quit ) {
        while ( SDL_PollEvent( &event ) ) {
            gui::process_event( &event );
            camera::process_event( ctx, &event, engine.camera );

            if ( event.type == SDL_EVENT_QUIT ) {
                will_quit = true;
            }

            if ( event.type == SDL_EVENT_WINDOW_MINIMIZED ) {
                stop_drawing = true;
            } else if ( event.type == SDL_EVENT_WINDOW_RESTORED ) {
                stop_drawing = false;
            }
        }

        camera::process_input( engine.camera );

        // Don't draw if we're minimized
        if ( stop_drawing ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        camera::OrbitCamera& camera = engine.camera;
        glm::mat4 view = camera::calculate_view_matrix( camera );
        glm::mat4 projection = glm::perspective(
            camera.fov_y, camera.aspect_ratio, camera.near_plane, camera.far_plane );

        // This cursed piece of code flips the positive y-axis down because Vulkan's clip space +y
        // points down (whereas in OpenGL/WebGPU it points up).
        projection[1][1] *= -1;

        glm::vec3 camera_position = camera::calculate_eye_position( camera );

        // Update atmosphere uniform buffer
        {
            if ( gui.atms.animate_zenith ) {
                float t
                    = ( std::sin( 0.001f * static_cast<float>( engine.rendered_frames ) ) + 1.f )
                    * 0.5f;
                atms.sun_zenith = glm::lerp( 0.f, glm::pi<float>(), t );
            }

            glm::vec3 atmosphere_position = {
                camera_position.x,
                // A y-value of 9 means the camera is 9 km above the surface. This is pretty
                // ridiculous so we manually adjust it here. Now y needs to be 900.
                camera_position.y * 0.01f,
                camera_position.z,
            };

            ub_data::Atmosphere atms_ub = atms.uniform_buffer.get_data();
            atms_ub.inverse_proj = glm::inverse( projection );
            atms_ub.inverse_view = glm::inverse( view );
            atms_ub.camera_position = atmosphere_position;
            atms_ub.sun_direction = atmosphere::compute_sun_direction( atms );
            atms_ub.radiance_exposure = gui.atms.radiance_exposure;

            atms.uniform_buffer.set_data( atms_ub );
            atms.uniform_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

        // Update camera uniform buffer
        {
            ub_data::Camera camera_ub = camera_buffer.get_data();

            // glm::mat4 model = gui.demo.rotate_on
            // ? glm::rotate( camera_ub.model, gui.demo.rotate_speed, glm::vec3( 0, 1, 0 ) )
            // : camera_ub.model;
            glm::mat4 model = glm::identity<glm::mat4>();

            camera_ub.mvp = projection * view * model;
            camera_ub.model = model;
            camera_ub.view_mat = view;
            camera_ub.inv_model = glm::inverse( model );
            camera_ub.camera_pos = glm::vec4( camera_position, 1.0f );
            camera_ub.camera_constants = glm::vec4(
                camera.near_plane, camera.far_plane, camera.aspect_ratio, camera.fov_y );

            camera_buffer.set_data( camera_ub );
            camera_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

        // AO update
        {
            ub_data::AOData ao_ub = ao_pass.ao_buffer.get_data();

            ao_ub.packed_floats0 = glm::vec4(
                gui.ao.thickness, gui.ao.radius, gui.ao.offset, gui.ao.enable_debug ? 1.0f : 0.0f );
            ao_ub.packed_floats1 = glm::vec4( gui.ao.enable_ao ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f );

            ao_pass.ao_buffer.set_data( ao_ub );
            ao_pass.ao_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

#if ENABLE_VOLUMETRICS
        // Update volumetric camera buffer
        {
            volumetric.uniform_buffer.set_data( camera_buffer.get_data() );
            volumetric.uniform_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }
#endif

        // Update debug uniform buffer
        {
            ub_data::Atmosphere atms_ub = atms.uniform_buffer.get_data();

            ub_data::Debug debug_ub = { .color = gui.debug.color,
                .packed_data0 = glm::vec4( gui.debug.roughness, gui.debug.metallic,
                    gui.debug.clearcoat_roughness, gui.debug.clearcoat_weight ),
                .sun_direction = glm::vec4( atms_ub.sun_direction, 1.0f ),

                .enable_albedo_map = gui.debug.enable_albedo_map,
                .enable_normal_map = gui.debug.enable_normal_map,
                .enable_roughness_metal_map = gui.debug.enable_roughness_metal_map,
                .normals_only = gui.debug.normals_only,
                .albedo_only = gui.debug.albedo_only,
                .roughness_metal_only = gui.debug.roughness_metal_only,

                .enable_bloom = gui.debug.enable_bloom,
                .ray_traced_shadows = gui.debug.ray_traced_shadows };

            debug_buffer.set_data( debug_ub );
            debug_buffer.update( ctx.vulkan, engine.get_frame_index() );
        }

        // update materials 
        {
            gui.debug.current_editing_material = glm::clamp(gui.debug.current_editing_material, 0, int(num_materials));
            int mat_idx = gui.debug.current_editing_material;
            auto mat_data = material_uniform_buffers[size_t(mat_idx)].get_data();
            if ( gui.debug.load_material_into_gui ) {
                gui.debug.color = mat_data.base_color;
                gui.debug.roughness = mat_data.roughness;
                gui.debug.metallic = mat_data.metallic;
                gui.debug.clearcoat_weight = mat_data.clearcoat;
                gui.debug.clearcoat_roughness = mat_data.clearcoat_roughness;
                gui.debug.load_material_into_gui = false;
            }
            mat_data.base_color = gui.debug.color;
            mat_data.roughness = gui.debug.roughness;
            mat_data.metallic = gui.debug.metallic;
            mat_data.clearcoat = gui.debug.clearcoat_weight;
            mat_data.clearcoat_roughness = gui.debug.clearcoat_roughness;
            material_uniform_buffers[size_t(mat_idx)].set_data(mat_data);
            material_uniform_buffers[size_t(mat_idx)].update(ctx.vulkan, engine.get_frame_index());
        }

        // {
        //     ub_data::RaymarchBufferData raymarch_ub = {
        //         .step_size = 1,
        //     };

        //     raymarch_buffer.set_data( raymarch_ub );
        //     raymarch_buffer.update( ctx.vulkan, engine.get_frame_index() );
        // }

        gui::update( gui, camera, atms );

        engine::execute( engine, ctx, task_list );
        engine.rendered_frames = engine.rendered_frames + 1;
        engine.frame_number = ( engine.rendered_frames + 1 ) % engine.frame_overlap;

        // Make new screen visible
        SDL_UpdateWindowSurface( ctx.window );
    }

    vkDeviceWaitIdle( ctx.vulkan.device );
    gui::free();
    engine::free( engine );
    ctx.vulkan.destructor_stack.execute_cleanup();
    vk::free( ctx.vulkan );
    sdl::free( ctx.window );
}

}

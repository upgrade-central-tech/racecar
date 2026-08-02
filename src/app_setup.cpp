#include "app_setup.hpp"

#include "constants.hpp"
#include "demo/car_assets.hpp"
#include "engine/descriptors.hpp"
#include "engine/images.hpp"
#include "geometry/ibl.hpp"
#include "geometry/procedural.hpp"
#include "sdl.hpp"
#include "vk/create.hpp"

namespace racecar {

namespace {

constexpr size_t NUM_MATERIAL_TEXTURE_BINDINGS = 3;

vk::mem::AllocatedImage create_fallback_texture( Context& ctx, engine::State& engine )
{
    uint32_t white = 0xFFFFFFFF;

    return engine::create_image(
        ctx.vulkan,
        engine,
        &white,
        VkExtent3D( 1, 1, 1 ),
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false
    );
}

}

Context initialize_context( bool use_fullscreen )
{
    Context ctx;
    ctx.window = sdl::initialize( constant::SCREEN_W, constant::SCREEN_H, use_fullscreen ),
    ctx.vulkan = vk::initialize( ctx.window );
    return ctx;
}

void load_scene(
    Context& ctx, engine::State& engine, scene::Scene* scene, geometry::scene::Mesh* scene_mesh
)
{
    scene::load_gltf(
        ctx.vulkan,
        engine,
        GLTF_FILE_PATH,
        *scene,
        scene_mesh->vertices,
        scene_mesh->indices
    );
    geometry::scene::generate_tangents( *scene_mesh );
    scene_mesh->mesh_buffers = geometry::scene::upload_mesh(
        ctx.vulkan,
        engine,
        scene_mesh->indices,
        scene_mesh->vertices
    );
}

void load_camera_debug_uniform_buffers(
    Context& ctx,
    engine::State& engine,
    UniformBuffer<ub_data::Camera>* camera_buffer,
    UniformBuffer<ub_data::Debug>* debug_buffer,
    engine::DescriptorSet* uniform_desc_set
)
{
    *camera_buffer = create_uniform_buffer<ub_data::Camera>(
        ctx.vulkan,
        { },
        static_cast<size_t>( engine.frame_overlap )
    );
    *debug_buffer = create_uniform_buffer<ub_data::Debug>(
        ctx.vulkan,
        { },
        static_cast<size_t>( engine.frame_overlap )
    );
    // UniformBuffer raymarch_buffer = create_uniform_buffer<ub_data::RaymarchBufferData>(
    //     ctx.vulkan, {}, static_cast<size_t>( engine.frame_overlap ) );

    *uniform_desc_set = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    );

    engine::update_descriptor_set_uniform(
        ctx.vulkan,
        engine,
        *uniform_desc_set,
        *camera_buffer,
        0
    );
    engine::update_descriptor_set_uniform(
        ctx.vulkan,
        engine,
        *uniform_desc_set,
        *debug_buffer,
        1
    );
}

void load_samplers(
    Context& ctx,
    engine::State& engine,
    VkSampler* linear_sampler,
    VkSampler* point_sampler,
    engine::DescriptorSet* sampler_desc_set
)
{
    // Simple set up for linear sampler
    VkSamplerCreateInfo sampler_linear_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 100,
    };

    vk::check(
        vkCreateSampler( ctx.vulkan.device, &sampler_linear_create_info, nullptr, linear_sampler ),
        "Failed to create sampler"
    );
    ctx.vulkan.destructor_stack.push( ctx.vulkan.device, *linear_sampler, vkDestroySampler );

    VkSamplerCreateInfo sampler_nearest_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 100,
    };

    vk::check(
        vkCreateSampler( ctx.vulkan.device, &sampler_nearest_create_info, nullptr, point_sampler ),
        "Failed to create nearest smapler!"
    );
    ctx.vulkan.destructor_stack.push( ctx.vulkan.device, *point_sampler, vkDestroySampler );

    *sampler_desc_set = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    engine::update_descriptor_set_sampler(
        ctx.vulkan,
        engine,
        *sampler_desc_set,
        *linear_sampler,
        0
    );

    engine::update_descriptor_set_sampler(
        ctx.vulkan,
        engine,
        *sampler_desc_set,
        *point_sampler,
        1
    );
}

void load_materials(
    Context& ctx,
    engine::State& engine,
    scene::Scene& scene,
    size_t num_materials,
    std::vector<engine::DescriptorSet>* material_desc_sets,
    std::vector<UniformBuffer<ub_data::Material>>* material_uniform_buffers
)
{
    for ( size_t i = 0; i < num_materials; i++ ) {
        // Generate a separate texture descriptor set for each of the materials
        ( *material_desc_sets )[i] = engine::generate_descriptor_set(
            ctx.vulkan,
            engine,
            {
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        );

        scene::Material& mat = scene.materials[i];

        UniformBuffer material_buffer = create_uniform_buffer<ub_data::Material>(
            ctx.vulkan,
            { },
            static_cast<size_t>( engine.frame_overlap )
        );
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
            ctx.vulkan,
            engine,
            ( *material_desc_sets )[i],
            material_buffer,
            3
        );
        ( *material_uniform_buffers )[i] = std::move( material_buffer );
    }
}

void load_model_mat_uniform_buffers(
    Context& ctx,
    engine::State& engine,
    size_t num_nodes,
    scene::Scene* scene,
    std::vector<engine::DescriptorSet>* model_mat_desc_sets,
    std::vector<UniformBuffer<ub_data::ModelMat>>* model_mat_uniform_buffers
)
{
    for ( size_t i = 0; i < num_nodes; i++ ) {
        ( *model_mat_desc_sets )[i] = engine::generate_descriptor_set(
            ctx.vulkan,
            engine,
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            VK_SHADER_STAGE_VERTEX_BIT
        );

        scene::Node* node = scene->nodes[i].get();
        glm::mat4 transform = node->transform;
        while ( node->parent != nullptr ) {
            node = node->parent;
            transform = node->transform * transform;
        }

        UniformBuffer model_mat_buffer = create_uniform_buffer<ub_data::ModelMat>(
            ctx.vulkan,
            { },
            static_cast<size_t>( engine.frame_overlap )
        );

        ub_data::ModelMat model_mat_ub = { .model_mat = transform,
                                           .inv_model_mat = glm::inverse( transform ),
                                           .prev_model_mat = transform };
        model_mat_buffer.set_data( model_mat_ub );
        model_mat_buffer.update( ctx.vulkan, engine.get_frame_index() );

        engine::update_descriptor_set_uniform(
            ctx.vulkan,
            engine,
            ( *model_mat_desc_sets )[i],
            model_mat_buffer,
            0
        );
        ( *model_mat_uniform_buffers )[i] = std::move( model_mat_buffer );
    }
}

void create_raymarch_tex_sets(
    Context& ctx, engine::State& engine, engine::DescriptorSet* raymarch_tex_sets
)
{
    *raymarch_tex_sets = engine::generate_descriptor_set(
        ctx.vulkan,
        engine,
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    vk::mem::AllocatedImage test_data_3D = geometry::generate_test_3D( ctx.vulkan, engine );
    engine::update_descriptor_set_image( ctx.vulkan, engine, *raymarch_tex_sets, test_data_3D, 0 );
}

void create_screen_buffers(
    Context& ctx,
    engine::State& engine,
    engine::RWImage* screen_color,
    engine::RWImage* screen_buffer,
    engine::RWImage* screen_history
)
{
    *screen_color = engine::create_rwimage(
        ctx.vulkan,
        engine,
        { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );

    // Buffer needed, this is what the final compute pass will write to.
    // We will later copy the results of this back to `screen_color`, and blit it to the swapchain.
    *screen_buffer = engine::create_rwimage(
        ctx.vulkan,
        engine,
        { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );

    // Stores the last-rendered image. Needed for TAA/motion-vectors
    *screen_history = engine::create_rwimage(
        ctx.vulkan,
        engine,
        { engine.swapchain.extent.width, engine.swapchain.extent.height, 1 },
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TYPE_2D,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
    );
}

void load_model_primitive_material_data(
    Context& ctx,
    engine::State& engine,
    const scene::Scene& scene,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    std::vector<glm::mat4>& transforms,
    std::vector<const scene::Primitive*>& prims,
    ub_data::RTTextureUniform& rt_texture_uniform,
    std::vector<vk::mem::AllocatedImage>& albedo_textures,
    std::vector<vk::mem::AllocatedImage>& metallic_roughness_textures,
    std::vector<engine::DescriptorSet>& material_desc_sets
)
{
    const vk::mem::AllocatedImage fallback_texture = create_fallback_texture( ctx, engine );

    int tex_count = 0;
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

                    textures_needed.push_back(
                        albedo_index
                            ? std::optional { (
                                  scene.textures[static_cast<size_t>( albedo_index.value() )]
                              ) }
                            : std::nullopt
                    );
                    textures_needed.push_back(
                        normal_index
                            ? std::optional { (
                                  scene.textures[static_cast<size_t>( normal_index.value() )]
                              ) }
                            : std::nullopt
                    );
                    textures_needed.push_back(
                        metallic_roughness_index
                            ? std::optional { ( scene.textures[static_cast<
                                  size_t>( metallic_roughness_index.value() )] ) }
                            : std::nullopt
                    );

                    if ( albedo_index ) {
                        albedo_textures.push_back(
                            scene.textures[static_cast<size_t>( albedo_index.value() )].data.value()
                        );
                        rt_texture_uniform.albedo_texture_index[tex_count]
                            = int( albedo_textures.size() - 1 );
                    } else {
                        rt_texture_uniform.albedo_texture_index[tex_count] = -1;
                        rt_texture_uniform.base_color[tex_count]
                            = glm::vec4( current_material.base_color, 1.0 );
                    }

                    if ( metallic_roughness_index ) {
                        metallic_roughness_textures.push_back(
                            scene.textures[static_cast<size_t>( metallic_roughness_index.value() )]
                                .data.value()
                        );
                        rt_texture_uniform.metallic_roughness_texture_index[tex_count]
                            = int( metallic_roughness_textures.size() - 1 );
                    } else {
                        rt_texture_uniform.metallic_roughness_texture_index[tex_count] = -1;
                        rt_texture_uniform.metallic[tex_count] = current_material.metallic;
                        rt_texture_uniform.roughness[tex_count] = current_material.roughness;
                    }

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

                for ( size_t i = 0; i < NUM_MATERIAL_TEXTURE_BINDINGS; i++ ) {
                    const bool has_texture = i < textures_needed.size() && textures_needed[i];

                    engine::update_descriptor_set_image(
                        ctx.vulkan,
                        engine,
                        material_desc_sets[static_cast<size_t>( prim.material_id )],
                        has_texture ? textures_needed[i]->data.value() : fallback_texture,
                        static_cast<int>( i )
                    );
                }

                transforms.push_back(
                    model_mat_uniform_buffers[static_cast<size_t>( prim.node_id )]
                        .get_data()
                        .model_mat
                );

                prims.push_back( &prim );
                tex_count++;
            }
        }
    }
}

}

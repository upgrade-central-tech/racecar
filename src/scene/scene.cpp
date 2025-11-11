#include "scene.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL  // Needed for quaternion.hpp
#include <glm/gtx/quaternion.hpp>

#include <filesystem>
#include <stb_image.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf.h>

namespace racecar::scene {

static inline glm::vec3 double_array_to_vec3( std::vector<double> arr ) {
    return glm::vec3( arr[0], arr[1], arr[2] );
}

// These are all the image formats currently supported. If more formats are required, also add to
// vk::utility::bytes_from_format
VkFormat get_vk_format( int bitsPerChannel, int numChannels ) {
    if ( bitsPerChannel == 32 ) {
        // Assume floating point formats for 32 bits per channel
        switch ( numChannels ) {
            case 4:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            default:
                SDL_Log( "[Scene] Texture loading: Unsupported 32-bit channel count: %i",
                         numChannels );
        }
    } else if ( bitsPerChannel == 8 ) {
        switch ( numChannels ) {
            case 1:
                return VK_FORMAT_R8_UNORM;
            case 3:
                return VK_FORMAT_R8G8B8_UNORM;
            case 4:
                return VK_FORMAT_R8G8B8A8_UNORM;
            default:
                SDL_Log( "[Scene] Texture loading: Unsupported 8-bit channel count: %i",
                         numChannels );
        }
    }
    SDL_Log( "[Scene] Texture loading: Unknown texture format" );
    return VK_FORMAT_R8G8B8A8_UNORM;
}

bool load_gltf( vk::Common vulkan,
                engine::State& engine,
                std::string filepath,
                Scene& scene,
                std::vector<geometry::Vertex>& out_global_vertices,
                std::vector<uint32_t>& out_global_indices ) {
    std::filesystem::path path( filepath );

    if ( !std::filesystem::exists( path ) ) {
        SDL_Log( "[Scene] File does not exist" );
        return false;
    }

    std::string ext = path.extension().string();
    if ( ext != ".gltf" && ext != ".glb" ) {
        SDL_Log( "[Scene] Invalid file extension loaded" );
        return false;
    }

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool has_loaded_successfully = false;
    // Binary files
    if ( ext == ".glb" ) {
        has_loaded_successfully = loader.LoadBinaryFromFile( &model, &err, &warn, filepath );
    } else {  // ASCII files
        has_loaded_successfully = loader.LoadASCIIFromFile( &model, &err, &warn, filepath );
    }

    // Check for errors and warnings
    if ( !warn.empty() ) {
        SDL_Log( "[Scene] GLTF load WARNING: %s", warn.c_str() );
    }

    if ( !err.empty() ) {
        SDL_Log( "[Scene] GLTF load ERROR: %s!", err.c_str() );
    }

    if ( !has_loaded_successfully ) {
        SDL_Log( "[Scene] GLTF parsing error!" );
    }

    // TODO @terskayl: Add Material Loading
    // Materials
    for ( tinygltf::Material& loaded_mat : model.materials ) {
        Material new_mat;
        new_mat.base_color =
            double_array_to_vec3( loaded_mat.pbrMetallicRoughness.baseColorFactor );
        new_mat.base_color_texture_index = loaded_mat.pbrMetallicRoughness.baseColorTexture.index;
        if ( new_mat.base_color_texture_index.value() == -1 ) {
            new_mat.base_color_texture_index = std::nullopt;
        }
        SDL_Log( "[Scene] GLTF parsing error! %i",
                 loaded_mat.pbrMetallicRoughness.baseColorTexture.index );

        new_mat.metallic = static_cast<float>(loaded_mat.pbrMetallicRoughness.metallicFactor);
        new_mat.roughness = static_cast<float>(loaded_mat.pbrMetallicRoughness.roughnessFactor);
        new_mat.metallic_roughness_texture_index =
            loaded_mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if ( new_mat.metallic_roughness_texture_index.value() == -1 ) {
            new_mat.metallic_roughness_texture_index = std::nullopt;
        }

        if ( loaded_mat.extensions.count( "KHR_materials_specular" ) != 0 ) {
            auto specular = loaded_mat.extensions.find( "KHR_materials_specular" )->second;
            new_mat.specular = static_cast<float>(specular.Get( "specularFactor" ).GetNumberAsDouble());
            // newMat.specularTint = specular.Get("specularColorFactor")
        }
        if ( loaded_mat.extensions.count( "KHR_materials_ior" ) != 0 ) {
            new_mat.ior = static_cast<float>(loaded_mat.extensions.find( "KHR_materials_ior" )
                              ->second.Get( "ior" )
                              .GetNumberAsDouble());
        }

        if ( loaded_mat.extensions.count( "KHR_materials_clearcoat" ) ) {
            new_mat.clearcoat = static_cast<float>(loaded_mat.extensions.find( "KHR_materials_clearcoat" )
                                    ->second.Get( "clearcoatFactor" )
                                    .GetNumberAsDouble());
            new_mat.clearcoat_roughness = static_cast<float>(loaded_mat.extensions.find( "KHR_materials_clearcoat" )
                                              ->second.Get( "clearcoatRoughnessFactor" )
                                              .GetNumberAsDouble());
        }

        if ( loaded_mat.extensions.count( "KHR_materials_sheen" ) ) {
            auto sheen = loaded_mat.extensions.find( "KHR_materials_sheen" )->second;
            new_mat.sheen_weight = 1.f;

            const tinygltf::Value& color_factor = sheen.Get( "sheenColorFactor" );

            if ( color_factor.IsArray() && color_factor.Size() == 3 ) {
                new_mat.sheen_tint = glm::vec3( color_factor.Get( 0 ).GetNumberAsDouble(),
                                                color_factor.Get( 1 ).GetNumberAsDouble(),
                                                color_factor.Get( 2 ).GetNumberAsDouble() );
            }
            new_mat.sheen_roughness = static_cast<float>(sheen.Get( "sheenRoughnessFactor" ).GetNumberAsDouble());
        }

        if ( loaded_mat.extensions.count( "KHR_materials_transmission" ) ) {
            new_mat.transmission = static_cast<float>(loaded_mat.extensions.find( "KHR_materials_transmission" )
                                       ->second.Get( "transmissionFactor" )
                                       .GetNumberAsDouble());
        }

        new_mat.emissive = double_array_to_vec3( loaded_mat.emissiveFactor );
        if ( loaded_mat.extensions.count( "KHR_materials_emissive_strength" ) ) {
            new_mat.emissive *= loaded_mat.extensions.find( "KHR_materials_emissive_strength" )
                                    ->second.Get( "emissiveStrength" )
                                    .GetNumberAsDouble();
        }
        new_mat.emmisive_texture_index = loaded_mat.emissiveTexture.index;
        if ( new_mat.emmisive_texture_index.value() == -1 ) {
            new_mat.emmisive_texture_index = std::nullopt;
        }

        new_mat.normal_texture_index = loaded_mat.normalTexture.index;
        if ( new_mat.normal_texture_index.value() == -1 ) {
            new_mat.normal_texture_index = std::nullopt;
        }
        new_mat.normal_texture_weight = static_cast<int>(loaded_mat.normalTexture.scale);
        new_mat.occulusion_texture_index = loaded_mat.occlusionTexture.index;
        if ( new_mat.occulusion_texture_index.value() == -1 ) {
            new_mat.occulusion_texture_index = std::nullopt;
        }

        new_mat.double_sided = loaded_mat.doubleSided;
        new_mat.unlit = false;

        scene.materials.push_back( new_mat );
    }

    // Textures & Load onto GPU
    for ( tinygltf::Texture& loaded_tex : model.textures ) {
        Texture new_tex;
        tinygltf::Image loaded_img = model.images[loaded_tex.source];

        new_tex.width = loaded_img.width;
        new_tex.height = loaded_img.height;
        new_tex.bitsPerChannel = loaded_img.bits;
        new_tex.numChannels = loaded_img.component;

        VkFormat image_format = get_vk_format( new_tex.bitsPerChannel, new_tex.numChannels );

        new_tex.data =
            engine::create_image( vulkan, engine, static_cast<void*>( loaded_img.image.data() ),
                                 { static_cast<uint32_t>( loaded_img.width ),
                                   static_cast<uint32_t>( loaded_img.height ), 1 },
                                 image_format, VK_IMAGE_USAGE_SAMPLED_BIT, false );
        if ( !new_tex.data ) {
            SDL_Log( "[Scene] GLTF loading: Failed to load texture onto the GPU" );
        }

        scene.textures.push_back( new_tex );
    }

    // Used for pairing children and parents in the scene graph
    std::vector<std::vector<int>> children_lists;
    // Load Nodes
    for ( tinygltf::Node& loaded_node : model.nodes ) {
        std::unique_ptr<Node> new_node = std::make_unique<Node>();

        // Get node transform
        if ( loaded_node.matrix.size() ) {
            std::vector<double> mat = loaded_node.matrix;
            new_node->transform =
                glm::mat4( mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7], mat[8],
                           mat[9], mat[10], mat[11], mat[12], mat[13], mat[14], mat[15] );
        } else {
            // Attempt to recover from individual transforms

            glm::vec3 translation = glm::vec3( 0.f );
            glm::quat rotation = glm::quat();
            glm::vec3 scale = glm::vec3( 1.f );

            if ( loaded_node.translation.size() ) {
                translation = glm::vec3( loaded_node.translation[0], loaded_node.translation[1],
                                         loaded_node.translation[2] );
            }
            if ( loaded_node.rotation.size() ) {
                // GLM's quat expects WXYZ order, but GLTF uses XYZW order
                rotation = glm::quat( static_cast<float>( loaded_node.rotation[3] ),
                                      static_cast<float>( loaded_node.rotation[0] ),
                                      static_cast<float>( loaded_node.rotation[1] ),
                                      static_cast<float>( loaded_node.rotation[2] ) );
            }
            if ( loaded_node.scale.size() ) {
                scale =
                    glm::vec3( loaded_node.scale[0], loaded_node.scale[1], loaded_node.scale[2] );
            }

            // Combine translate, rotation, and scale
            // Build transform: translate * rotate * scale
            new_node->transform = glm::translate( glm::mat4( 1.0f ), translation ) *
                                  glm::mat4_cast( rotation ) *
                                  glm::scale( glm::mat4( 1.0f ), scale );
        }

        new_node->inv_transform = glm::inverse( new_node->transform );
        new_node->inv_transpose = glm::inverseTranspose( new_node->transform );

        children_lists.push_back( loaded_node.children );

        // Load the mesh or camera of the node. Potential optimization: Right now we are
        // creating a new mesh on every node. In the case where meshes are instanced, we will
        // created Mesh structs with the same data. We could use a lookup set to resolve this in
        // the future.
        if ( loaded_node.mesh != -1 ) {
            new_node->data = std::make_unique<Mesh>();
            tinygltf::Mesh loaded_mesh = model.meshes[static_cast<size_t>( loaded_node.mesh )];

            // Load primitives
            for ( tinygltf::Primitive& loaded_prim : loaded_mesh.primitives ) {
                Primitive new_prim;
                new_prim.material_id = loaded_prim.material;
                if ( loaded_prim.mode != 4 ) {
                    SDL_Log(
                        "[Scene] GLTF Loading: Mesh detected that uses a mode other than "
                        "TRIANGLES. This is "
                        "currently unsupported." );
                }

                std::vector<glm::vec3> pos;
                std::vector<glm::vec3> nor;
                std::vector<glm::vec2> uv;

                if ( loaded_prim.attributes.count( "POSITION" ) > 0 ) {
                    int accessor_id = loaded_prim.attributes["POSITION"];
                    tinygltf::Accessor accessor =
                        model.accessors[static_cast<size_t>( accessor_id )];
                    int buffer_view_id = accessor.bufferView;
                    if ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float "
                            "- "
                            "unsupported "
                            "component type detected" );
                    }
                    if ( accessor.type != TINYGLTF_TYPE_VEC3 ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all positions to be in vec3 - "
                            "unsupported type "
                            "detected" );
                    }

                    tinygltf::BufferView buffer_view =
                        model.bufferViews[static_cast<size_t>( buffer_view_id )];
                    size_t length = accessor.count;
                    size_t byte_offset = buffer_view.byteOffset;
                    size_t byte_length = buffer_view.byteLength;

                    size_t buffer_id = buffer_view.buffer;
                    tinygltf::Buffer buffer = model.buffers[buffer_id];

                    pos.resize( length );
                    std::memcpy( pos.data(), buffer.data.data() + byte_offset, byte_length );
                }

                if ( loaded_prim.attributes.count( "NORMAL" ) > 0 ) {
                    int accessor_id = loaded_prim.attributes["NORMAL"];
                    tinygltf::Accessor accessor =
                        model.accessors[static_cast<size_t>( accessor_id )];
                    int buffer_view_id = accessor.bufferView;
                    if ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float "
                            "- "
                            "unsupported "
                            "component type detected" );
                    }
                    if ( accessor.type != TINYGLTF_TYPE_VEC3 ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all normals to be in vec3 - "
                            "unsupported type "
                            "detected" );
                    }

                    tinygltf::BufferView buffer_view =
                        model.bufferViews[static_cast<size_t>( buffer_view_id )];
                    size_t length = accessor.count;
                    size_t byte_offset = buffer_view.byteOffset;
                    size_t byte_length = buffer_view.byteLength;

                    size_t buffer_id = buffer_view.buffer;
                    tinygltf::Buffer buffer = model.buffers[buffer_id];

                    nor.resize( length );
                    std::memcpy( nor.data(), buffer.data.data() + byte_offset, byte_length );
                }

                // Currently only accepting one uv coordinate per primative.
                if ( loaded_prim.attributes.count( "TEXCOORD_0" ) > 0 ) {
                    int accessor_id = loaded_prim.attributes["TEXCOORD_0"];
                    tinygltf::Accessor accessor =
                        model.accessors[static_cast<size_t>( accessor_id )];
                    int buffer_view_id = accessor.bufferView;
                    if ( accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float "
                            "- "
                            "unsupported "
                            "component type detected" );
                    }
                    if ( accessor.type != TINYGLTF_TYPE_VEC2 ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all uvs to be in vec2 - "
                            "unsupported "
                            "type "
                            "detected" );
                    }

                    tinygltf::BufferView buffer_view =
                        model.bufferViews[static_cast<size_t>( buffer_view_id )];
                    size_t length = accessor.count;
                    size_t byte_offset = buffer_view.byteOffset;
                    size_t byte_length = buffer_view.byteLength;

                    size_t buffer_id = buffer_view.buffer;
                    tinygltf::Buffer buffer = model.buffers[buffer_id];

                    uv.resize( length );
                    std::memcpy( uv.data(), buffer.data.data() + byte_offset, byte_length );
                }

                // Assume position data always exists
                if ( ( pos.size() != nor.size() && nor.size() != 0 ) ||
                     ( pos.size() != uv.size() && uv.size() != 0 ) ) {
                    SDL_Log(
                        "[Scene] GLTF Loading: WARN, nonzero nor or uv count does not match up "
                        "with pos count" );
                }

                new_prim.vertex_offset = static_cast<int>( out_global_vertices.size() );

                for ( size_t i = 0; i < pos.size(); ++i ) {
                    geometry::Vertex new_vertex;
                    new_vertex.position = pos[i];
                    if ( i < nor.size() ) {
                        new_vertex.normal = nor[i];
                    } else {
                        new_vertex.normal = glm::vec3( 0, 0, 1 );
                    }
                    if ( i < uv.size() ) {
                        new_vertex.uv = uv[i];
                    } else {
                        new_vertex.uv = glm::vec2( 0.5, 0.5 );
                    }
                    new_vertex.color = glm::vec4( 0, 0, 0, 1 );
                    // Vertex Colors are currently not supported.
                    out_global_vertices.push_back( new_vertex );
                }

                if ( loaded_prim.indices != -1 ) {
                    int accessor_id = loaded_prim.indices;
                    tinygltf::Accessor accessor =
                        model.accessors[static_cast<size_t>( accessor_id )];
                    int buffer_view_id = accessor.bufferView;

                    if ( accessor.type != TINYGLTF_TYPE_SCALAR ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all indices to be scalars - "
                            "unsupported type "
                            "detected" );
                    }

                    tinygltf::BufferView buffer_view =
                        model.bufferViews[static_cast<size_t>( buffer_view_id )];
                    new_prim.ind_offset = static_cast<int>( out_global_indices.size() );
                    new_prim.ind_count = accessor.count;
                    size_t byte_offset = buffer_view.byteOffset;
                    size_t byte_length = buffer_view.byteLength;

                    size_t buffer_id = buffer_view.buffer;
                    tinygltf::Buffer buffer = model.buffers[buffer_id];

                    if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT ) {
                        std::vector<uint16_t> ind( new_prim.ind_count );

                        std::memcpy( ind.data(), buffer.data.data() + byte_offset, byte_length );

                        for ( uint16_t index : ind ) {
                            out_global_indices.push_back( static_cast<uint32_t>( index ) );
                        }
                    } else if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT ) {
                        std::vector<uint32_t> ind( new_prim.ind_count );
                        std::memcpy( ind.data(), buffer.data.data() + byte_offset, byte_length );
                        out_global_indices.insert( out_global_indices.end(), ind.begin(),
                                                   ind.end() );
                    } else {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all indices to either be a "
                            "unsigned "
                            "short or an "
                            "unsigned int - unsupported component type detected" );
                    }
                } else {  // If there are no indices, assume position vector will lay out all
                          // triangles
                    new_prim.is_indexed = false;
                    for ( size_t i = 0; i < pos.size(); i++ ) {
                        out_global_indices.push_back( static_cast<unsigned int>( i ) );
                    }
                }

                std::get<std::unique_ptr<Mesh>>( new_node->data )->primitives.push_back( new_prim );
            }
        } else {  // Presumably the node is a camera
            std::unique_ptr<Camera> camera = std::make_unique<Camera>();
            tinygltf::Camera loaded_camera =
                model.cameras[static_cast<size_t>( loaded_node.camera )];
            camera->aspect_ratio = loaded_camera.perspective.aspectRatio;
            camera->near_plane = loaded_camera.perspective.znear;
            camera->far_plane = loaded_camera.perspective.zfar;
            camera->fov_y = loaded_camera.perspective.yfov;
            camera->view_mat = new_node->transform;
            camera->forward = glm::normalize(
                glm::vec3( new_node->transform * glm::vec4( 0.f, 0.f, -1.f, 0.f ) ) );
            camera->up = glm::normalize(
                glm::vec3( new_node->transform * glm::vec4( 0.f, 1.f, 0.f, 0.f ) ) );
            camera->right = glm::normalize( glm::cross( camera->forward, camera->up ) );

            // The main camera will be the first camera loaded.
            if ( scene.main_camera == nullptr ) {
                scene.main_camera = camera.get();
            }
            new_node->data = std::move( camera );
        }

        scene.nodes.push_back( std::move( new_node ) );
    }

    // Assumes scene.nodes order is the same as the indices in the GLTF. Will probably break if
    // this function is multithreaded.

    for ( size_t i = 0; i < scene.nodes.size(); i++ ) {
        std::unique_ptr<Node>& node = scene.nodes[i];
        std::vector<int> children = children_lists[i];
        for ( int child : children ) {
            std::unique_ptr<Node>& child_node = scene.nodes[static_cast<size_t>( child )];
            child_node->parent = node.get();
            node->children.push_back( child_node.get() );
        }
    }

    return true;
}

bool load_hdri( vk::Common vulkan, engine::State& engine, std::string filepath, Scene& scene ) {
    int x = 0, y = 0, channels = 0;
    float* hdriData = stbi_loadf( filepath.c_str(), &x, &y, &channels, 4 );
    if ( x == 0 || y == 0 ) {
        SDL_Log( "[Scene] Failed to load HDRI: %s", stbi_failure_reason() );
        return false;
    }
    Texture hdri;
    hdri.width = x;
    hdri.height = y;
    hdri.bitsPerChannel = 32;  // via stbi_loadf
    hdri.numChannels = 4;      // via stbi_loadf

    VkFormat image_format = get_vk_format( hdri.bitsPerChannel, hdri.numChannels );
    hdri.data = engine::create_image(
        vulkan, engine, static_cast<void*>( hdriData ),
        { static_cast<uint32_t>( hdri.width ), static_cast<uint32_t>( hdri.height ), 1 },
        image_format, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    scene.textures.push_back( hdri );
    scene.hdri_index = scene.textures.size() - 1;
    return true;
}

}  // namespace racecar::scene

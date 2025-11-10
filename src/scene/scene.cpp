#include "scene.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL  // Needed for quaternion.hpp
#include <glm/gtx/quaternion.hpp>

#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

namespace racecar::scene {

bool load_gltf( std::string filepath,
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

        // Load the mesh or camera of the node. Potential optimization: Right now we are creating a
        // new mesh on every node. In the case where meshes are instanced, we will created Mesh
        // structs with the same data. We could use a lookup set to resolve this in the future.
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
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float - "
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
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float - "
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
                            "[Scene] GLTF Loading: Expected all vertex attributes to be float - "
                            "unsupported "
                            "component type detected" );
                    }
                    if ( accessor.type != TINYGLTF_TYPE_VEC2 ) {
                        SDL_Log(
                            "[Scene] GLTF Loading: Expected all uvs to be in vec2 - unsupported "
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

                new_prim.vertex_offset = out_global_vertices.size();

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
                    new_prim.ind_offset = out_global_indices.size();
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
                            "[Scene] GLTF Loading: Expected all indices to either be a unsigned "
                            "short or an "
                            "unsigned int - unsupported component type detected" );
                    }
                } else {  // If there are no indices, assume position vector will lay out all
                          // triangles
                    new_prim.is_indexed = false;
                    for ( size_t i = 0; i < pos.size(); i++ ) {
                        out_global_indices.push_back( i );
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

    // Assumes scene.nodes order is the same as the indices in the GLTF. Will probably break if this
    // function is multithreaded.

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

}  // namespace racecar::scene

#include "mesh.hpp"

namespace racecar::geometry {

struct Triangle {
    geometry::Mesh mesh = {
        .vertices = { {
                          glm::vec4( 1, 0, 0, 1 ),
                          glm::vec3( 0, -0.5, 0 ),
                          glm::vec3( 0.894427, 0, 0.447214 ),
                          glm::vec2( 0.5, 1 ),
                      },
                      {
                          glm::vec4( 0, 1, 0, 1 ),
                          glm::vec3( 0.5, 0.5, 1 ),
                          glm::vec3( 0.894427, 0, 0.447214 ),
                          glm::vec2( 1, 0 ),
                      },
                      {
                          glm::vec4( 0, 0, 1, 1 ),
                          glm::vec3( -0.5, 0.5, -1 ),
                          glm::vec3( 0.894427, 0, 0.447214 ),
                          glm::vec2( 0, 0 ),
                      } },

        .indices = { 0, 2, 1 },
    };

    Triangle( const vk::Common& vulkan, const engine::State& engine ) {
        std::optional<GPUMeshBuffers> uploaded_mesh_buffer =
            geometry::upload_mesh( vulkan, engine, mesh.indices, mesh.vertices );

        if ( !uploaded_mesh_buffer ) {
            SDL_Log( "[VMA] Failed to upload mesh" );
            return;
        }

        mesh.mesh_buffers = uploaded_mesh_buffer.value();
    }
};

}  // namespace racecar::geometry

#include "terrain.h"

#include "../geometry/gpu_mesh_buffers.hpp"

namespace racecar::terrain {

void initialize_terrain(
    vk::Common& vulkan, engine::State& engine, [[maybe_unused]] Terrain& terrain )
{
    // Generate enough information for just one planar quad. Expand it later on arbitrarily
    [[maybe_unused]] int32_t size = 1;

    // Arbitrarily populate this with size later on
    std::vector<Vertex> vertices = {
        { glm::vec3( -1, 0, -1 ) },
        { glm::vec3( -1, 0, 1 ) },
        { glm::vec3( 1, 0, -1 ) },
        { glm::vec3( 1, 0, 1 ) },
    };

    // Hardcoded indices for now
    std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };

    // Create respective mesh buffers using CPU data
    geometry::create_mesh_buffers( vulkan, terrain.mesh_buffers, sizeof( Vertex ) * vertices.size(),
        sizeof( int32_t ) * indices.size() );

    // Upload to GPU
    geometry::upload_mesh_buffers(
        vulkan, engine, terrain.mesh_buffers, vertices.data(), indices.data() );
}

void draw_terrain( [[maybe_unused]] const Terrain& terrain ) { }

}

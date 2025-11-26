#pragma once

#include "../geometry/scene_mesh.hpp"
#include "cbt.h"

namespace racecar::terrain {

// Keep it simple for now
struct Vertex {
    glm::vec3 position;
};

struct Terrain {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    geometry::GPUMeshBuffers mesh_buffers;
};

void initialize_terrain( Terrain& terrain );
void draw_terrain( const Terrain& terrain );

}

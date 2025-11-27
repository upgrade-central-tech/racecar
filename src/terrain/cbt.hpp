#pragma once

#include "../geometry/gpu_mesh_buffers.hpp"
#include "../geometry/scene_mesh.hpp"

namespace racecar::terrain {

struct CBTMesh { };

void initialize_CBT_mesh( CBTMesh& cbt_mesh );

}

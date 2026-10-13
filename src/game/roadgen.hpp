#pragma once

#include <glm/common.hpp>
#include <glm/glm.hpp>

#include "../engine/ub_data.hpp"

namespace racecar {

static constexpr float ROAD_MAX_TURNING_ANGLE = 30.0f;

void recenter_road( std::array<glm::vec3, ub_data::ROAD_NUM_POINTS>& points, glm::vec2 position, float* road_integrated_dist );

}
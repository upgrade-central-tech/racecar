#pragma once

#include <glm/common.hpp>
#include <glm/glm.hpp>

#include "../engine/ub_data.hpp"

namespace racecar{

void recenter_road( std::array<glm::vec3, ub_data::ROAD_NUM_POINTS>& points, glm::vec2 position );

}
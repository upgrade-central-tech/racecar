#pragma once

#include <glm/glm.hpp>

namespace racecar::gui {

struct Material {
    glm::vec4 color = {};
    float roughness = 0.f;
    float metallic = 0.f;
    float clearcoat_roughness = 0.f;
    float clearcoat_weight = 0.f;

    float glintiness = 1.0f;
    float glint_log_density = 18.0f;
    float glint_roughness = 0.28f;
    float glint_randomness = 2.0f;
};

}

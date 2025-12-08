#pragma once

#include <glm/glm.hpp>

namespace racecar::gui {

struct Material {
    glm::vec4 color = {};
    float roughness = 0.f;
    float metallic = 0.f;
    float clearcoat_roughness = 0.f;
    float clearcoat_weight = 0.f;

    float glintiness = 0.f;
    float glint_log_density = 0.f;
    float glint_roughness = 0.f;
    float glint_randomness = 0.f;
};

}

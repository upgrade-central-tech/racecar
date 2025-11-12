
#include <glm/glm.hpp>

/// Stores all uniform buffer structs used for different layouts and shaders. Ensure that all
/// uniform structs are 16 byte aligned!
namespace racecar::uniform_buffer {

/// Camera data
struct Camera {
    glm::mat4 mvp = {};
    glm::mat4 model = glm::mat4( 1.f );
    glm::mat4 inv_model = {};
    glm::vec3 camera_pos = {};
    float padding0 = 0.0f;
    glm::vec3 color = {};
    float padding1 = 0.0f;
};

struct Debug {
    bool enable_albedo_map = false;
    uint8_t p0[3];

    bool enable_normal_map = false;
    uint8_t p1[3];

    bool enable_roughness_metal_map = false;
    uint8_t p2[3];

    bool normals_only = false;
    uint8_t p3[3];

    bool albedo_only = false;
    uint8_t p4[3];

    bool roughness_metal_only = false;
    uint8_t p5[3];
};

} // namespace racecar::uniform_buffer

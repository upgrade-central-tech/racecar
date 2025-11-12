
#include <glm/glm.hpp>

/// Support hpp storing all uniform buffer structs used for different layouts and shaders.
namespace racecar::uniform_buffer {

/// Simple uniform example for setting up camera buffer data
/// Ensure that all uniform structs are 16 byte aligned!
struct CameraBufferData {
    glm::mat4 mvp = {};
    glm::mat4 model = glm::mat4( 1.f );
    glm::mat4 inv_model = {};
    glm::vec3 camera_pos = {};
    float padding = 0.0f;
    glm::vec3 color = {};
    float padding2 = 0.0f;

    glm::ivec4 flags0 = glm::ivec4( 0 );
    glm::ivec4 flags1 = glm::ivec4( 0 );
};

} // namespace racecar::uniform_buffer

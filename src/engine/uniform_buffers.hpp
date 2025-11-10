
#include <glm/glm.hpp>

/// Support hpp storing all uniform buffer structs used for different layouts and shaders.
namespace racecar::uniform_buffer {

/// Simple uniform example for setting up camera buffer data
// Ensure that all uniform structs are 16 byte aligned!
struct CameraBufferData {
    glm::mat4 mvp = {};
    glm::mat4 inv_model  = {};
    glm::vec3 color = {};
    float padding;
};

}  // namespace racecar::uniform_buffer

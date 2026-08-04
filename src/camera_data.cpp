#include "camera_data.hpp"

#include "orbit_camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace racecar {

CameraData get_camera_data()
{
    const engine::State& engine = engine::State::GetConst();
    const camera::OrbitCamera& camera = engine.camera;

    glm::mat4 view = camera::calculate_view_matrix( camera );
    glm::mat4 projection = glm::perspective(
        camera.fov_y,
        camera.aspect_ratio,
        camera.near_plane,
        camera.far_plane
    );

    // This cursed piece of code flips the positive y-axis down because Vulkan's clip space +y
    // points down (whereas in OpenGL/WebGPU it points up).
    projection[1][1] *= -1;

    return {
        .view = view,
        .projection = projection,
        .position = camera::calculate_eye_position( camera ),
    };
}

void update_camera_uniform_buffer(
    gui::Gui& gui, UniformBuffer<ub_data::Camera>& camera_buffer, const CameraData& camera_data
)
{
    const engine::State& engine = engine::State::GetConst();
    const camera::OrbitCamera& camera = engine.camera;

    ub_data::Camera camera_ub = camera_buffer.get_data();

    glm::mat4 model = glm::identity<glm::mat4>();

    glm::mat4 jittered_projection = camera_data.projection;

    if ( gui.aa.mode == gui::Gui::AAData::Mode::TAA ) {
        glm::vec2 offset = vk::Jitter16[engine.rendered_frames % 16];

        jittered_projection[2][0] += offset.x / static_cast<float>( engine.swapchain.extent.width );
        jittered_projection[2][1]
            += offset.y / static_cast<float>( engine.swapchain.extent.height );
    }

    camera_ub.prev_mvp = camera_ub.mvp;
    camera_ub.mvp = jittered_projection * camera_data.view * model;
    camera_ub.model = model;
    camera_ub.view_mat = camera_data.view;
    camera_ub.inv_model = glm::inverse( model );
    camera_ub.inv_vp = glm::inverse( jittered_projection * camera_data.view );

    camera_ub.proj_mat = jittered_projection;
    camera_ub.inv_proj = glm::inverse( jittered_projection );

    camera_ub.camera_pos = glm::vec4( camera_data.position, 1.0f );
    camera_ub.camera_constants
        = glm::vec4( camera.near_plane, camera.far_plane, camera.aspect_ratio, camera.fov_y );

    // Store modded frame index, used for the jitter
    camera_ub.camera_constants1 = glm::vec4( engine.get_frame_index() % 16, 0.0f, 0.0f, 0.0f );

    camera_buffer.set_data( camera_ub );
    camera_buffer.update( engine.get_frame_index() );
}

}

#include "car_animation.hpp"

#include "../scene/scene.hpp"
#include "car_assets.hpp"
#include "terrain/terrain.hpp"

#include <glm/ext/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>

namespace racecar {

// clang-format off
const std::unordered_map<std::string, std::array<glm::vec2, 2>> wheel_centers = {
    { "../assets/bugatti.glb",                  { glm::vec2( -0.658391, -2.35642 ), glm::vec2( -0.667301,2.57912 ) } },
    { "../assets/mclaren.glb",                  { glm::vec2( -0.452689, -1.66372 ), glm::vec2( -0.452689,2.13727 ) } },
    { "../assets/porsche.glb",                  { glm::vec2( 0.0, 0.0 ), glm::vec2( 0.0, 0.0 ) } },
    { "../assets/ferrari.glb",                  { glm::vec2( -0.358881, -1.47297 ), glm::vec2( -0.358822,2.10473 ) } },
    { "../assets/lamborghini_sesto.glb",        { glm::vec2( -0.309112, -1.28    ), glm::vec2( -0.317127,1.27501 ) } },
    { "../assets/mclaren_f1.glb",               { glm::vec2( -0.470348, -2.05553 ), glm::vec2( -0.510266,2.74863 ) } }
};

const std::unordered_map<std::string, float> wheel_radii = {
    { "../assets/bugatti.glb",           1.28f  },
    { "../assets/mclaren.glb",           0.997f },
    { "../assets/porsche.glb",           1.32f  },
    { "../assets/ferrari.glb",           0.715f },
    { "../assets/lamborghini_sesto.glb", 0.649f },
    { "../assets/mclaren_f1.glb",        1.07f  }
};
// clang-format on

void apply_demo_camera_motion(
    camera::OrbitCamera& camera,
    const gui::Gui& gui,
    const scene::Scene& scene,
    const std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    const volumetric::Volumetric& volumetric
)
{
    if ( scene.demo_scene_nodes.car_parent_id.has_value() && gui.demo.enable_camera_lock_on_car ) {
        camera.center = model_mat_uniform_buffers.at( scene.demo_scene_nodes.car_parent_id.value() )
                            .get_data()
                            .model_mat[3];
    }

    camera.center.y += gui.demo.bumpiness
        * static_cast<float>( sin( volumetric.uniform_buffer.get_data().cloud_offset_x * 6000.0 ) );
}

void update_car_transform(
    gui::Gui& gui,
    scene::Scene& scene,
    [[maybe_unused]] std::vector<UniformBuffer<ub_data::ModelMat>>& model_mat_uniform_buffers,
    const volumetric::Volumetric& volumetric,
    [[maybe_unused]] std::vector<bool>& discovered
)
{
    if ( !scene.demo_scene_nodes.car_parent_id.has_value() ) {
        return;
    }

    glm::vec3 velocity = { };

    if ( gui.demo.enable_translation ) {
        velocity = glm::vec3(
            0.1 * sin( volumetric.uniform_buffer.get_data().cloud_offset_x * 1000.0 ),
            0,
            0.025
        );
    }

    [[maybe_unused]]
    glm::mat4 transform = glm::translate( glm::identity<glm::mat4>(), velocity );

    if ( gui.demo.enable_translation ) {
        // nothing here for now, this is scrapped
    }
}

void update_wheel_transforms( scene::Scene& scene )
{
    const engine::State& engine = engine::State::GetConst();

    const float distance = engine.gamestate.world_position.y * geometry::TERRAIN_SCROLL_SCALE
        / geometry::TERRAIN_WORLD_SCALE;
    const float radius = 0.5f * wheel_radii.at( std::string( GLTF_FILE_PATH ) );

    const glm::mat4 model = glm::rotate(
        glm::identity<glm::mat4>(),
        distance / radius,
        glm::vec3( 1.0f, 0.0f, 0.0f )
    );

    if ( scene.demo_scene_nodes.wheel_back_left_id.has_value() ) {
        scene::set_transform( scene, scene.demo_scene_nodes.wheel_back_left_id.value(), model );
    }
    if ( scene.demo_scene_nodes.wheel_back_right_id.has_value() ) {
        scene::set_transform( scene, scene.demo_scene_nodes.wheel_back_right_id.value(), model );
    }
    if ( scene.demo_scene_nodes.wheel_front_left_id.has_value() ) {
        scene::set_transform( scene, scene.demo_scene_nodes.wheel_front_left_id.value(), model );
    }
    if ( scene.demo_scene_nodes.wheel_front_right_id.has_value() ) {
        scene::set_transform( scene, scene.demo_scene_nodes.wheel_front_right_id.value(), model );
    }
}

}

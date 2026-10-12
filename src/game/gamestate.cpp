#include "gamestate.h"

#include "../engine/descriptor_set.hpp"
#include "../engine/state.hpp"
#include "../scene/scene.hpp"
#include "settings.h"
#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <vulkan/vulkan_core.h>

namespace racecar {

static const float MAX_SPEED = 80.0f;
static const float MAX_WHEEL_ANGLE = glm::radians( 40.0f );
static const float WHEEL_TURNING_RATE = glm::radians( 50.0f );
static const float MAX_LATERAL_ACCELERATION = 15.0f;
static const float TURNING_DRAG = 0.1f;

void update_game_state( const scene::Scene& scene )
{
    engine::State& state = engine::State::GetMut();
    GameState& gamestate = state.gamestate;

    const bool* key_states = SDL_GetKeyboardState( nullptr );

    if ( key_states[SDL_SCANCODE_UP] ) {
        gamestate.speed_lock = false;
        gamestate.acceleration = 10.0f;
    } else if ( key_states[SDL_SCANCODE_DOWN] ) {
        gamestate.speed_lock = false;
        gamestate.acceleration = -50.0f;
    } else {
        gamestate.acceleration = gamestate.speed_lock ? 0.0f : -10.0f;
    }

    gamestate.speed += float( state.delta ) * gamestate.acceleration;
    gamestate.speed = glm::clamp( gamestate.speed, 0.0f, MAX_SPEED );


    // front wheel turning
    bool wheel_turn_input = key_states[SDL_SCANCODE_LEFT] || key_states[SDL_SCANCODE_RIGHT];

    if (wheel_turn_input) {
        const float steer_input = ( key_states[SDL_SCANCODE_LEFT] ? 1.0f : 0.0f )
            - ( key_states[SDL_SCANCODE_RIGHT] ? 1.0f : 0.0f );

        gamestate.wheel_angle = glm::clamp(
            gamestate.wheel_angle + steer_input * WHEEL_TURNING_RATE * float( state.delta ),
            -MAX_WHEEL_ANGLE,
            MAX_WHEEL_ANGLE
        );
    }
    else {
        if (glm::abs(gamestate.wheel_angle) > glm::epsilon<float>()) {
            if (gamestate.wheel_angle > 0.0f) {
                gamestate.wheel_angle = glm::clamp(gamestate.wheel_angle - WHEEL_TURNING_RATE * float( state.delta ),
                    0.0f,
                    MAX_WHEEL_ANGLE
                );
            }
            else {
                gamestate.wheel_angle = glm::clamp(gamestate.wheel_angle + WHEEL_TURNING_RATE * float( state.delta ),
                    -MAX_WHEEL_ANGLE,
                    0.0f
                );
            }
        }
    }

    const float back_axle_z = scene.demo_scene_nodes.back_axle_local_z;
    const float wheelbase = scene.demo_scene_nodes.front_axle_local_z - back_axle_z;

    const float speed_sq = gamestate.speed * gamestate.speed;
    if ( speed_sq > 0.0f ) {
        const float grip_wheel_angle = glm::atan( wheelbase * MAX_LATERAL_ACCELERATION / speed_sq );
        gamestate.wheel_angle = glm::clamp( gamestate.wheel_angle, -grip_wheel_angle, grip_wheel_angle );
    }

    const float lateral_acceleration = speed_sq * glm::abs( glm::tan( gamestate.wheel_angle ) ) / wheelbase;
    gamestate.speed = glm::max(
        gamestate.speed - TURNING_DRAG * lateral_acceleration * float( state.delta ), 0.0f
    );

    glm::vec3 world_pos_v3 = glm::vec3(gamestate.world_position.x, 0.0f, gamestate.world_position.y);

    if (glm::distance(world_pos_v3, gamestate.spline_positions[3]) < RuntimeSettings::GetFloat(RacecarSettings::TERRAIN_ROAD_WIDTH) + RuntimeSettings::GetFloat(RacecarSettings::TERRAIN_BLEND_WIDTH)) {
        float sign = gamestate.spline_positions.back().x > 0.0f ? -1.0f : 1.0f;

        for ( size_t i = 0; i + 1 < gamestate.spline_positions.size(); i++ ) {
            gamestate.spline_positions[i] = gamestate.spline_positions[i + 1];
        }
        gamestate.spline_positions.back().z += ROAD_POINT_SPACING;
        gamestate.spline_positions.back().x = sign * 50.0f;
    }

    // car movement with steering
    const float distance = gamestate.speed * static_cast<float>(state.delta);
    gamestate.integrated_distance_traveled += distance;

    glm::vec2 heading = glm::vec2(glm::sin(gamestate.yaw), glm::cos(gamestate.yaw));
    glm::vec2 back_axle_position = gamestate.world_position + back_axle_z * heading + heading * distance;

    gamestate.yaw += distance * glm::tan(gamestate.wheel_angle) / wheelbase;

    heading = glm::vec2(glm::sin(gamestate.yaw), glm::cos(gamestate.yaw));
    gamestate.world_position = back_axle_position - back_axle_z * heading;
}

void initialize_gamestate_buffer(
    UniformBuffer<ub_data::GameStateBuffer>* out_buffer, engine::DescriptorSet* out_desc_set
)
{
    const engine::State& engine = engine::State::GetConst();

    *out_buffer = create_uniform_buffer<ub_data::GameStateBuffer>( { }, engine.frame_overlap );

    *out_desc_set = engine::generate_descriptor_set(
        {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // gamestate
        },
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    );

    engine::update_descriptor_set_uniform( *out_desc_set, *out_buffer, 0 );
}

void update_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>& buffer )
{
    const engine::State& engine = engine::State::GetConst();

    ub_data::GameStateBuffer gamestate_ub = buffer.get_data();

    gamestate_ub.world_position = glm::vec4(
        engine.gamestate.world_position.x,
        0.0f,
        engine.gamestate.world_position.y,
        1.0f
    );
    for ( size_t i = 0; i < ub_data::ROAD_NUM_POINTS; i++ ) {
        gamestate_ub.spline_positions[i] = glm::vec4( engine.gamestate.spline_positions[i], 0.0f );
    }

    buffer.set_data( gamestate_ub );
    buffer.update( engine.get_frame_index() );
}

}

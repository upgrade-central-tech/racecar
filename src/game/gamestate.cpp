#include "gamestate.h"

#include "../engine/descriptor_set.hpp"
#include "../engine/state.hpp"
#include <glm/geometric.hpp>
#include <vulkan/vulkan_core.h>

namespace racecar {

static const glm::vec2 FORWARD = glm::vec2( 0.0f, 1.0f );
static const float MAX_SPEED = 80.0f;
static const float MAX_WHEEL_ANGLE = glm::radians( 70.0f );
static const float WHEEL_TURNING_RATE = glm::radians( 300.0f );

void update_game_state()
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

    gamestate.world_position += FORWARD * gamestate.speed * float( state.delta );

    // front wheel turning
    const float steer_input = ( key_states[SDL_SCANCODE_LEFT] ? 1.0f : 0.0f )
        - ( key_states[SDL_SCANCODE_RIGHT] ? 1.0f : 0.0f );

    const float prev_wheel_angle = gamestate.wheel_angle;

    gamestate.wheel_angle = glm::clamp(
        gamestate.wheel_angle + steer_input * WHEEL_TURNING_RATE * float( state.delta ),
        -MAX_WHEEL_ANGLE,
        MAX_WHEEL_ANGLE
    );
    gamestate.wheel_turn_speed = gamestate.wheel_angle - prev_wheel_angle;

    glm::vec3 world_pos_v3 = glm::vec3(gamestate.world_position.x, 0.0f, gamestate.world_position.y);

    if (glm::distance(world_pos_v3, gamestate.spline_positions[1]) < 10.0f) {
        gamestate.spline_positions[0] = gamestate.spline_positions[1];
        gamestate.spline_positions[1] = gamestate.spline_positions[1] + glm::vec3(0.0, 0.0, 100.0f);
    }
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
    gamestate_ub.spline_positions[0] = glm::vec4(engine.gamestate.spline_positions[0], 0.0f);
    gamestate_ub.spline_positions[1] = glm::vec4(engine.gamestate.spline_positions[1], 0.0f);

    buffer.set_data( gamestate_ub );
    buffer.update( engine.get_frame_index() );
}

}

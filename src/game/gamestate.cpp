#include "gamestate.h"

#include "../engine/descriptor_set.hpp"
#include "../engine/state.hpp"

namespace racecar {

static const glm::vec2 FORWARD = glm::vec2( 0.0f, 1.0f );

void update_game_state() 
{
    engine::State& state = engine::State::GetMut();
    GameState& gamestate = state.gamestate;

    const bool* key_states = SDL_GetKeyboardState( nullptr );

    if (key_states[SDL_SCANCODE_UP]) {
        gamestate.speed_lock = false;
        gamestate.acceleration = 0.01f;
    }
    else if (key_states[SDL_SCANCODE_DOWN]) {
        gamestate.speed_lock = false;
        gamestate.acceleration = -0.05f;
    }
    else {
        gamestate.acceleration = gamestate.speed_lock ? 0.0f : -0.01f;
    }

    gamestate.speed += float(state.delta) * gamestate.acceleration;
    gamestate.speed = glm::clamp(gamestate.speed, 0.0f, 1.0f);

    gamestate.world_position += FORWARD * gamestate.speed * float(state.delta);
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
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
    );

    engine::update_descriptor_set_uniform( *out_desc_set, *out_buffer, 0 );
}

void update_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>& buffer )
{
    const engine::State& engine = engine::State::GetConst();

    ub_data::GameStateBuffer gamestate_ub = buffer.get_data();
    gamestate_ub.world_position = glm::vec4(
        engine.gamestate.world_position.x, 0.0f, engine.gamestate.world_position.y, 1.0f );

    buffer.set_data( gamestate_ub );
    buffer.update( engine.get_frame_index() );
}

}

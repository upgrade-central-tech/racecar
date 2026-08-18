#include "gamestate.h"
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

}
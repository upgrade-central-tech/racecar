#include "gamestate.h"
#include "../engine/state.hpp"

namespace racecar {

void update_game_state() 
{
    engine::State& state = engine::State::GetMut();
    GameState& gamestate = state.gamestate;

    const bool* key_states = SDL_GetKeyboardState( nullptr );

    gamestate.acceleration = key_states[SDL_SCANCODE_UP] ? 0.01f : 0.0f;

    if (key_states[SDL_SCANCODE_UP]) {
        gamestate.acceleration = 0.01f;
    }
    else if (key_states[SDL_SCANCODE_DOWN]) {
        gamestate.acceleration = -0.05f;
    }
    else {
        gamestate.acceleration = -0.01f;
    }

    gamestate.speed += float(state.delta) * gamestate.acceleration;
    gamestate.speed = glm::clamp(gamestate.speed, 0.0f, 1.0f);
}

}
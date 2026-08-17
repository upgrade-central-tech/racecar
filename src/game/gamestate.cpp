#include "gamestate.h"
#include "../engine/state.hpp"

namespace racecar {

void update_game_state() 
{
    engine::State& state = engine::State::GetMut();
    GameState& gamestate = state.gamestate;

    gamestate.acceleration = float(sin(state.time)) * 0.01f;
    gamestate.speed += float(state.delta) * gamestate.acceleration;
}

}
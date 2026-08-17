#pragma once

namespace racecar {

struct GameState {
    float acceleration = 0;
    float speed = 0;
};

void update_game_state();

}
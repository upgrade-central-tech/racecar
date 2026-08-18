#pragma once

#include <glm/glm.hpp>

namespace racecar {

struct GameState {
    float acceleration = 0;
    float speed = 0;
    glm::vec2 world_position = glm::vec2( 0.0f );
};

void update_game_state();

}
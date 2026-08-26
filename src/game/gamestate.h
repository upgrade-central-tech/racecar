#pragma once

#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"

#include <glm/glm.hpp>

namespace racecar::engine {
struct DescriptorSet;
}

namespace racecar {

struct GameState {
    float acceleration = 0;
    float speed = 0;
    glm::vec2 world_position = glm::vec2( 0.0f );
    bool speed_lock = false;

    float wheel_angle = 0.0f;
    float wheel_turn_speed = 0.0f;
};

void update_game_state();

void initialize_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>* out_buffer,
    engine::DescriptorSet* out_desc_set );

void update_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>& buffer );

}

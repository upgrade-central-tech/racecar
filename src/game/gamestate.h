#pragma once

#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"

#include <glm/glm.hpp>

namespace racecar::engine {

struct DescriptorSet;

}

namespace racecar::scene {

struct Scene;

}

namespace racecar {

struct GameState {
    float acceleration = 0;
    float speed = 0;
    glm::vec2 world_position = glm::vec2( 0.0f );
    bool speed_lock = false;

    float wheel_angle = 0.0f;

    glm::vec3 spline_positions[2] = {
        glm::vec3(0.0, 0.0, -100.0f),
        glm::vec3(0.0, 0.0, +100.0f),
    };

    float yaw = 0.0f;
    float integrated_distance_traveled = 0.0f;
};

void update_game_state( const scene::Scene& scene );

void initialize_gamestate_buffer(
    UniformBuffer<ub_data::GameStateBuffer>* out_buffer, engine::DescriptorSet* out_desc_set
);

void update_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>& buffer );

} 

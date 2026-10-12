#pragma once

#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"

#include <glm/glm.hpp>

#include <array>

namespace racecar::engine {

struct DescriptorSet;

}

namespace racecar::scene {

struct Scene;

}

namespace racecar {

constexpr float ROAD_POINT_SPACING = 100.0f;

struct GameState {
    float acceleration = 0;
    float speed = 0;
    glm::vec2 world_position = glm::vec2( 0.0f );
    bool speed_lock = false;

    float wheel_angle = 0.0f;

    std::array<glm::vec3, ub_data::ROAD_NUM_POINTS> spline_positions = [] {
        std::array<glm::vec3, ub_data::ROAD_NUM_POINTS> points;
        for ( size_t i = 0; i < points.size(); i++ ) {
            float sign = i % 2 == 0 ? -1.0f : 1.0f;

            points[i] = glm::vec3(
                50.0f * sign, 0.0f, ( float( i ) - 2.5f ) * ROAD_POINT_SPACING
            );
        }
        return points;
    }();

    float yaw = 0.0f;
    float integrated_distance_traveled = 0.0f;
};

void update_game_state( const scene::Scene& scene );

void initialize_gamestate_buffer(
    UniformBuffer<ub_data::GameStateBuffer>* out_buffer, engine::DescriptorSet* out_desc_set
);

void update_gamestate_buffer( UniformBuffer<ub_data::GameStateBuffer>& buffer );

} 

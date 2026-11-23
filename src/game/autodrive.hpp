#pragma once

namespace racecar {

struct GameState;

struct AutodriveCommand {
    float acceleration;
    float wheel_angle;
};

AutodriveCommand compute_autodrive( const GameState& gamestate, float back_axle_z, float wheelbase );

}

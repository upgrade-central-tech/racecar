#include "autodrive.hpp"

#include "../settings.h"
#include "gamestate.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <limits>
#include <vector>

namespace racecar {

static const int AUTODRIVE_SAMPLES_PER_SEGMENT = 16;
static const float AUTODRIVE_LOOKAHEAD_TIME = 0.8f;
static const float AUTODRIVE_MIN_LOOKAHEAD = 10.0f;
static const float AUTODRIVE_MAX_LOOKAHEAD = 60.0f;
static const float AUTODRIVE_LATERAL_ACCELERATION = 0.8f * MAX_LATERAL_ACCELERATION;
static const float AUTODRIVE_PLANNED_DECELERATION = 25.0f;
static const float AUTODRIVE_PREVIEW_MARGIN = 50.0f;
static const float AUTODRIVE_SPEED_GAIN = 2.0f;

struct RoadSample {
    glm::vec2 position;
    float curvature;
};

static std::vector<RoadSample> sample_road( const GameState& gamestate )
{
    const auto& points = gamestate.spline_positions;
    std::vector<RoadSample> samples;
    samples.reserve( ( points.size( ) - 2 ) * AUTODRIVE_SAMPLES_PER_SEGMENT + 1 );

    for ( size_t i = 0; i + 2 < points.size( ); i++ ) {
        const glm::vec2 p0 = glm::vec2( points[i].x, points[i].z );
        const glm::vec2 p1 = glm::vec2( points[i + 1].x, points[i + 1].z );
        const glm::vec2 p2 = glm::vec2( points[i + 2].x, points[i + 2].z );

        const glm::vec2 a = 0.5f * ( p0 + p1 );
        const glm::vec2 b = p1;
        const glm::vec2 c = 0.5f * ( p1 + p2 );

        const bool last_segment = i + 3 == points.size( );
        const int sample_count = AUTODRIVE_SAMPLES_PER_SEGMENT + ( last_segment ? 1 : 0 );

        for ( int j = 0; j < sample_count; j++ ) {
            const float t = float( j ) / float( AUTODRIVE_SAMPLES_PER_SEGMENT );
            const float u = 1.0f - t;

            const glm::vec2 position = u * u * a + 2.0f * u * t * b + t * t * c;
            const glm::vec2 d1 = 2.0f * u * ( b - a ) + 2.0f * t * ( c - b );
            const glm::vec2 d2 = 2.0f * ( a - 2.0f * b + c );

            const float d1_length = glm::length( d1 );
            const float curvature = glm::abs( d1.x * d2.y - d1.y * d2.x ) /
                glm::max( d1_length * d1_length * d1_length, 1e-6f );

            samples.push_back( { position, curvature } );
        }
    }

    return samples;
}

AutodriveCommand compute_autodrive( const GameState& gamestate, float back_axle_z, float wheelbase )
{
    const std::vector<RoadSample> samples = sample_road( gamestate );

    const glm::vec2 heading = glm::vec2( glm::sin( gamestate.yaw ), glm::cos( gamestate.yaw ) );
    const glm::vec2 lateral = glm::vec2( glm::cos( gamestate.yaw ), -glm::sin( gamestate.yaw ) );
    const glm::vec2 back_axle_position = gamestate.world_position + back_axle_z * heading;

    size_t closest = 0;
    float closest_dist_sq = std::numeric_limits<float>::max( );
    for ( size_t i = 0; i < samples.size( ); i++ ) {
        const glm::vec2 offset = samples[i].position - back_axle_position;
        const float dist_sq = glm::dot( offset, offset );
        if ( dist_sq < closest_dist_sq ) {
            closest_dist_sq = dist_sq;
            closest = i;
        }
    }

    const float lookahead = glm::clamp(
        AUTODRIVE_LOOKAHEAD_TIME * gamestate.speed,
        AUTODRIVE_MIN_LOOKAHEAD,
        AUTODRIVE_MAX_LOOKAHEAD
    );
    const float preview_distance = gamestate.speed * gamestate.speed /
            ( 2.0f * AUTODRIVE_PLANNED_DECELERATION ) +
        AUTODRIVE_PREVIEW_MARGIN;

    float target_speed = RuntimeSettings::Get<RacecarSettings::AUTODRIVE_TARGET_SPEED>( );
    glm::vec2 lookahead_point = samples.back( ).position;
    bool found_lookahead = false;
    float distance_along = 0.0f;

    for ( size_t i = closest; i < samples.size( ); i++ ) {
        if ( i > closest ) {
            distance_along += glm::length( samples[i].position - samples[i - 1].position );
        }

        if ( !found_lookahead && distance_along >= lookahead ) {
            lookahead_point = samples[i].position;
            found_lookahead = true;
        }

        if ( distance_along <= preview_distance && samples[i].curvature > 1e-6f ) {
            const float corner_speed_sq = AUTODRIVE_LATERAL_ACCELERATION / samples[i].curvature;
            const float allowed_speed = glm::sqrt(
                corner_speed_sq + 2.0f * AUTODRIVE_PLANNED_DECELERATION * distance_along
            );
            target_speed = glm::min( target_speed, allowed_speed );
        }

        if ( found_lookahead && distance_along > preview_distance ) {
            break;
        }
    }

    const glm::vec2 to_target = lookahead_point - back_axle_position;
    const float target_distance = glm::max( glm::length( to_target ), 1e-3f );
    const float alpha = glm::atan( glm::dot( to_target, lateral ), glm::dot( to_target, heading ) );

    return {
        .acceleration = glm::clamp(
            AUTODRIVE_SPEED_GAIN * ( target_speed - gamestate.speed ),
            BRAKE_ACCELERATION,
            THROTTLE_ACCELERATION
        ),
        .wheel_angle = glm::atan( 2.0f * wheelbase * glm::sin( alpha ) / target_distance ),
    };
}

}

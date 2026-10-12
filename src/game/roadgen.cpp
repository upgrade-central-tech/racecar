#include "roadgen.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "gamestate.h"

namespace racecar{

struct ClosestPointData {
    float dist_sq;
    float t;
};

ClosestPointData closest_point_on_quadratic_bezier( glm::vec2 A, glm::vec2 B, glm::vec2 C )
{
    const glm::vec2 a = B - A;
    const glm::vec2 b = A - 2.0f * B + C;
    const glm::vec2 c = 2.0f * a;
    const glm::vec2 d = A;

    const auto point_at = [&]( float t ) {
        const glm::vec2 q = d + ( c + b * t ) * t;
        return ClosestPointData { glm::dot( q, q ), t };
    };

    const float bb = glm::dot( b, b );
    if ( bb < 1e-6f * glm::max( glm::dot( a, a ), 1e-8f ) ) {
        const glm::vec2 ac = C - A;
        return point_at( glm::clamp( -glm::dot( A, ac ) / glm::max( glm::dot( ac, ac ), 1e-8f ), 0.0f, 1.0f ) );
    }

    const float kk = 1.0f / bb;
    const float kx = kk * glm::dot( a, b );
    const float ky = kk * ( 2.0f * glm::dot( a, a ) + glm::dot( d, b ) ) / 3.0f;
    const float kz = kk * glm::dot( d, a );

    const float p = ky - kx * kx;
    const float q = kx * ( 2.0f * kx * kx - 3.0f * ky ) + kz;
    float h = q * q + 4.0f * p * p * p;

    if ( h >= 0.0f ) {
        h = std::sqrt( h );
        const float u = std::cbrt( ( h - q ) / 2.0f );
        const float v = std::cbrt( ( -h - q ) / 2.0f );
        return point_at( glm::clamp( u + v - kx, 0.0f, 1.0f ) );
    }

    const float z = std::sqrt( -p );
    const float v = std::acos( glm::clamp( q / ( p * z * 2.0f ), -1.0f, 1.0f ) ) / 3.0f;
    const float m = std::cos( v );
    const float n = std::sin( v ) * 1.732050808f;
    const ClosestPointData r0 = point_at( glm::clamp( ( m + m ) * z - kx, 0.0f, 1.0f ) );
    const ClosestPointData r1 = point_at( glm::clamp( ( -n - m ) * z - kx, 0.0f, 1.0f ) );
    return r0.dist_sq < r1.dist_sq ? r0 : r1;
}

size_t closest_road_segment(
    const std::array<glm::vec3, ub_data::ROAD_NUM_POINTS>& points, glm::vec2 position
)
{
    float best_dist_sq = std::numeric_limits<float>::max();
    size_t best_segment = ROAD_CENTER_SEGMENT;

    for ( size_t i = 0; i + 2 < points.size(); i++ ) {
        const glm::vec2 p0 = glm::vec2( points[i].x, points[i].z ) - position;
        const glm::vec2 p1 = glm::vec2( points[i + 1].x, points[i + 1].z ) - position;
        const glm::vec2 p2 = glm::vec2( points[i + 2].x, points[i + 2].z ) - position;

        const ClosestPointData closest
            = closest_point_on_quadratic_bezier( 0.5f * ( p0 + p1 ), p1, 0.5f * ( p1 + p2 ) );

        if ( closest.dist_sq < best_dist_sq ) {
            best_dist_sq = closest.dist_sq;
            best_segment = closest.t < 0.5f ? i : i + 1;
        }
    }

    return best_segment;
}

glm::vec3 generate_road_point( const glm::vec3& neighbor, float direction )
{
    const float sign = neighbor.x > 0.0f ? -1.0f : 1.0f;
    return glm::vec3( sign * 50.0f, 0.0f, neighbor.z + direction * ROAD_POINT_SPACING );
}

void recenter_road( std::array<glm::vec3, ub_data::ROAD_NUM_POINTS>& points, glm::vec2 position )
{
    const size_t segment = closest_road_segment( points, position );

    if ( segment > ROAD_CENTER_SEGMENT ) {
        const size_t shift = segment - ROAD_CENTER_SEGMENT;
        std::copy( points.begin() + static_cast<std::ptrdiff_t>( shift ), points.end(), points.begin() );
        for ( size_t i = points.size() - shift; i < points.size(); i++ ) {
            points[i] = generate_road_point( points[i - 1], 1.0f );
        }
    } else if ( segment < ROAD_CENTER_SEGMENT ) {
        const size_t shift = ROAD_CENTER_SEGMENT - segment;
        std::copy_backward(
            points.begin(), points.end() - static_cast<std::ptrdiff_t>( shift ), points.end()
        );
        for ( size_t i = shift; i-- > 0; ) {
            points[i] = generate_road_point( points[i + 1], -1.0f );
        }
    }
}

}
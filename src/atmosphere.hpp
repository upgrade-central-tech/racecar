#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "vk/common.hpp"
#include "vk/mem.hpp"

#include <volk.h>

#include <string_view>

namespace racecar::atmosphere {

constexpr std::string_view SHADER_PATH = "../shaders/atmosphere/atmosphere.spv";

/// Earth radius is 6360 km, which we represent as height.
constexpr glm::vec3 EARTH_CENTER = { 0.f, -6'360, 0.f };

/// The sun's angular radius as seen from Earth, in radians.
constexpr float SUN_ANGULAR_RADIUS = 0.015f;

/// White point for tone mapping. Currently neutral white.
constexpr glm::vec3 WHITE_POINT = { 1.f, 1.f, 1.f };

struct Atmosphere {
    vk::mem::AllocatedImage irradiance;
    vk::mem::AllocatedImage scattering;
    vk::mem::AllocatedImage transmittance;

    UniformBuffer<ub_data::Atmosphere> uniform_buffer;

    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet lut_desc_set;
    engine::DescriptorSet sampler_desc_set;

    float sun_azimuth = 0.f; ///< Stored in radians.
    float sun_zenith = 0.f; ///< Stored in radians.
    float exposure = 0.f;
};

Atmosphere initialize( vk::Common& vulkan, engine::State& engine );

glm::vec3 compute_sun_direction( const Atmosphere& atms );

}

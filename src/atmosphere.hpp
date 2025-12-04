#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "vk/mem.hpp"

#include <volk.h>

#include <string_view>

namespace racecar::atmosphere {

constexpr std::string_view SHADER_PATH = "../shaders/atmosphere/sky/atmosphere.spv";

struct Atmosphere {
    vk::mem::AllocatedImage irradiance;
    vk::mem::AllocatedImage scattering;
    vk::mem::AllocatedImage transmittance;

    UniformBuffer<ub_data::Atmosphere> uniform_buffer;

    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet lut_desc_set;
    engine::DescriptorSet sampler_desc_set;

    float sun_azimuth = 0.f; ///< Stored in radians. Bound between [0, 2π].
    float sun_zenith = 0.f; ///< Stored in radians. Bound between [0, π].
};

Atmosphere initialize( vk::Common& vulkan, engine::State& engine );

glm::vec3 compute_sun_direction( const Atmosphere& atms );

}

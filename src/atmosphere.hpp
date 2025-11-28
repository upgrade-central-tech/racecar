#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/pipeline.hpp"
#include "engine/state.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "vk/common.hpp"
#include "vk/mem.hpp"

#include <volk.h>

#include <string_view>

namespace racecar::atmosphere {

constexpr std::string_view SHADER_PATH = "../shaders/atmosphere/atmosphere.spv";

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

struct AtmosphereBaker {
    const Atmosphere& atmosphere;

    vk::mem::AllocatedImage octahedral_sky;
    engine::DescriptorSet octahedral_write;

    engine::Pipeline compute_pipeline;
};

Atmosphere initialize( vk::Common& vulkan, engine::State& engine );

glm::vec3 compute_sun_direction( const Atmosphere& atms );

void initialize_atmosphere_baker( AtmosphereBaker& atms_baker,
    vk::Common& vulkan, [[maybe_unused]] engine::State& engine );

void prebake_octahedral_sky(
    const AtmosphereBaker& atms_baker, vk::Common& vulkan, engine::State& engine );
void bake_octahedral_sky_task( const AtmosphereBaker& atms_baker, VkCommandBuffer command_buffer );

}

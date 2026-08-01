#pragma once

#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/task_list.hpp"
#include "engine/ub_data.hpp"
#include "engine/uniform_buffer.hpp"
#include "vk/mem.hpp"

#include <volk.h>

#include <string_view>

namespace racecar {
struct CameraData;
}

namespace racecar::gui {
struct Gui;
}

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

    float sun_zenith = 0.f; ///< Stored in radians. Bound between [-π/2, π/2].
    float sun_azimuth = 0.f; ///< Stored in radians. Roughly clamped between [0, 2π].
};

Atmosphere initialize( vk::Common& vulkan, engine::State& engine );

glm::vec3 compute_sun_direction( const Atmosphere& atms );

void draw_atmosphere( Context& ctx, engine::State& engine, engine::TaskList& task_list,
    atmosphere::Atmosphere& atms, engine::RWImage& out_color );

void update_atmosphere_uniform_buffer( Context& ctx, engine::State& engine, gui::Gui& gui,
    atmosphere::Atmosphere& atms, const CameraData& camera_data );

}

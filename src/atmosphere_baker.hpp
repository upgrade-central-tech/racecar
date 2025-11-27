#pragma once

#include "atmosphere.hpp"
#include "engine/pipeline.hpp"
#include "engine/task_list.hpp"

namespace racecar::atmosphere {

struct AtmosphereBaker {
    const Atmosphere& atmosphere;

    engine::RWImage octahedral_sky_irradiance;
    vk::mem::AllocatedImage octahedral_sky;

    engine::DescriptorSet octahedral_write;

    engine::Pipeline compute_pipeline;
};

void initialize_atmosphere_baker(
    AtmosphereBaker& atms_baker, vk::Common& vulkan, [[maybe_unused]] engine::State& engine );

// TODO: refactor this later so that it abandons the junk-task system
void prebake_octahedral_sky(
    const AtmosphereBaker& atms_baker, vk::Common& vulkan, engine::State& engine );

void compute_octahedral_sky_irradiance( const AtmosphereBaker& atms_baker, vk::Common& vulkan,
    engine::State& engine, engine::TaskList& task_list );

void bake_octahedral_sky_task( const AtmosphereBaker& atms_baker, VkCommandBuffer command_buffer );

}

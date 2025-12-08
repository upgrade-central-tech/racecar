#pragma once

#include "atmosphere.hpp"
#include "engine/pipeline.hpp"
#include "engine/task_list.hpp"
#include "volumetrics.hpp"

namespace racecar::atmosphere {

struct AtmosphereBaker {
    Atmosphere* atmosphere;

    engine::RWImage octahedral_sky_test;
    engine::RWImage octahedral_sky_irradiance;
    vk::mem::AllocatedImage octahedral_sky;

    engine::DescriptorSet octahedral_write_desc_set;
    engine::DescriptorSet volumetrics_desc_set;
    std::vector<engine::DescriptorSet> octahedral_mip_writes;

    engine::Pipeline compute_pipeline;
};

void initialize_atmosphere_baker( AtmosphereBaker& atms_baker, volumetric::Volumetric& volumetric,
    vk::Common& vulkan, [[maybe_unused]] engine::State& engine );

// TODO: refactor this later so that it abandons the junk-task system
void prebake_octahedral_sky(
    const AtmosphereBaker& atms_baker, vk::Common& vulkan, engine::State& engine );

void compute_octahedral_sky_irradiance( AtmosphereBaker& atms_baker, vk::Common& vulkan,
    [[maybe_unused]] engine::State& engine, [[maybe_unused]] engine::TaskList& task_list );

void bake_octahedral_sky_task( const AtmosphereBaker& atms_baker, VkCommandBuffer command_buffer );

void compute_octahedral_sky_mips( AtmosphereBaker& atms_baker, vk::Common& vulkan,
    engine::State& engine, engine::TaskList& task_list );

}

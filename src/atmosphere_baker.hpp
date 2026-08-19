#pragma once

#include "atmosphere.hpp"
#include "engine/pipeline.hpp"
#include "engine/task_list.hpp"
#include "volumetrics.hpp"

namespace racecar {
struct Context;
}

namespace racecar::atmosphere {

struct AtmosphereBaker {
    Atmosphere* atmosphere;

    engine::RWImage octahedral_sky_mips;
    vk::mem::AllocatedImage octahedral_sky;

    engine::DescriptorSet octahedral_write_desc_set;
    engine::DescriptorSet volumetrics_desc_set;
    std::vector<engine::DescriptorSet> octahedral_mip_writes;

    engine::Pipeline compute_pipeline;
    engine::Pipeline cs_bake_atmosphere_pipeline;
    engine::Pipeline cs_octahedral_mip_pipeline;

    std::vector<UniformBuffer<ub_data::OctahedralData>> mip_data;
};

void initialize_atmosphere_baker(
    AtmosphereBaker& atms_baker,
    const volumetric::Volumetric& volumetric
);

void atmosphere_baker_precompute( AtmosphereBaker& atms_baker, VkCommandBuffer precompute_cmdbuf );

// TODO: refactor this later so that it abandons the junk-task system
void prebake_octahedral_sky(
    const AtmosphereBaker& atms_baker
);

void compute_octahedral_sky(
    AtmosphereBaker& atms_baker, engine::TaskList& task_list
);

void bake_octahedral_sky_task( const AtmosphereBaker& atms_baker, VkCommandBuffer command_buffer );

void compute_octahedral_sky_mips(
    AtmosphereBaker& atms_baker,
    engine::TaskList& task_list
);

void dispatch_atmosphere_baker( engine::TaskList& task_list,
    engine::DescriptorSet& lut_sets, atmosphere::AtmosphereBaker& atms_baker );

}

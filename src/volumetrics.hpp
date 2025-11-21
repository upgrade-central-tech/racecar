#pragma once

#include "engine/task_list.hpp"
#include "engine/ub_data.hpp"

namespace racecar::volumetric {

struct Volumetric {
    // Idiotic solution for now
    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::scene::Mesh scene_mesh;

    vk::mem::AllocatedImage noise_3D;
    UniformBuffer<ub_data::Camera> uniform_buffer;

    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet lut_desc_set;
    engine::DescriptorSet sampler_desc_set;
};

Volumetric initialize( vk::Common& vulkan, engine::State& engine );
bool generate_noise( [[maybe_unused]] Volumetric& volumetric );
void draw_volumetric(
    [[maybe_unused]] Volumetric& volumetric, vk::Common& vulkan, const engine::State& engine, [[maybe_unused]] engine::TaskList& task_list );
    
}
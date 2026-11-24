#pragma once
#include "engine/image.h"
#include "engine/task_list.hpp"
#include "engine/ub_data.hpp"
#include "geometry/quad.hpp"

namespace racecar {
struct CameraData;
struct Context;
}

namespace racecar::atmosphere {
struct Atmosphere;
}

namespace racecar::volumetric {

struct Volumetric {
    // Idiotic solution for now
    // SCENE LOADING/PROCESSING
    scene::Scene scene;
    geometry::quad::Mesh scene_mesh;

    engine::Image cumulus_map;
    engine::Image low_freq_noise;
    engine::Image high_freq_noise;

    engine::Image cloud_buffer;

    UniformBuffer<ub_data::Clouds> uniform_buffer;

    engine::DescriptorSet uniform_desc_set;
    engine::DescriptorSet lut_desc_set;
    engine::DescriptorSet sampler_desc_set;
    engine::DescriptorSet texture_composite_desc_set;
};

Volumetric initialize();

bool generate_noise(
    [[maybe_unused]] Volumetric& volumetric );

void draw_volumetric( [[maybe_unused]] Volumetric& volumetric,
    [[maybe_unused]] engine::TaskList& task_list,
    engine::Image& color_attachment,
    const engine::Image& irradiance_lut,
    const engine::Image& transmittance_lut );

void update_volumetric_uniform_buffer( 
    atmosphere::Atmosphere& atms, volumetric::Volumetric& volumetric,
    const CameraData& camera_data );

}

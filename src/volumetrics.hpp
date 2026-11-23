#pragma once
#include "engine/rwimage.hpp"
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

    engine::RWImage cumulus_map;
    engine::RWImage low_freq_noise;
    engine::RWImage high_freq_noise;

    engine::RWImage cloud_buffer;

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
    engine::RWImage& color_attachment,
    const engine::RWImage& irradiance_lut,
    const engine::RWImage& transmittance_lut );

void update_volumetric_uniform_buffer( 
    atmosphere::Atmosphere& atms, volumetric::Volumetric& volumetric,
    const CameraData& camera_data );

}

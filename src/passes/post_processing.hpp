#pragma once

#include "../context.hpp"
#include "../deferred.hpp"
#include "../engine/post/anti_aliasing.hpp"
#include "../engine/post/ao.hpp"
#include "../engine/post/bloom.hpp"
#include "../engine/post/tonemapping.hpp"
#include "../engine/state.hpp"
#include "../engine/task_list.hpp"
#include "../engine/ub_data.hpp"
#include "../engine/uniform_buffer.hpp"

namespace racecar {

/// Bloom and AO
void pre_transparency_post_passes(
    UniformBuffer<ub_data::Camera>& camera_buffer,
    deferred::GBuffers& gbuffers,
    engine::RWImage& screen_color,
    engine::RWImage& screen_buffer,
    engine::TaskList& task_list,
    engine::post::AoPass& ao_pass,
    engine::post::BloomPass& bloom_pass
);

/// Tonemapping and AA
void post_transparency_post_passes(
    UniformBuffer<ub_data::Camera>& camera_buffer,
    deferred::GBuffers& gbuffers,
    engine::RWImage& screen_color,
    engine::RWImage& screen_buffer,
    engine::RWImage& screen_history,
    engine::TaskList& task_list,
    engine::post::AAPass& aa_pass,
    engine::post::TonemappingPass& tm_pass
);

void create_screen_buffer_pipeline_barrier(
    engine::RWImage& screen_color,
    engine::RWImage& screen_buffer,
    engine::TaskList& task_list
);

void create_screen_buffer_present_pipeline_barrier(
    engine::RWImage& screen_buffer, engine::TaskList& task_list
);

}

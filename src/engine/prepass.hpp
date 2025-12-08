#pragma once

#include "descriptor_set.hpp"
#include "gfx_task.hpp"
#include "pipeline.hpp"

namespace racecar::engine {

struct DepthPrepassMS {
    engine::GfxTask& depth_ms_gfx_task;

    std::vector<DescriptorSet*> descriptor_sets;
    engine::Pipeline pipeline;
};

void PushDepthPrepassMS(
    DepthPrepassMS& depth_prepass_ms, engine::DrawResourceDescriptor draw_descriptor );

}

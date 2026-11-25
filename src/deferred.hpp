#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/image.h"
#include "engine/pipeline_barrier.hpp"
#include "engine/state.hpp"
#include "vk/common.hpp"

namespace racecar::deferred {

struct GBuffers {
    engine::Image GBuffer_Normal;

    engine::Image GBuffer_Position;

    engine::Image GBuffer_Tangent;

    engine::Image GBuffer_UV;

    engine::Image GBuffer_Albedo;

    engine::Image GBuffer_Packed_Data;

    engine::Image GBuffer_Depth;

    engine::Image GBuffer_DepthMS;

    engine::Image GBuffer_Velocity;

    engine::DescriptorSet desc_set;
};

GBuffers initialize_GBuffers( );

std::vector<engine::Image*> get_color_attachments( GBuffers& gbuffers );
engine::Image* get_depth_image( GBuffers& gbuffers );

std::vector<engine::ImageBarrier> init_gbuffer_image_barriers( deferred::GBuffers& gbuffers );

engine::ImageBarrier init_to_color_write( engine::Image& image );
engine::ImageBarrier color_write_to_frag_read( engine::Image& image );

void update_desc_sets( GBuffers& gbuffers );

void create_top_pipeline_barriers(
    deferred::GBuffers& gbuffers,
    engine::Image& screen_color,
    engine::PipelineBarrierDescriptor* top_pipeline_barrier_desc
);

}

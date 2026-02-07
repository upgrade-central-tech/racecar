#pragma once

#include "engine/descriptor_set.hpp"
#include "engine/pipeline_barrier.hpp"
#include "engine/rwimage.hpp"
#include "engine/state.hpp"
#include "vk/common.hpp"

namespace racecar::deferred {

struct GBuffers {
    engine::RWImage GBuffer_Normal;

    engine::RWImage GBuffer_Position;

    engine::RWImage GBuffer_Tangent;

    engine::RWImage GBuffer_UV;

    engine::RWImage GBuffer_Albedo;

    engine::RWImage GBuffer_Packed_Data;

    engine::RWImage GBuffer_Depth;

    engine::RWImage GBuffer_DepthMS;

    engine::RWImage GBuffer_Velocity;

    engine::DescriptorSet desc_set;
};

GBuffers initialize_GBuffers( vk::Common& vulkan, engine::State& engine );

std::vector<engine::RWImage> get_color_attachments( GBuffers& gbuffers );
engine::RWImage get_depth_image( GBuffers& gbuffers );

engine::PipelineBarrierDescriptor initialize_barrier_desc( GBuffers& gbuffers );

engine::ImageBarrier init_to_color_write( engine::RWImage& image );
engine::ImageBarrier color_write_to_frag_read( engine::RWImage& image );

void update_desc_sets( vk::Common& vulkan, engine::State& engine, GBuffers& gbuffers );

}

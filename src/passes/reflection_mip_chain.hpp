#pragma once

#include <cstdint>

namespace racecar {

constexpr uint32_t REFLECTION_MIP_COUNT = 6;

}

#if RACECAR_RAY_TRACING

#include "../engine/descriptor_set.hpp"
#include "../engine/pipeline.hpp"
#include "../engine/rwimage.hpp"
#include "../engine/task_list.hpp"

#include <array>

namespace racecar {

struct ReflectionMipChain {
    static constexpr uint32_t MIP_COUNT = REFLECTION_MIP_COUNT;
    static constexpr uint32_t DOWNSAMPLE_COUNT = MIP_COUNT - 1;

    // Refs to reflection images
    engine::RWImage* reflection_color = nullptr;
    engine::RWImage* reflection_data = nullptr;

    std::array<engine::DescriptorSet, DOWNSAMPLE_COUNT> downsample_desc_sets = { };
    engine::Pipeline downsample_pipeline = { };
};

void create_reflection_mip_chain_resources( ReflectionMipChain* chain,
    engine::RWImage& reflection_color, engine::RWImage& reflection_data );

void add_reflection_mip_chain_pass( ReflectionMipChain& chain, engine::TaskList& task_list );

}

#endif // RACECAR_RAY_TRACING

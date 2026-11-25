#pragma once

#include "context.hpp"
#include "engine/descriptor_set.hpp"
#include "engine/image.h"
#include "engine/state.hpp"

namespace racecar {

/// Binding layout of the global LUT descriptor set.
enum LUT_INDEX { BRDF = 0, GLINT = 1, OCTAHEDRAL_SKY = 2, OCTAHEDRAL_MIPS = 3 };

void create_lut_sets(
    engine::DescriptorSet* lut_sets, engine::Image* lut_brdf, engine::Image* glint_noise
);

}

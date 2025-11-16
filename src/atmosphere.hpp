#pragma once

#include "engine/state.hpp"
#include "vk/common.hpp"
#include "vk/mem.hpp"

namespace racecar::atmosphere {

struct Atmosphere {
    vk::mem::AllocatedImage irradiance;
    vk::mem::AllocatedImage scattering;
    vk::mem::AllocatedImage transmittance;
};

Atmosphere initialize( vk::Common& vulkan, engine::State& engine );

}

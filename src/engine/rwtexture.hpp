#pragma once

#include <volk.h>

#include <vector>


namespace racecar::engine {

/// Abstraction
struct RWTexture {
    std::vector<VkImage>
};

}

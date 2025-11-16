#pragma once

#include "../vk/mem.hpp"

#include <volk.h>

namespace racecar::geometry {

struct GPUMeshBuffers {
    vk::mem::AllocatedBuffer index_buffer;
    vk::mem::AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address = 0;
};

}

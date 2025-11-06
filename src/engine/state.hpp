#pragma once

#include "../vk/common.hpp"
#include "frame_data.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace racecar::engine {

/// Global engine state.
struct State {
    vkb::Swapchain swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    std::vector<FrameData> frames;
    std::vector<VkSemaphore> render_semaphores;
    uint32_t frame_overlap = 0;
    uint32_t frame_number = 0;
    uint32_t rendered_frames = 0;

    VkCommandPool global_gfx_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer global_gfx_cmd_buf = VK_NULL_HANDLE;
};

std::optional<State> initialize( SDL_Window* window, const vk::Common& vulkan );
void free( State& engine, const vk::Common& vulkan );

}  // namespace racecar::engine

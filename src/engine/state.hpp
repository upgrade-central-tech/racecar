#pragma once

#include "../vk/common.hpp"

#include "descriptors.hpp"
#include "imm_submit.hpp"
#include "frame_data.hpp"
#include "../scene/camera.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace racecar::engine {

/// Global engine state.
struct State {
    vkb::Swapchain swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    // Initialized in initalize
    scene::Camera global_camera = {};

    std::vector<FrameData> frames;
    uint32_t frame_overlap = 1;
    uint32_t frame_number = 0;
    uint32_t rendered_frames = 0;

    VkFence render_fence = nullptr;

    ImmediateSubmit immediate_submit = {};

    VkCommandPool global_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer global_start_cmd_buf = VK_NULL_HANDLE;
    VkCommandBuffer global_end_cmd_buf = VK_NULL_HANDLE;

    DescriptorSystem descriptor_system = {};

    /// Semaphore that gets signaled when `vkAcquireNextImageKHR` finishes.
    VkSemaphore acquire_img_semaphore = nullptr;

    /// Semaphore that gets signaled when `global_start_cmd_buf` completes.
    /// Should be used as a wait for gfx tasks
    VkSemaphore begin_gfx_semaphore = nullptr;

    /// Semaphore that gets signaled when `global_end_cmd_buf` completes.
    /// Should be used as a wait for `vkQueuePresetKHR`.
    VkSemaphore present_image_signal_semaphore = nullptr;
};

std::optional<State> initialize( SDL_Window* window, const vk::Common& vulkan );
void free( State& engine, const vk::Common& vulkan );

}  // namespace racecar::engine

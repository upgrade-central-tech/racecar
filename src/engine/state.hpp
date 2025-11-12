#pragma once

#include "../orbit_camera.hpp"
#include "../vk/common.hpp"
#include "../vk/mem.hpp"
#include "descriptors.hpp"
#include "imm_submit.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace racecar::engine {

struct FrameData {
    VkFence render_fence = VK_NULL_HANDLE;

    VkCommandBuffer start_cmdbuf = VK_NULL_HANDLE;
    VkCommandBuffer render_cmdbuf = VK_NULL_HANDLE;
    VkCommandBuffer end_cmdbuf = VK_NULL_HANDLE;

    VkSemaphore acquire_start_smp = VK_NULL_HANDLE;
    VkSemaphore start_render_smp = VK_NULL_HANDLE;
    VkSemaphore render_end_smp = VK_NULL_HANDLE;

    vk::mem::UniformBuffer triangle_uniform_buffer;
};

struct SwapchainSemaphores {
    VkSemaphore end_present_smp = VK_NULL_HANDLE;
};

/// Global engine state.
struct State {
    vkb::Swapchain swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<vk::mem::AllocatedImage> depth_images;

    camera::OrbitCamera camera;

    uint32_t frame_overlap = 1;
    uint32_t frame_number = 1;
    uint32_t rendered_frames = 0;

    ImmediateSubmit immediate_submit = {};

    VkCommandPool cmd_pool = VK_NULL_HANDLE;

    std::vector<FrameData> frames;
    std::vector<SwapchainSemaphores> swapchain_semaphores;

    DescriptorSystem descriptor_system = {};

    size_t get_frame_index() const;
};

std::optional<State> initialize( SDL_Window* window, vk::Common& vulkan );
void free( State& engine, vk::Common& vulkan );

}  // namespace racecar::engine

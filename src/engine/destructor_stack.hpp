#pragma once

#include <functional>
#include <stack>
#include <volk.h>
#include "../vk/vma.hpp"

namespace racecar::vk::mem {
    struct AllocatedBuffer;
    struct AllocatedImage;
}

struct DestructorStack {
    std::stack<std::function<void()>> destructors;

    template <typename T>
    void push(VkDevice device, T handle, void (VKAPI_PTR* destroy_func)(VkDevice, T, const VkAllocationCallbacks*)) {
        if (handle != VK_NULL_HANDLE) {
            // Push a lambda function that captures the device, handle, and destroy_func
            destructors.push([=]() {
                destroy_func(device, handle, nullptr);
            });
        }
    }  

    void push_free_cmdbufs(VkDevice device, VkCommandPool pool, const std::vector<VkCommandBuffer>& buffers);

    void push_free_vmabuffer( const VmaAllocator allocator, racecar::vk::mem::AllocatedBuffer buffer );

    void push_free_vmaimage( const VmaAllocator allocator, racecar::vk::mem::AllocatedImage allocated_image );

    void execute_cleanup();
};
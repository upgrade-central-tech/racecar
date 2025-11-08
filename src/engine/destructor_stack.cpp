#include "destructor_stack.hpp"


void DestructorStack::execute_cleanup() {
    while ( !destructors.empty() ) {
        auto destructor = destructors.top();
        destructors.pop();

        destructor();
    }
}

void DestructorStack::push_free_cmdbufs(VkDevice device, VkCommandPool pool, const std::vector<VkCommandBuffer>& buffers) {
    if (!buffers.empty() && pool != VK_NULL_HANDLE) {
        // NOTE: Capturing the vector by value ensures it is available when run() is called later
        destructors.push([=]() {
            vkFreeCommandBuffers(device, pool, static_cast<uint32_t>(buffers.size()), buffers.data());
        });
    }
}
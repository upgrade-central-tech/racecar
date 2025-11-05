#include "init.hpp"

#include <SDL3/SDL.h>

#include <fstream>
#include <ios>

namespace racecar::vk::init {

VkCommandPoolCreateInfo command_pool_create_info(uint32_t queue_family_index,
                                                 VkCommandPoolCreateFlags flags) {
  return {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = flags,
      .queueFamilyIndex = queue_family_index,
  };
}

VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count) {
  return {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = count,
  };
}

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags) {
  return {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = flags,
  };
}

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags) {
  return {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = flags,
  };
}

VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags) {
  return {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = flags,
  };
}

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_mask) {
  return {
      .aspectMask = aspect_mask,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };
}

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
                                            VkSemaphore semaphore) {
  return {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = semaphore,
      .value = 1,
      .stageMask = stage_mask,
      .deviceIndex = 0,
  };
}

VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer command_buffer) {
  return {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = command_buffer,
      .deviceMask = 0,
  };
}

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* command_buffer_info,
                          VkSemaphoreSubmitInfo* signal_semaphore_info,
                          VkSemaphoreSubmitInfo* wait_semaphore_info) {
  return {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount = (wait_semaphore_info ? 1U : 0U),
      .pWaitSemaphoreInfos = wait_semaphore_info,
      .commandBufferInfoCount = 1U,
      .pCommandBufferInfos = command_buffer_info,
      .signalSemaphoreInfoCount = (signal_semaphore_info ? 1U : 0U),
      .pSignalSemaphoreInfos = signal_semaphore_info,
  };
}

VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits flags,
                                                                  VkShaderModule shader_module,
                                                                  std::string_view name) {
  return {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = flags,
      .module = shader_module,
      .pName = name.data(),
  };
}

/// Similar to a resource barrier transition in DX12, this is meant
/// to adjust a given image's layout/access properties, etc.
/// Ex: To write to our image in draw, need our image layout to transition
/// from a read only state (VK_IMAGE_LAYOUT_GENERAL) to write (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR).
///
/// Helpful execution and memory barrier info can be found here:
/// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
void transition_image(VkCommandBuffer command_buffer,
                      VkImage image,
                      VkImageLayout currentLayout,
                      VkImageLayout newLayout) {
  VkImageMemoryBarrier2 image_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

      // For example, we can consider only looking at COMPUTE work, so
      // VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT.
      // Similarly, dstStageMask represents the work that waits until the barrier is lifted.
      //
      // Note: VkGuide notes that ALL_COMMANDS in StageMask is inefficient!!!
      // More refined examples of using StageMask effectively can be found here:
      // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

      .oldLayout = currentLayout,
      .newLayout = newLayout,
  };

  VkImageAspectFlags aspect_mask = newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                       ? VK_IMAGE_ASPECT_DEPTH_BIT
                                       : VK_IMAGE_ASPECT_COLOR_BIT;
  image_barrier.subresourceRange = image_subresource_range(aspect_mask);
  image_barrier.image = image;

  VkDependencyInfo dependency_info = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                      .imageMemoryBarrierCount = 1,
                                      .pImageMemoryBarriers = &image_barrier};

  vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

std::optional<VkShaderModule> create_shader_module(const Common& vulkan,
                                                   std::filesystem::path shader_path) {
  std::ifstream file(shader_path.string(), std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    SDL_Log("[Shader] Could not open file \"%s\"",
            std::filesystem::absolute(shader_path).string().c_str());
    return {};
  }

  // Use read position to determine size of file and pre-allocate buffer
  std::size_t file_size = static_cast<std::size_t>(file.tellg());
  std::vector<char> shader_buffer(file_size);

  file.seekg(0);
  file.read(shader_buffer.data(), static_cast<std::streamsize>(file_size));
  file.close();

  // The pointer is of type `uint32_t`, so we have to cast it. std::vector's default allocator
  // ensures the data satisfies the alignment requirements
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_buffer.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(shader_buffer.data());

  VkShaderModule shader_module;
  RACECAR_VK_CHECK(vkCreateShaderModule(vulkan.device, &create_info, nullptr, &shader_module),
                   "Failed to create shader module");

  return shader_module;
}

}  // namespace racecar::vk::init

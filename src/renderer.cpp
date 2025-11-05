#include "renderer.hpp"

#include "vk/init.hpp"

#include <array>

namespace racecar::renderer {

void create_graphics_pipeline(const vk::Common& vulkan) {
  std::optional<VkShaderModule> shader_module_opt =
      vk::init::create_shader_module(vulkan, "../shaders/triangle.spv");

  if (!shader_module_opt) {
    SDL_Log("[Renderer] Failed to create shader module");
    return;
  }

  VkShaderModule& shader_module = shader_module_opt.value();

  [[maybe_unused]] std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
      vk::init::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shader_module,
                                                  "vertex_main"),
      vk::init::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_module,
                                                  "fragment_main"),
  };

  std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
  dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
  dynamic_state_create_info.pDynamicStates = dynamic_states.data();

  VkPipelineVertexInputStateCreateInfo vertex_input_info;
  vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = 0;
  vertex_input_info.pVertexBindingDescriptions = nullptr;
  vertex_input_info.vertexAttributeDescriptionCount = 0;
  vertex_input_info.pVertexAttributeDescriptions = nullptr;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
  input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly_info.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(vulkan.swapchain.extent.width);
  viewport.height = static_cast<float>(vulkan.swapchain.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor;
  scissor.offset = {.x = 0, .y = 0};
  scissor.extent = vulkan.swapchain.extent;

  vkDestroyShaderModule(vulkan.device, shader_module, nullptr);
}

std::optional<bool> draw(const Context& ctx) {
  const vk::Common& vulkan = ctx.vulkan;
  const vk::FrameData* frame = &vulkan.frames[vulkan.frame_number];

  RACECAR_VK_CHECK(vkWaitForFences(vulkan.device, 1, &frame->render_fence, true, 1e9),
                   "Failed to wait for fences");
  RACECAR_VK_CHECK(vkResetFences(vulkan.device, 1, &frame->render_fence),
                   "Failed to reset render fence");

  // Request swap chain index.
  uint32_t swapchain_index = 0;
  RACECAR_VK_CHECK(vkAcquireNextImageKHR(vulkan.device, vulkan.swapchain, 1e9,
                                         frame->swapchain_semaphore, nullptr, &swapchain_index),
                   "Failed to acquire next image from swapchain");

  // Reset command buffer clears all command from memory - always make sure to do this.
  VkCommandBuffer command_buffer = frame->command_buffer;
  RACECAR_VK_CHECK(vkResetCommandBuffer(command_buffer, 0), "Failed to reset command buffer");

  // Begin the command buffer, use one time flag for single usage (one submit per frame).
  VkCommandBufferBeginInfo command_buffer_begin_info =
      vk::init::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  RACECAR_VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info),
                   "Failed to begin command buffer");

  // Make swapchain image writeable
  const VkImage& swapchain_image = vulkan.swapchain_images[swapchain_index];

  // VERY IMPORTANT: VK_IMAGE_LAYOUT_GENERAL literally allows all perms, including read/write image
  // VkGuide notes that there are more optimal image layouts for rendering.
  vk::init::transition_image(command_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

  // Ripped from the VkGuide, but so far all we do for now is clear the background
  // with a random color.
  VkClearColorValue clear_value;
  float flash = std::abs(std::sin(static_cast<float>(vulkan.frame_number) / 120.f));
  clear_value = {{0.0f, 0.0f, flash, 1.0f}};

  VkImageSubresourceRange clear_range =
      vk::init::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  vkCmdClearColorImage(command_buffer, swapchain_image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                       &clear_range);

  vk::init::transition_image(command_buffer, swapchain_image, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  RACECAR_VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end the command buffer");

  // Prepare to submit our command to the graphics queue
  VkCommandBufferSubmitInfo command_info = vk::init::command_buffer_submit_info(command_buffer);
  VkSemaphoreSubmitInfo wait_info = vk::init::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame->swapchain_semaphore);
  VkSemaphoreSubmitInfo signal_info = vk::init::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame->render_semaphore);

  VkSubmitInfo2 submit_info = vk::init::submit_info(&command_info, &signal_info, &wait_info);
  RACECAR_VK_CHECK(vkQueueSubmit2(vulkan.graphics_queue, 1, &submit_info, frame->render_fence),
                   "Graphics queue submit failed");

  // Present the image... immediately? This will stall!
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.pSwapchains = &vulkan.swapchain.swapchain;
  present_info.swapchainCount = 1;

  present_info.pWaitSemaphores = &frame->render_semaphore;
  present_info.waitSemaphoreCount = 1;

  present_info.pImageIndices = &swapchain_index;

  RACECAR_VK_CHECK(vkQueuePresentKHR(vulkan.graphics_queue, &present_info),
                   "Failed to present to screen");

  return true;
}

}  // namespace racecar::renderer

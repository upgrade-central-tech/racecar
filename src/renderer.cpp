#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include "renderer.hpp"

#include "vk/create.hpp"
#include "vk/utility.hpp"

#include <array>
#include <limits>

namespace racecar::renderer {

std::optional<Pipeline> create_gfx_pipeline(const vk::Common& vulkan) {
  std::optional<VkShaderModule> shader_module_opt =
      vk::create::shader_module(vulkan, "../shaders/triangle.spv");

  if (!shader_module_opt) {
    SDL_Log("[Renderer] Failed to create shader module");
    return {};
  }

  VkShaderModule& shader_module = shader_module_opt.value();

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
  };

  VkPipelineViewportStateCreateInfo viewport_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterization_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment_info = {
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo color_blend_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment_info,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  VkPipelineLayout gfx_layout = nullptr;

  if (VkResult result =
          vkCreatePipelineLayout(vulkan.device, &pipeline_layout_info, nullptr, &gfx_layout);
      result != VK_SUCCESS) {
    SDL_Log("[Vulkan] Failed to create pipeline layout | Error code: %d", result);
    vkDestroyShaderModule(vulkan.device, shader_module, nullptr);
    return {};
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
      vk::create::pipeline_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT, shader_module,
                                             "vertex_main"),
      vk::create::pipeline_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, shader_module,
                                             "fragment_main"),
  };

  // Use dynamic rendering instead of manually creating render passes
  VkPipelineRenderingCreateInfo pipeline_rendering_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &vulkan.swapchain.image_format,
  };

  VkGraphicsPipelineCreateInfo gfx_pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &pipeline_rendering_info,
      .stageCount = 2,
      .pStages = shader_stages.data(),
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly_info,
      .pViewportState = &viewport_state_info,
      .pRasterizationState = &rasterization_info,
      .pMultisampleState = &multisample_info,
      .pColorBlendState = &color_blend_info,
      .pDynamicState = &dynamic_state_info,
      .layout = gfx_layout,
      .renderPass = nullptr,
  };

  Pipeline gfx_pipeline;

  RACECAR_VK_CHECK(vkCreateGraphicsPipelines(vulkan.device, nullptr, 1, &gfx_pipeline_info, nullptr,
                                             &gfx_pipeline.handle),
                   "Failed to create graphics pipeline");

  vkDestroyShaderModule(vulkan.device, shader_module, nullptr);

  gfx_pipeline.layout = gfx_layout;

  return gfx_pipeline;
}

void free_pipeline(const vk::Common& vulkan, Pipeline& pipeline) {
  vkDestroyPipeline(vulkan.device, pipeline.handle, nullptr);
  vkDestroyPipelineLayout(vulkan.device, pipeline.layout, nullptr);
}

bool draw(const Context& ctx, const Pipeline& gfx_pipeline) {
  const vk::Common& vulkan = ctx.vulkan;

  // This is the previous frame. We want to block the host until it has finished
  const vk::FrameData* frame = &vulkan.frames[vulkan.frame_number];

  // Using the maximum 64-bit unsigned integer value effectively disables the timeout
  RACECAR_VK_CHECK(vkWaitForFences(vulkan.device, 1, &frame->render_fence, VK_TRUE,
                                   std::numeric_limits<uint64_t>::max()),
                   "Failed to wait for frame render fence");

  // Manually reset previous frame's render fence to an unsignaled state
  RACECAR_VK_CHECK(vkResetFences(vulkan.device, 1, &frame->render_fence),
                   "Failed to reset frame render fence");

  // Request swapchain index.
  uint32_t swapchain_index = 0;
  RACECAR_VK_CHECK(
      vkAcquireNextImageKHR(vulkan.device, vulkan.swapchain, std::numeric_limits<uint64_t>::max(),
                            frame->swapchain_semaphore, nullptr, &swapchain_index),
      "Failed to acquire next image from swapchain");

  // Reset command buffer clears all commands from memory - always make sure to do this.
  VkCommandBuffer command_buffer = frame->command_buffer;
  RACECAR_VK_CHECK(vkResetCommandBuffer(command_buffer, 0), "Failed to reset command buffer");

  // Begin the command buffer, use one time flag for single usage (one submit per frame).
  VkCommandBufferBeginInfo command_buffer_begin_info =
      vk::create::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  RACECAR_VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info),
                   "Failed to begin command buffer");

  // Make swapchain image writeable
  const VkImage& current_image = vulkan.swapchain_images[swapchain_index];
  vk::utility::transition_image(
      command_buffer, current_image, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  // Clear the background with a pulsing blue color
  float flash = std::abs(std::sin(static_cast<float>(vulkan.rendered_frames) / 120.f));
  VkClearColorValue clear_color = {{0.0f, 0.0f, flash, 1.0f}};

  VkRenderingAttachmentInfo color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = vulkan.swapchain_image_views[swapchain_index],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.color = clear_color},
  };

  VkRenderingInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {.x = 0, .y = 0}, .extent = vulkan.swapchain.extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
  };

  vkCmdBeginRendering(command_buffer, &rendering_info);

  {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_pipeline.handle);

    // Dynamically set viewport state
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(vulkan.swapchain.extent.width),
        .height = static_cast<float>(vulkan.swapchain.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    // Dynamically set scissor state
    VkRect2D scissor = {
        .offset = {.x = 0, .y = 0},
        .extent = vulkan.swapchain.extent,
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
  }

  vkCmdEndRendering(command_buffer);

  // Transition image layout back so it can be presented on the screen
  vk::utility::transition_image(
      command_buffer, current_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

  RACECAR_VK_CHECK(vkEndCommandBuffer(command_buffer), "Failed to end the command buffer");

  // Prepare to submit our command to the graphics queue
  VkSemaphoreSubmitInfo wait_info = vk::create::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame->swapchain_semaphore);
  VkSemaphoreSubmitInfo signal_info = vk::create::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, vulkan.render_semaphores[swapchain_index]);
  VkCommandBufferSubmitInfo command_info = vk::create::command_buffer_submit_info(command_buffer);
  VkSubmitInfo2 submit_info = vk::create::submit_info(&command_info, &signal_info, &wait_info);

  RACECAR_VK_CHECK(vkQueueSubmit2(vulkan.graphics_queue, 1, &submit_info, frame->render_fence),
                   "Graphics queue submit failed");

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &vulkan.render_semaphores[swapchain_index],
      .swapchainCount = 1,
      .pSwapchains = &vulkan.swapchain.swapchain,
      .pImageIndices = &swapchain_index,
  };

  RACECAR_VK_CHECK(vkQueuePresentKHR(vulkan.graphics_queue, &present_info),
                   "Failed to present to screen");

  return true;
}

}  // namespace racecar::renderer

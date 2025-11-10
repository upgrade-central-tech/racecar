#include "gui.hpp"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_sdl3.h"
#include "../imgui/imgui_impl_vulkan.h"

namespace racecar::engine::gui {

/*namespace {

  PFN_vkVoidFunction loader_function(const char* function_name, void* user_data) {
    vkb::Instance* instance_ptr = static_cast<vkb::Instance*>(user_data);

    return instance_ptr->fp_vkGetInstanceProcAddr(instance_ptr->instance, function_name);
  }

}*/

std::optional<Gui> initialize( const Context& ctx, const State& engine ) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = pool_size.descriptorCount,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    Gui gui;

    RACECAR_VK_CHECK( vkCreateDescriptorPool( ctx.vulkan.device, &descriptor_pool_info, nullptr,
                                              &gui.descriptor_pool ),
                      "[engine::gui::initialize] Failed to create descriptor pool" );

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Enable keyboard controls
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan( ctx.window );

    {
        ImGui_ImplVulkan_InitInfo init_info = {
            .ApiVersion = VK_API_VERSION_1_4,
            .Instance = ctx.vulkan.instance,
            .PhysicalDevice = ctx.vulkan.device.physical_device,
            .Device = ctx.vulkan.device,
            .QueueFamily = ctx.vulkan.graphics_queue_family,
            .Queue = ctx.vulkan.graphics_queue,
            .DescriptorPool = gui.descriptor_pool,
            .MinImageCount = engine.swapchain.requested_min_image_count,
            .ImageCount = engine.swapchain.image_count,

            .PipelineInfoMain =
                {
                    .PipelineRenderingCreateInfo =
                        {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                            .colorAttachmentCount = 1,
                            .pColorAttachmentFormats = &engine.swapchain.image_format,
                        },
                },
            .UseDynamicRendering = true,

        };

        SDL_Log( "[Info] vkGetInstanceProcAddr: %p", ctx.vulkan.instance.fp_vkGetInstanceProcAddr );
        SDL_Log( "[Info] vkInstance: %p", ctx.vulkan.instance.instance );

        /*bool result = ImGui_ImplVulkan_LoadFunctions(
          VK_API_VERSION_1_4,
          &loader_function,
          const_cast<void*>(static_cast<const void*>(&ctx.vulkan.instance))
        );

        if (!result) {
          SDL_Log("[ImGui] Failed to load Vulkan pointer functions!");
          return {};
        } else {
          SDL_Log("worked");
        }*/

        ImGui_ImplVulkan_Init( &init_info );
    }

    return gui;
}

void free( const vk::Common& vulkan, Gui& gui ) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool( vulkan.device, gui.descriptor_pool, nullptr );
}

}  // namespace racecar::engine::gui

#pragma once

#define VK_NO_PROTOTYPES  // Necessary because VMA includes Vulkan headers internally

#define VMA_VULKAN_VERSION 1004000  // Vulkan 1.4
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#include <volk.h>  // Enables vmaImportVulkanFunctionsFromVolk() usage
#include <vk_mem_alloc.h>

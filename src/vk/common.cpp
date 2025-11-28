#include "common.hpp"

#include "../exception.hpp"
#include "../log.hpp"
#include "create.hpp"

#include <SDL3/SDL_vulkan.h>

#include <format>

namespace racecar::vk {

namespace {

vkb::Instance create_instance()
{
    vkb::Result<vkb::SystemInfo> system_info_ret = vkb::SystemInfo::get_system_info();

    if ( !system_info_ret ) {
        throw Exception( "[vkb] Unable to get system info" );
    }

    vkb::SystemInfo& system_info = system_info_ret.value();
    auto handler
        = reinterpret_cast<PFN_vkGetInstanceProcAddr>( SDL_Vulkan_GetVkGetInstanceProcAddr() );

    if ( !handler ) {
        throw Exception( "[SDL] Could not get vkGetInstanceProcAddr: {}", SDL_GetError() );
    }

    volkInitializeCustom( handler );

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name( "RACECAR" )
        .set_app_version( 1, 0, 0 )
        .require_api_version( 1, 4 );

#if RACECAR_DEBUG
    if ( system_info.validation_layers_available ) {
        instance_builder.enable_validation_layers().use_default_debug_messenger();
    }
#endif

    uint32_t extension_count = 0;
    const char* const* inst_extensions = SDL_Vulkan_GetInstanceExtensions( &extension_count );

    if ( !inst_extensions ) {
        throw Exception(
            "[SDL] Could not get necessary Vulkan instance extensions: {}", SDL_GetError() );
    }

    std::vector<std::string_view> extensions;

    for ( uint32_t i = 0; i < extension_count; ++i ) {
        const char* extension = inst_extensions[i];
        extensions.push_back( extension );

        if ( system_info.is_extension_available( extension ) ) {
            instance_builder.enable_extension( extension );
        } else {
            throw Exception(
                "[vkb] Necessary Vulkan instance extension not available: {}", extension );
        }
    }

    {
        std::string message = "[Vulkan] Enabling necessary instance extensions: ";

        bool first = true;
        for ( std::string_view extension : extensions ) {
            if ( !first ) {
                message += ", ";
            }

            message += std::string( extension );
            first = false;
        }

        log::info( "{}", message );
    }

    vkb::Result<vkb::Instance> instance_ret = instance_builder.build();

    if ( !instance_ret ) {
        throw Exception(
            "[vkb] Failed to create Vulkan instance. Error: {}", instance_ret.error().message() );
    }

    return instance_ret.value();
}

/// Picks a physical device with a preference for a discrete GPU, and then creates a logical
/// device with one queue enabled from each available queue family.
vkb::Device pick_and_create_device( const Common& vulkan )
{
    vkb::PhysicalDeviceSelector phys_selector( vulkan.instance, vulkan.surface );

    VkPhysicalDeviceVulkan11Features required_features_11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features required_features_12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .shaderFloat16 = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features required_features_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures required_features = {
        .shaderInt16 = VK_TRUE,
    };

    vkb::Result<vkb::PhysicalDevice> phys_selector_ret
        = phys_selector.prefer_gpu_device_type( vkb::PreferredDeviceType::discrete )
              .allow_any_gpu_device_type( false )
              .set_minimum_version( 1, 3 )
              .add_required_extension( VK_KHR_SWAPCHAIN_EXTENSION_NAME )
              .add_required_extension( VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME )
              .set_required_features_13( required_features_13 )
              .set_required_features_12( required_features_12 )
              .set_required_features_11( required_features_11 )
              .set_required_features( required_features )
              .select();

    if ( !phys_selector_ret ) {
        auto phys_device_error
            = static_cast<vkb::PhysicalDeviceError>( phys_selector_ret.error().value() );

        // Handle no physical devices separately
        switch ( phys_device_error ) {
        case vkb::PhysicalDeviceError::no_physical_devices_found:
            throw Exception( "[vkb] No physical devices found" );

        default:
            throw Exception( "[vkb] Physical device selection failed because: {}",
                vkb::to_string( phys_device_error ) );
        }
    }

    vkb::PhysicalDevice& phys_device = phys_selector_ret.value();
    const std::string& phys_name = phys_device.name;
    vkb::DeviceBuilder device_builder( phys_device );
    vkb::Result<vkb::Device> device_ret = device_builder.build();

    if ( !device_ret ) {
        throw Exception(
            "[vkb] Failed to create VkDevice from VkPhysicalDevice named \"{}\"", phys_name );
    }

    vkb::Device& device = device_ret.value();

    if ( !device.get_queue( vkb::QueueType::graphics ) ) {
        throw Exception(
            "[vkb] VkDevice created from physical device \"{}\" does not have a graphics queue",
            phys_name );
    }

    if ( !device.get_queue( vkb::QueueType::present ) ) {
        throw Exception(
            "[vkb] VkDevice created from physical device \"{}\" does not have a present queue",
            phys_name );
    }

    log::info( "[Vulkan] Selected physical device: {}", phys_name );

    return device;
}

void initialize_vmaallocator( vk::Common& vulkan )
{
    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = vulkan.device.physical_device,
        .device = vulkan.device,
        .instance = vulkan.instance,
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };

    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    allocator_info.pVulkanFunctions = &vulkan_functions;

    check( vmaCreateAllocator( &allocator_info, &vulkan.allocator ),
        "[VMA] Failed to create global VMA allocator" );
}

} // namespace

Common initialize( SDL_Window* window )
{
    Common vulkan;

    try {
        vulkan.instance = create_instance();
        volkLoadInstance( vulkan.instance );

        if ( !SDL_Vulkan_CreateSurface( window, vulkan.instance, nullptr, &vulkan.surface ) ) {
            throw Exception( "[SDL] Could not create Vulkan surface: {}", SDL_GetError() );
        }

        vulkan.device = pick_and_create_device( vulkan );
        initialize_vmaallocator( vulkan );

        {
            vkb::Result<VkQueue> gfx_queue_res
                = vulkan.device.get_queue( vkb::QueueType::graphics );

            if ( !gfx_queue_res ) {
                throw Exception(
                    "[vkb] Failed to get graphics queue: {}", gfx_queue_res.error().message() );
            }

            vulkan.graphics_queue = std::move( gfx_queue_res.value() );
        }

        {
            vkb::Result<uint32_t> gfx_queue_family_res
                = vulkan.device.get_queue_index( vkb::QueueType::graphics );

            if ( !gfx_queue_family_res ) {
                throw Exception( "[vkb] Failed to get graphics queue family: {}",
                    gfx_queue_family_res.error().message() );
            }

            vulkan.graphics_queue_family = gfx_queue_family_res.value();
        }

        // Used by a lot of stuff
        {
            VkSamplerCreateInfo linear_sampler_info = vk::create::sampler_info( VK_FILTER_LINEAR );
            vk::check( vkCreateSampler( vulkan.device, &linear_sampler_info, nullptr,
                           &vulkan.global_samplers.linear_sampler ),
                "Failed to create global linear sampler" );
            vulkan.destructor_stack.push(
                vulkan.device, vulkan.global_samplers.linear_sampler, vkDestroySampler );

            VkSamplerCreateInfo nearest_sampler_info
                = vk::create::sampler_info( VK_FILTER_NEAREST );
            vk::check( vkCreateSampler( vulkan.device, &nearest_sampler_info, nullptr,
                           &vulkan.global_samplers.nearest_sampler ),
                "Failed to create global nearest sampler" );
            vulkan.destructor_stack.push(
                vulkan.device, vulkan.global_samplers.nearest_sampler, vkDestroySampler );
        }
    } catch ( const Exception& ex ) {
        log::error( "[vk] {}", ex.what() );
        throw Exception( "[Vulkan] Failed to initialize" );
    }

    log::info( "[Vulkan] Initialized!" );

    return vulkan;
}

void free( Common& vulkan )
{
    vmaDestroyAllocator( vulkan.allocator );
    vkb::destroy_device( vulkan.device );
    SDL_Vulkan_DestroySurface( vulkan.instance, vulkan.surface, nullptr );
    vkb::destroy_instance( vulkan.instance );
}

} // namespace racecar::vk

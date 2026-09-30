cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

option(RACECAR_ENABLE_ASAN "Build racecar with AddressSanitizer" OFF)
option(RACECAR_ENABLE_RAY_TRACING "Build racecar with ray tracing (ray query) support" ON)
option(RACECAR_ENABLE_DEV "Build racecar with development-only tooling" ON)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VK_DIR "${SRC_DIR}/vk")
set(ENGINE_DIR "${SRC_DIR}/engine")
set(POST_DIR "${ENGINE_DIR}/post")
set(GEOMETRY_DIR "${SRC_DIR}/geometry")
set(TERRAIN_DIR "${SRC_DIR}/terrain")
set(SCENE_DIR "${SRC_DIR}/scene")
set(RACECAR_DIR "${SRC_DIR}/racecar")
set(PASSES_DIR "${SRC_DIR}/passes")
set(DEMO_DIR "${SRC_DIR}/demo")
set(GAME_DIR "${SRC_DIR}/game")
set(EXEC_DIR "${SRC_DIR}/exec")

set(RACECAR_SOURCES
    ${SRC_DIR}/sdl.cpp
    ${SRC_DIR}/orbit_camera.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/stb.cpp
    ${SRC_DIR}/atmosphere.cpp
    ${SRC_DIR}/atmosphere_baker.cpp
    ${SRC_DIR}/sun_visibility.cpp
    ${SRC_DIR}/volumetrics.cpp
    ${SRC_DIR}/preset.cpp
    ${SRC_DIR}/deferred.cpp
    ${SRC_DIR}/app_setup.cpp
    ${SRC_DIR}/camera_data.cpp
    ${SRC_DIR}/gui_uniforms.cpp
    ${SRC_DIR}/lut_sets.cpp
    ${SRC_DIR}/window_events.cpp
    ${SRC_DIR}/gui_helpers.cpp
    ${SRC_DIR}/settings.cpp

    ${PASSES_DIR}/scene_pass.cpp
    ${PASSES_DIR}/reflection_pass.cpp
    ${PASSES_DIR}/reflection_mip_chain.cpp
    ${PASSES_DIR}/lighting_pass.cpp
    ${PASSES_DIR}/post_processing.cpp
    ${PASSES_DIR}/debug_texture_pass.cpp
    ${PASSES_DIR}/transparency_pass.cpp

    ${DEMO_DIR}/car_animation.cpp

    ${VK_DIR}/common.cpp
    ${VK_DIR}/create.cpp
    ${VK_DIR}/utility.cpp
    ${VK_DIR}/vma.cpp
    ${VK_DIR}/mem.cpp
    ${VK_DIR}/ray_tracing.cpp

    ${ENGINE_DIR}/execute.cpp
    ${ENGINE_DIR}/state.cpp
    ${ENGINE_DIR}/pipeline.cpp
    ${ENGINE_DIR}/draw_task.cpp
    ${ENGINE_DIR}/imm_submit.cpp
    ${ENGINE_DIR}/task_list.cpp
    ${ENGINE_DIR}/destructor_stack.cpp
    ${ENGINE_DIR}/descriptors.cpp
    ${ENGINE_DIR}/images.cpp
    ${ENGINE_DIR}/pipeline_barrier.cpp
    ${ENGINE_DIR}/rwimage.cpp
    ${ENGINE_DIR}/gfx_task.cpp
    ${ENGINE_DIR}/descriptor_set.cpp
    ${ENGINE_DIR}/compute_task.cpp
    ${ENGINE_DIR}/blit_task.cpp
    ${ENGINE_DIR}/precompute.cpp
    ${ENGINE_DIR}/prepass.cpp

    ${POST_DIR}/bloom.cpp
    ${POST_DIR}/ao.cpp
    ${POST_DIR}/tonemapping.cpp
    ${POST_DIR}/temporal_anti_aliasing.cpp

    ${GEOMETRY_DIR}/scene_mesh.cpp
    ${GEOMETRY_DIR}/procedural.cpp
    ${GEOMETRY_DIR}/quad.cpp
    ${GEOMETRY_DIR}/ibl.cpp
    ${GEOMETRY_DIR}/gpu_mesh_buffers.cpp

    ${TERRAIN_DIR}/terrain.cpp

    ${SCENE_DIR}/scene.cpp
    ${SCENE_DIR}/car_rt.cpp
    ${SCENE_DIR}/terrain_rt.cpp

    ${GAME_DIR}/gamestate.cpp

    ${RACECAR_DIR}/racecar.cpp
    ${RACECAR_DIR}/furnace.cpp
)

# Third party libraries not part of our vcpkg system
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(IMGUI_DIR "${THIRD_PARTY_DIR}/imgui")

add_library(imgui
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui.cpp
)

find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_package(Stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ImGui has to use volk, otherwise it won't find the correct function pointers
target_link_libraries(imgui PRIVATE volk::volk)

if (RACECAR_ENABLE_ASAN)
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
        OUTPUT_VARIABLE RACECAR_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "AddressSanitizer enabled (runtime: ${RACECAR_CLANG_RESOURCE_DIR}/lib/windows)")
endif()

# Enable RACECAR_MACOS macro if compiling on macOS
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # On macOS we have to explicitly link to the C++ Standard Library
    message(STATUS "Explicitly linking C++ Standard Library on macOS")
endif()

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

find_program(RACECAR_SLANGC NAMES slangc
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../slang/bin"
)

if (RACECAR_SLANGC)
    set(RACECAR_SLANGC_FLAGS
        -target spirv
        -profile spirv_1_5
        -emit-spirv-directly
        -g2
        -fvk-use-entrypoint-name
        -I "${SHADER_DIR}/atmosphere"
        -I "${SHADER_DIR}/clouds"
        -capability SPV_EXT_shader_atomic_float_add
        -Wno-41012
    )

    if (RACECAR_ENABLE_RAY_TRACING)
        list(APPEND RACECAR_SLANGC_FLAGS -DRACECAR_RAY_TRACING=1 -capability spvRayQueryKHR)
    else()
        list(APPEND RACECAR_SLANGC_FLAGS -DRACECAR_RAY_TRACING=0)
    endif()

    set(RACECAR_SHADER_OUTPUTS)

    function(racecar_add_shader OUTPUT SOURCE)
        set(OUTPUT_PATH "${SHADER_DIR}/${OUTPUT}")
        set(DEPFILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/shader_deps/${OUTPUT}.d")
        get_filename_component(DEPFILE_DIR "${DEPFILE_PATH}" DIRECTORY)
        file(MAKE_DIRECTORY "${DEPFILE_DIR}")

        set(ENTRY_ARGS)
        foreach(ENTRY IN LISTS ARGN)
            list(APPEND ENTRY_ARGS -entry ${ENTRY})
        endforeach()

        add_custom_command(
            OUTPUT "${OUTPUT_PATH}"
            COMMAND "${RACECAR_SLANGC}" "${SHADER_DIR}/${SOURCE}" ${RACECAR_SLANGC_FLAGS}
                    ${ENTRY_ARGS} -depfile "${DEPFILE_PATH}" -o "${OUTPUT_PATH}"
            DEPENDS "${SHADER_DIR}/${SOURCE}"
            DEPFILE "${DEPFILE_PATH}"
            WORKING_DIRECTORY "${SHADER_DIR}"
            COMMENT "Compiling shader ${OUTPUT}"
            VERBATIM
        )

        list(APPEND RACECAR_SHADER_OUTPUTS "${OUTPUT_PATH}")
        set(RACECAR_SHADER_OUTPUTS ${RACECAR_SHADER_OUTPUTS} PARENT_SCOPE)
    endfunction()

    racecar_add_shader(atmosphere/sky/atmosphere.spv atmosphere/sky/main.slang)
    racecar_add_shader(atmosphere/sky/bake_atmosphere.spv atmosphere/sky/bake_atmosphere.slang cs_bake_atmosphere)
    racecar_add_shader(atmosphere/sky/bake_atmosphere_mips.spv atmosphere/sky/bake_atmosphere.slang cs_bake_atmosphere_mips)

    racecar_add_shader(clouds/clouds.spv clouds/clouds.slang vs_main fs_main)
    racecar_add_shader(clouds/cs_generate_low_frequency.spv clouds/cloud_noise.slang cs_generate_low_frequency)
    racecar_add_shader(clouds/cs_generate_high_frequency.spv clouds/cloud_noise.slang cs_generate_high_frequency)
    racecar_add_shader(clouds/cs_generate_cumulus.spv clouds/cloud_noise.slang cs_generate_cumulus)
    racecar_add_shader(clouds/cloud_composite.spv clouds/cloud_composite.slang vs_main fs_main)

    racecar_add_shader(deferred/prepass.spv deferred/prepass.slang vs_main fs_main)
    racecar_add_shader(deferred/lighting.spv deferred/lighting.slang vs_main fs_main)
    racecar_add_shader(deferred/depth_prepass.spv deferred/depth_prepass.slang vs_main fs_main)
    racecar_add_shader(deferred/pp_test.spv deferred/pp_test.slang cs_pp_test)

    racecar_add_shader(glint/glint_noise_init.spv glint/glint_noise_init.slang cs_generate_glint_noise)

    racecar_add_shader(pbr/pbr.spv pbr/pbr.slang vs_main fs_main)

    racecar_add_shader(post/aa/aa.spv post/aa/aa.slang cs_main)
    racecar_add_shader(post/aa/history_aa.spv post/aa/aa.slang cs_write_history)
    racecar_add_shader(post/ao/ao.spv post/ao/ao.slang cs_ao)
    racecar_add_shader(post/bloom/threshold.spv post/bloom/threshold.slang threshold)
    racecar_add_shader(post/bloom/downsample.spv post/bloom/downsample.slang downsample)
    racecar_add_shader(post/bloom/upsample.spv post/bloom/upsample.slang upsample)
    racecar_add_shader(post/debug_texture/debug_texture.spv post/debug_texture/debug_texture.slang cs_debug_texture)
    racecar_add_shader(post/tonemapping/tonemapping.spv post/tonemapping/tonemapping.slang cs_main)

    racecar_add_shader(prefilter/irradiance.spv prefilter/irradiance.slang cs_compute_irradiance cs_compute_irradiance_sh)
    racecar_add_shader(prefilter/irradiance_sh.spv prefilter/irradiance.slang cs_compute_irradiance_sh)

    racecar_add_shader(raymarch/raymarch.spv raymarch/raymarch.slang vs_main fs_main)

    racecar_add_shader(raytracing/shadow.spv raytracing/shadow.slang vs_main fs_main)

    racecar_add_shader(reflections/reflections.spv reflections/reflections.slang vs_main fs_main)
    racecar_add_shader(reflections/reflection_mips.spv reflections/reflection_mips.slang cs_reflection_mips)
    racecar_add_shader(reflections/reflection_upsample.spv reflections/reflection_upsample.slang cs_reflection_upsample)

    racecar_add_shader(sun_visibility/sun_visibility.spv sun_visibility/sun_visibility.slang cs_sun_visibility)

    racecar_add_shader(terrain/cs_terrain_draw.spv terrain/terrain_lighting.slang cs_terrain_draw)
    racecar_add_shader(terrain/terrain_prepass.spv terrain/terrain_prepass.slang vs_main fs_main ts_control_main ts_eval_main)
    racecar_add_shader(terrain/terrain_rt_displace.spv terrain/terrain_rt_displace.slang cs_terrain_rt_displace)

    racecar_add_shader(transparency/transparency.spv transparency/transparency.slang vs_main fs_main)

    racecar_add_shader(world_pos_debug/world_pos_debug.spv world_pos_debug/world_pos_debug.slang vs_main fs_main)

    add_custom_target(shaders DEPENDS ${RACECAR_SHADER_OUTPUTS})
else()
    message(WARNING "slangc not found; shaders will not be compiled automatically")
endif()

function(racecar_configure_target TARGET)
    target_include_directories(${TARGET} PRIVATE ${SRC_DIR})

    target_compile_features(${TARGET} PRIVATE cxx_std_20)
    set_target_properties(${TARGET} PROPERTIES CXX_EXTENSIONS OFF)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${TARGET} PRIVATE RACECAR_DEBUG=1)
    else()
        target_compile_definitions(${TARGET} PRIVATE RACECAR_DEBUG=0)
    endif()

    if (RACECAR_ENABLE_RAY_TRACING)
        target_compile_definitions(${TARGET} PRIVATE RACECAR_RAY_TRACING=1)
    else()
        target_compile_definitions(${TARGET} PRIVATE RACECAR_RAY_TRACING=0)
    endif()

    if (RACECAR_ENABLE_DEV)
        target_compile_definitions(${TARGET} PRIVATE RACECAR_DEV=1)
    else()
        target_compile_definitions(${TARGET} PRIVATE RACECAR_DEV=0)
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${TARGET} PRIVATE /EHsc /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)
    else()
        target_compile_options(${TARGET} PRIVATE)
    endif()

    if (RACECAR_ENABLE_ASAN)
        target_compile_options(${TARGET} PRIVATE /fsanitize=address /Od)

        target_compile_definitions(${TARGET} PRIVATE
            _DISABLE_STRING_ANNOTATION=1
            _DISABLE_VECTOR_ANNOTATION=1
        )

        target_link_directories(${TARGET} PRIVATE
            "${RACECAR_CLANG_RESOURCE_DIR}/lib/windows"
        )
        target_link_libraries(${TARGET} PRIVATE
            clang_rt.asan_dynamic-x86_64
            clang_rt.asan_dynamic_runtime_thunk-x86_64
        )
        target_link_options(${TARGET} PRIVATE
            "/wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib"
            "/INCLUDE:__asan_seh_interceptor"
        )

        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RACECAR_CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll"
                "$<TARGET_FILE_DIR:${TARGET}>"
            COMMENT "Copying AddressSanitizer runtime DLL next to ${TARGET}"
        )
    endif()

    target_link_libraries(${TARGET} PRIVATE
        imgui
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        SDL3::SDL3
        vk-bootstrap::vk-bootstrap
        nlohmann_json::nlohmann_json
    )

    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(${TARGET} PRIVATE -lc++)
        target_compile_definitions(${TARGET} PRIVATE RACECAR_MACOS=1)
    else()
        target_compile_definitions(${TARGET} PRIVATE RACECAR_MACOS=0)
    endif()

    target_include_directories(${TARGET} PRIVATE
        ${IMGUI_DIR}
        ${TINYGLTF_INCLUDE_DIRS}
        ${Stb_INCLUDE_DIR}
    )

    if (TARGET shaders)
        add_dependencies(${TARGET} shaders)
    endif()
endfunction()

add_executable(racecar
    ${EXEC_DIR}/main.cpp
    ${RACECAR_SOURCES}
)
racecar_configure_target(racecar)

add_executable(furnace
    ${EXEC_DIR}/main_furnace.cpp
    ${RACECAR_SOURCES}
)
racecar_configure_target(furnace)
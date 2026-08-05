cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

option(RACECAR_ENABLE_ASAN "Build racecar with AddressSanitizer" OFF)
option(RACECAR_ENABLE_RAY_TRACING "Build racecar with ray tracing (ray query) support" ON)

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

add_executable(racecar
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/sdl.cpp
    ${SRC_DIR}/orbit_camera.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/stb.cpp
    ${SRC_DIR}/atmosphere.cpp
    ${SRC_DIR}/atmosphere_baker.cpp
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
    ${PASSES_DIR}/lighting_pass.cpp
    ${PASSES_DIR}/post_processing.cpp

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
    ${POST_DIR}/anti_aliasing.cpp

    ${GEOMETRY_DIR}/scene_mesh.cpp
    ${GEOMETRY_DIR}/procedural.cpp
    ${GEOMETRY_DIR}/quad.cpp
    ${GEOMETRY_DIR}/ibl.cpp
    ${GEOMETRY_DIR}/gpu_mesh_buffers.cpp

    ${TERRAIN_DIR}/terrain.cpp

    ${SCENE_DIR}/scene.cpp
    ${SCENE_DIR}/car_rt.cpp

    ${RACECAR_DIR}/racecar.cpp
)

target_include_directories(racecar PRIVATE ${SRC_DIR})

target_compile_features(racecar PRIVATE cxx_std_20)
set_target_properties(racecar PROPERTIES CXX_EXTENSIONS OFF)

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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(racecar PRIVATE RACECAR_DEBUG=1)
else()
    target_compile_definitions(racecar PRIVATE RACECAR_DEBUG=0)
endif()

if (RACECAR_ENABLE_RAY_TRACING)
    target_compile_definitions(racecar PRIVATE RACECAR_RAY_TRACING=1)
else()
    target_compile_definitions(racecar PRIVATE RACECAR_RAY_TRACING=0)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(racecar PRIVATE /EHsc /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)
else()
    target_compile_options(racecar PRIVATE)
endif()

if (RACECAR_ENABLE_ASAN)
    target_compile_options(racecar PRIVATE /fsanitize=address /Od)

    target_compile_definitions(racecar PRIVATE
        _DISABLE_STRING_ANNOTATION=1
        _DISABLE_VECTOR_ANNOTATION=1
    )

    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" --print-resource-dir
        OUTPUT_VARIABLE RACECAR_CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    target_link_directories(racecar PRIVATE
        "${RACECAR_CLANG_RESOURCE_DIR}/lib/windows"
    )
    target_link_libraries(racecar PRIVATE
        clang_rt.asan_dynamic-x86_64
        clang_rt.asan_dynamic_runtime_thunk-x86_64
    )
    target_link_options(racecar PRIVATE
        "/wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib"
        "/INCLUDE:__asan_seh_interceptor"
    )

    add_custom_command(TARGET racecar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RACECAR_CLANG_RESOURCE_DIR}/lib/windows/clang_rt.asan_dynamic-x86_64.dll"
            "$<TARGET_FILE_DIR:racecar>"
        COMMENT "Copying AddressSanitizer runtime DLL next to racecar.exe"
    )
    message(STATUS "AddressSanitizer enabled for racecar (runtime: ${RACECAR_CLANG_RESOURCE_DIR}/lib/windows)")
endif()

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

target_link_libraries(racecar PRIVATE
    imgui
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    SDL3::SDL3
    vk-bootstrap::vk-bootstrap
    nlohmann_json::nlohmann_json
)

# Enable RACECAR_MACOS macro if compiling on macOS
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # On macOS we have to explicitly link to the C++ Standard Library
    target_link_libraries(racecar PRIVATE -lc++)
    message(STATUS "Explicitly linking C++ Standard Library on macOS")
    target_compile_definitions(racecar PRIVATE RACECAR_MACOS=1)
else()
    target_compile_definitions(racecar PRIVATE RACECAR_MACOS=0)
endif()

target_include_directories(racecar PRIVATE
    ${IMGUI_DIR}
    ${TINYGLTF_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

if (WIN32)
    find_program(RACECAR_POWERSHELL NAMES powershell pwsh)

    if (RACECAR_POWERSHELL)
        file(GLOB_RECURSE RACECAR_SHADER_SOURCES CONFIGURE_DEPENDS
            "${SHADER_DIR}/*.slang"
            "${SHADER_DIR}/*.hlsl"
        )
        file(GLOB_RECURSE RACECAR_SHADER_SCRIPTS CONFIGURE_DEPENDS
            "${SHADER_DIR}/*.ps1"
        )

        set(RACECAR_SHADER_STAMP "${CMAKE_CURRENT_BINARY_DIR}/shaders.stamp")

        set(RACECAR_SHADER_SCRIPT_ARGS -File "${SHADER_DIR}/compile_all.ps1")

        if(RACECAR_ENABLE_RAY_TRACING)
            list(APPEND RACECAR_SHADER_SCRIPT_ARGS -RayTracing)
        endif()

        add_custom_command(
            OUTPUT ${RACECAR_SHADER_STAMP}
            COMMAND "${RACECAR_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                    ${RACECAR_SHADER_SCRIPT_ARGS}
            COMMAND ${CMAKE_COMMAND} -E touch "${RACECAR_SHADER_STAMP}"
            DEPENDS ${RACECAR_SHADER_SOURCES} ${RACECAR_SHADER_SCRIPTS}
            WORKING_DIRECTORY "${SHADER_DIR}"
            COMMENT "Compiling shaders (shaders/compile_all.ps1)"
            USES_TERMINAL
            VERBATIM
        )

        add_custom_target(shaders DEPENDS ${RACECAR_SHADER_STAMP})
        add_dependencies(racecar shaders)
    else()
        message(WARNING "PowerShell not found; shaders will not be compiled automatically")
    endif()
else()
    message(STATUS "Automatic shader compilation is Windows-only; run shaders/compile_all.ps1 manually")
endif()

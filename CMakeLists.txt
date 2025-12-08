cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VK_DIR "${SRC_DIR}/vk")
set(ENGINE_DIR "${SRC_DIR}/engine")
set(POST_DIR "${ENGINE_DIR}/post")
set(GEOMETRY_DIR "${SRC_DIR}/geometry")
set(TERRAIN_DIR "${SRC_DIR}/terrain")
set(SCENE_DIR "${SRC_DIR}/scene")

add_executable(racecar
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/racecar.cpp
    ${SRC_DIR}/sdl.cpp
    ${SRC_DIR}/orbit_camera.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/stb.cpp
    ${SRC_DIR}/atmosphere.cpp
    ${SRC_DIR}/atmosphere_baker.cpp
    ${SRC_DIR}/volumetrics.cpp
    ${SRC_DIR}/preset.cpp

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
)

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

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(racecar PRIVATE /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)
else()
    target_compile_options(racecar PRIVATE)
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

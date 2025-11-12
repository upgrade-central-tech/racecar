cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VK_DIR "${SRC_DIR}/vk")
set(ENGINE_DIR "${SRC_DIR}/engine")
set(GEOMETRY_DIR "${SRC_DIR}/geometry")
set(SCENE_DIR "${SRC_DIR}/scene")

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(IMGUI_DIR "${THIRD_PARTY_DIR}/imgui")

add_executable(racecar
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/sdl.cpp
  ${SRC_DIR}/orbit_camera.cpp

  ${VK_DIR}/common.cpp
  ${VK_DIR}/create.cpp
  ${VK_DIR}/utility.cpp
  ${VK_DIR}/vma.cpp
  ${VK_DIR}/mem.cpp

  ${ENGINE_DIR}/execute.cpp
  ${ENGINE_DIR}/state.cpp
  ${ENGINE_DIR}/pipeline.cpp
  ${ENGINE_DIR}/draw_task.cpp
  ${ENGINE_DIR}/imm_submit.cpp
  ${ENGINE_DIR}/task_list.cpp
  ${ENGINE_DIR}/destructor_stack.cpp
  ${ENGINE_DIR}/descriptors.cpp
  ${ENGINE_DIR}/images.cpp
  ${ENGINE_DIR}/gui.cpp
  ${ENGINE_DIR}/pipeline_barrier.cpp

  ${GEOMETRY_DIR}/mesh.cpp

  ${SCENE_DIR}/scene.cpp
)

add_library( imgui
  # Third party libraries not part of our vcpkg system
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_impl_sdl3.cpp
  ${IMGUI_DIR}/imgui_impl_vulkan.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui.cpp
)

# Treat ImGui as a third party library, has to be #included with angle brackets (<>)
target_include_directories(racecar PRIVATE "${IMGUI_DIR}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(racecar PRIVATE DEBUG=1)
else()
  target_compile_definitions(racecar PRIVATE DEBUG=0)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(racecar PRIVATE /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)
else()
  target_compile_options(racecar PRIVATE /W4 /WX)
endif()

find_package(volk CONFIG REQUIRED)
target_link_libraries(imgui PRIVATE volk::volk)

target_link_libraries(racecar PRIVATE imgui)

find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE volk::volk GPUOpen::VulkanMemoryAllocator)

target_link_libraries(racecar PRIVATE volk::volk)

find_package(glm CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE glm::glm)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE SDL3::SDL3)

find_package(vk-bootstrap CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE vk-bootstrap::vk-bootstrap)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(racecar PRIVATE ${TINYGLTF_INCLUDE_DIRS})

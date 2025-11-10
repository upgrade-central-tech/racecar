cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

set(SRC_DIR "src")
set(VK_DIR "${SRC_DIR}/vk")
set(ENGINE_DIR "${SRC_DIR}/engine")
set(GEOMETRY_DIR "${SRC_DIR}/geometry")
set(SCENE_DIR "${SRC_DIR}/scene")

add_executable(racecar
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/sdl.cpp

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
  ${ENGINE_DIR}/gui.cpp

  ${GEOMETRY_DIR}/mesh.cpp

  ${SCENE_DIR}/scene.cpp
  ${SCENE_DIR}/camera.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(racecar PRIVATE DEBUG=1)
else()
  target_compile_definitions(racecar PRIVATE DEBUG=0)
endif()

if (MSVC)
  target_compile_options(racecar PRIVATE /W4 /WX -Wno-missing-designated-field-initializers)
else()
  target_compile_options(racecar PRIVATE /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)
endif()

find_package(VulkanMemoryAllocator CONFIG REQUIRED)


find_package(volk CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE volk::volk)

target_link_libraries(racecar PRIVATE volk::volk GPUOpen::VulkanMemoryAllocator)

find_package(glm CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE glm::glm)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE SDL3::SDL3)

find_package(vk-bootstrap CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE vk-bootstrap::vk-bootstrap)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(racecar PRIVATE ${TINYGLTF_INCLUDE_DIRS})
find_package(imgui CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE imgui::imgui)

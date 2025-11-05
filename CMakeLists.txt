cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

set(SRC_DIR "src")
set(VK_DIR "${SRC_DIR}/vk")

add_executable(racecar
  ${SRC_DIR}/main.cpp

  ${SRC_DIR}/sdl.cpp
  ${SRC_DIR}/renderer.cpp

  ${VK_DIR}/common.cpp
  ${VK_DIR}/create.cpp
  ${VK_DIR}/utility.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(racecar PRIVATE DEBUG=1)
else()
  target_compile_definitions(racecar PRIVATE DEBUG=0)
endif()

target_compile_options(racecar PRIVATE /W4 /WX -Wconversion -fcolor-diagnostics -Wno-missing-designated-field-initializers)

find_package(glm CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE glm::glm)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE SDL3::SDL3)

find_package(volk CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE volk::volk)

find_package(vk-bootstrap CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE vk-bootstrap::vk-bootstrap)

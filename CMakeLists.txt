cmake_minimum_required(VERSION 4.0)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

set(SRC_DIR "src")

add_executable(racecar
  ${SRC_DIR}/main.cpp
)

target_compile_options(racecar PRIVATE /W4 /WX -Wconversion -fcolor-diagnostics)

find_package(glm CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE glm::glm)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE SDL3::SDL3)

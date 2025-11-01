cmake_minimum_required(VERSION 4.0..4.1)

project(racecar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

add_executable(racecar src/main.cpp)

find_package(glm CONFIG REQUIRED)
target_link_libraries(racecar PRIVATE glm::glm)

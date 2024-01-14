cmake_minimum_required(VERSION 3.20)

project(Satania)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(satania
    "src/aabb.hpp"
    "src/bvh.hpp"
    "src/camera.cpp"
    "src/camera.h"
    "src/main.cpp"
    "src/mca.hpp"
    "src/mesh.hpp"
    "src/nbt.hpp"
    "src/timer.hpp"
    "src/voxelizer.hpp"
)

target_link_libraries(satania PRIVATE
    glm::glm
    glfw
    glad::glad
    assimp::assimp
    ZLIB::ZLIB
)

ADD_CUSTOM_TARGET(link_target ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data)
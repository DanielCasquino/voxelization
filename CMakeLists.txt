cmake_minimum_required(VERSION 3.10.0)
project(voxelization VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(assimp QUIET)
find_package(OpenMP REQUIRED)

add_executable(voxelization main.cpp src/voxelize.cpp)

target_link_libraries(voxelization
    PRIVATE
    MPI::MPI_CXX
    OpenMP::OpenMP_CXX
)

if(assimp_FOUND)
    target_link_libraries(voxelization PRIVATE assimp::assimp)
    target_compile_definitions(voxelization PRIVATE VOXELIZATION_USE_ASSIMP=1)
endif()

target_include_directories(voxelization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

cmake_minimum_required(VERSION 3.10.0)
project(voxelization VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(voxelization main.cpp src/voxelize.cpp)

target_link_libraries(voxelization
    PRIVATE
    MPI::MPI_CXX
    assimp::assimp
    OpenMP::OpenMP_CXX
)

target_include_directories(voxelization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
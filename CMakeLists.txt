cmake_minimum_required(VERSION 3.18.0)
project(voxelization VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
    set(CMAKE_CUDA_ARCHITECTURES 86)
endif()

include(CheckLanguage)

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

option(VOXELIZATION_ENABLE_CUDA "Build the cuda_atomic mode when nvcc is available" ON)
if(VOXELIZATION_ENABLE_CUDA)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        target_sources(voxelization PRIVATE src/voxelize_cuda.cu)
        target_compile_definitions(voxelization PRIVATE VOXELIZATION_USE_CUDA=1)
        set_source_files_properties(src/voxelize_cuda.cu PROPERTIES LANGUAGE CUDA)
        set_target_properties(voxelization PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
    else()
        message(STATUS "CUDA compiler not found; cuda_atomic mode will be unavailable")
    endif()
endif()

target_include_directories(voxelization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

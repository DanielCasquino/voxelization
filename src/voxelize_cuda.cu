#include "voxelize.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include <cuda_runtime.h>

namespace
{
    __device__ int ClampIndexDevice(float value, float min_value, float cell_size, int resolution)
    {
        int index = static_cast<int>(floorf((value - min_value) / cell_size));
        if (index < 0)
            return 0;
        if (index >= resolution)
            return resolution - 1;
        return index;
    }

    __global__ void VoxelizeAABBKernel(const Triangle *triangles,
                                       uint64_t triangle_count,
                                       int resolution,
                                       Bounds bounds,
                                       uint64_t *grid,
                                       uint64_t *tested_voxels)
    {
        const uint64_t triangle_index = static_cast<uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
        if (triangle_index >= triangle_count)
            return;

        const Triangle triangle = triangles[triangle_index];
        float cell_size[3];
        for (int axis = 0; axis < 3; ++axis)
            cell_size[axis] = (bounds.max[axis] - bounds.min[axis]) / static_cast<float>(resolution);

        float tri_min[3] = {triangle.v[0][0], triangle.v[0][1], triangle.v[0][2]};
        float tri_max[3] = {triangle.v[0][0], triangle.v[0][1], triangle.v[0][2]};
        for (int vertex = 1; vertex < 3; ++vertex)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                tri_min[axis] = fminf(tri_min[axis], triangle.v[vertex][axis]);
                tri_max[axis] = fmaxf(tri_max[axis], triangle.v[vertex][axis]);
            }
        }

        const int x0 = ClampIndexDevice(tri_min[0], bounds.min[0], cell_size[0], resolution);
        const int x1 = ClampIndexDevice(tri_max[0], bounds.min[0], cell_size[0], resolution);
        const int y0 = ClampIndexDevice(tri_min[1], bounds.min[1], cell_size[1], resolution);
        const int y1 = ClampIndexDevice(tri_max[1], bounds.min[1], cell_size[1], resolution);
        const int z0 = ClampIndexDevice(tri_min[2], bounds.min[2], cell_size[2], resolution);
        const int z1 = ClampIndexDevice(tri_max[2], bounds.min[2], cell_size[2], resolution);

        uint64_t local_tested = 0;
        for (int z = z0; z <= z1; ++z)
        {
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const uint64_t voxel_index = static_cast<uint64_t>(z) * resolution * resolution +
                                                 static_cast<uint64_t>(y) * resolution +
                                                 static_cast<uint64_t>(x);
                    const uint64_t mask = uint64_t{1} << (voxel_index % 64);
                    atomicOr(reinterpret_cast<unsigned long long *>(&grid[voxel_index / 64]),
                             static_cast<unsigned long long>(mask));
                    ++local_tested;
                }
            }
        }
        atomicAdd(reinterpret_cast<unsigned long long *>(tested_voxels),
                  static_cast<unsigned long long>(local_tested));
    }

    void CheckCuda(cudaError_t status, const char *operation)
    {
        if (status != cudaSuccess)
        {
            throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
        }
    }

    uint64_t CountBits(const std::vector<uint64_t> &bits)
    {
        uint64_t total = 0;
        for (uint64_t word : bits)
            total += static_cast<uint64_t>(__builtin_popcountll(word));
        return total;
    }
}

std::vector<uint64_t> VoxelizeCUDAAtomic(const std::vector<Triangle> &triangles,
                                         int resolution,
                                         const Bounds &bounds,
                                         VoxelStats &stats)
{
    const uint64_t triangle_count = static_cast<uint64_t>(triangles.size());
    const uint64_t voxel_count = static_cast<uint64_t>(resolution) * resolution * resolution;
    const size_t word_count = static_cast<size_t>((voxel_count + 63) / 64);
    std::vector<uint64_t> grid(word_count, 0);

    Triangle *device_triangles = nullptr;
    uint64_t *device_grid = nullptr;
    uint64_t *device_tested = nullptr;

    try
    {
        CheckCuda(cudaMalloc(&device_triangles, triangles.size() * sizeof(Triangle)), "cudaMalloc triangles");
        CheckCuda(cudaMalloc(&device_grid, word_count * sizeof(uint64_t)), "cudaMalloc grid");
        CheckCuda(cudaMalloc(&device_tested, sizeof(uint64_t)), "cudaMalloc tested");
        CheckCuda(cudaMemcpy(device_triangles, triangles.data(), triangles.size() * sizeof(Triangle), cudaMemcpyHostToDevice),
                  "cudaMemcpy triangles");
        CheckCuda(cudaMemset(device_grid, 0, word_count * sizeof(uint64_t)), "cudaMemset grid");
        CheckCuda(cudaMemset(device_tested, 0, sizeof(uint64_t)), "cudaMemset tested");

        const int block_size = 128;
        const int block_count = static_cast<int>((triangle_count + block_size - 1) / block_size);
        VoxelizeAABBKernel<<<block_count, block_size>>>(device_triangles,
                                                        triangle_count,
                                                        resolution,
                                                        bounds,
                                                        device_grid,
                                                        device_tested);
        CheckCuda(cudaGetLastError(), "launch VoxelizeAABBKernel");
        CheckCuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

        CheckCuda(cudaMemcpy(grid.data(), device_grid, word_count * sizeof(uint64_t), cudaMemcpyDeviceToHost),
                  "cudaMemcpy grid");
        CheckCuda(cudaMemcpy(&stats.tested_voxels, device_tested, sizeof(uint64_t), cudaMemcpyDeviceToHost),
                  "cudaMemcpy tested");
    }
    catch (...)
    {
        cudaFree(device_triangles);
        cudaFree(device_grid);
        cudaFree(device_tested);
        throw;
    }

    cudaFree(device_triangles);
    cudaFree(device_grid);
    cudaFree(device_tested);

    stats.estimated_flops = stats.tested_voxels * 3;
    stats.occupied_voxels = CountBits(grid);
    return grid;
}

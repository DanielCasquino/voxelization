#include "voxelize.h"

#include <algorithm>
#include <cmath>
#include <limits>
#if defined(_OPENMP) && __has_include(<omp.h>)
#define VOXELIZATION_HAS_OPENMP_HEADER 1
#include <omp.h>
#endif

namespace
{
    int OpenMPMaxThreads()
    {
#ifdef VOXELIZATION_HAS_OPENMP_HEADER
        return omp_get_max_threads();
#else
        return 1;
#endif
    }

    int OpenMPThreadNum()
    {
#ifdef VOXELIZATION_HAS_OPENMP_HEADER
        return omp_get_thread_num();
#else
        return 0;
#endif
    }

    int ClampIndex(float value, float min_value, float cell_size, int resolution)
    {
        int index = static_cast<int>(std::floor((value - min_value) / cell_size));
        return std::clamp(index, 0, resolution - 1);
    }

    void SetBit(std::vector<uint64_t> &bits, uint64_t index)
    {
        bits[index / 64] |= (uint64_t{1} << (index % 64));
    }

    void AtomicSetBit(std::vector<uint64_t> &bits, uint64_t index)
    {
        const uint64_t mask = uint64_t{1} << (index % 64);
        uint64_t &word = bits[index / 64];
#pragma omp atomic update
        word |= mask;
    }

    uint64_t CountBits(const std::vector<uint64_t> &bits)
    {
        uint64_t total = 0;
        for (uint64_t word : bits)
            total += static_cast<uint64_t>(__builtin_popcountll(word));
        return total;
    }

    void ComputeCellSize(const Bounds &bounds, int resolution, float cell_size[3])
    {
        for (int axis = 0; axis < 3; ++axis)
            cell_size[axis] = (bounds.max[axis] - bounds.min[axis]) / static_cast<float>(resolution);
    }

    /// @brief Visits triangle's AABB, calls an abstract function to mark voxels
    /// @tparam MarkVoxel
    /// @param triangle
    /// @param resolution
    /// @param bounds
    /// @param cell_size
    /// @param mark_voxel
    /// @param tested_voxels
    /// @param estimated_flops
    template <typename MarkVoxel>
    void VisitTriangleAABB(const Triangle &triangle,
                           int resolution,
                           const Bounds &bounds,
                           const float cell_size[3],
                           MarkVoxel mark_voxel,
                           uint64_t &tested_voxels,
                           uint64_t &estimated_flops)
    {
        float tri_min[3] = {triangle.v[0][0], triangle.v[0][1], triangle.v[0][2]};
        float tri_max[3] = {triangle.v[0][0], triangle.v[0][1], triangle.v[0][2]};

        for (int vertex = 1; vertex < 3; ++vertex)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                tri_min[axis] = std::min(tri_min[axis], triangle.v[vertex][axis]);
                tri_max[axis] = std::max(tri_max[axis], triangle.v[vertex][axis]);
            }
        }

        const int x0 = ClampIndex(tri_min[0], bounds.min[0], cell_size[0], resolution);
        const int x1 = ClampIndex(tri_max[0], bounds.min[0], cell_size[0], resolution);
        const int y0 = ClampIndex(tri_min[1], bounds.min[1], cell_size[1], resolution);
        const int y1 = ClampIndex(tri_max[1], bounds.min[1], cell_size[1], resolution);
        const int z0 = ClampIndex(tri_min[2], bounds.min[2], cell_size[2], resolution);
        const int z1 = ClampIndex(tri_max[2], bounds.min[2], cell_size[2], resolution);

        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                {
                    const uint64_t index = static_cast<uint64_t>(z) * resolution * resolution +
                                           static_cast<uint64_t>(y) * resolution +
                                           static_cast<uint64_t>(x);
                    mark_voxel(index);
                    ++tested_voxels;
                    estimated_flops += 3;
                }
    }
}

/// @brief Compute the axis-aligned bounding box of all input triangles.
/// @param triangles
/// @return 3D bounding box for triangles
Bounds ComputeBounds(const std::vector<Triangle> &triangles)
{
    Bounds bounds{};
    for (int axis = 0; axis < 3; ++axis)
    {
        bounds.min[axis] = std::numeric_limits<float>::max();
        bounds.max[axis] = std::numeric_limits<float>::lowest();
    }

    for (const Triangle &triangle : triangles)
    {
        for (int vertex = 0; vertex < 3; ++vertex)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                bounds.min[axis] = std::min(bounds.min[axis], triangle.v[vertex][axis]);
                bounds.max[axis] = std::max(bounds.max[axis], triangle.v[vertex][axis]);
            }
        }
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        if (bounds.max[axis] <= bounds.min[axis])
            bounds.max[axis] = bounds.min[axis] + 1.0f;
    }

    return bounds;
}

std::vector<uint64_t> Voxelize(const std::vector<Triangle> &triangles,
                               int resolution,
                               const Bounds &bounds,
                               VoxelStats &stats)
{
    const uint64_t voxel_count = static_cast<uint64_t>(resolution) * resolution * resolution;
    std::vector<uint64_t> local_grid((voxel_count + 63) / 64, 0);

    float cell_size[3];
    ComputeCellSize(bounds, resolution, cell_size);

    for (const Triangle &triangle : triangles)
    {
        VisitTriangleAABB(triangle, resolution, bounds, cell_size, [&](uint64_t index)
                          { SetBit(local_grid, index); }, stats.tested_voxels, stats.estimated_flops);
    }

    stats.occupied_voxels = CountBits(local_grid);
    return local_grid;
}

/// @brief Voxelizes with OMP but each thread has its own grid
/// @param triangles
/// @param resolution
/// @param bounds
/// @param stats
/// @param requested_threads
/// @return Final bit-packed voxel grid.
std::vector<uint64_t> VoxelizeOMPPrivate(const std::vector<Triangle> &triangles,
                                         int resolution,
                                         const Bounds &bounds,
                                         VoxelStats &stats,
                                         int requested_threads)
{
    const uint64_t voxel_count = static_cast<uint64_t>(resolution) * resolution * resolution;
    const size_t word_count = static_cast<size_t>((voxel_count + 63) / 64);
    const int thread_count = requested_threads > 0 ? requested_threads : OpenMPMaxThreads();
    std::vector<std::vector<uint64_t>> private_grids(thread_count, std::vector<uint64_t>(word_count, 0));

    float cell_size[3];
    ComputeCellSize(bounds, resolution, cell_size);

    uint64_t tested_voxels = 0;
    uint64_t estimated_flops = 0;

#pragma omp parallel num_threads(thread_count) reduction(+ : tested_voxels, estimated_flops)
    {
        const int tid = OpenMPThreadNum();
        std::vector<uint64_t> &grid = private_grids[tid];

#pragma omp for schedule(static)
        for (size_t i = 0; i < triangles.size(); ++i)
        {
            VisitTriangleAABB(triangles[i], resolution, bounds, cell_size, [&](uint64_t index)
                              { SetBit(grid, index); }, tested_voxels, estimated_flops);
        }
    }

    std::vector<uint64_t> global_grid(word_count, 0);
    for (const std::vector<uint64_t> &grid : private_grids)
    {
        for (size_t word = 0; word < word_count; ++word)
            global_grid[word] |= grid[word];
    }

    stats.tested_voxels = tested_voxels;
    stats.estimated_flops = estimated_flops;
    stats.occupied_voxels = CountBits(global_grid);
    return global_grid;
}

/// @brief Voxelized with OMP but the global grid is shared, uses atomic
/// @param triangles
/// @param resolution
/// @param bounds
/// @param stats
/// @param requested_threads
/// @return Final bit-packed voxel grid.
std::vector<uint64_t> VoxelizeOMPAtomic(const std::vector<Triangle> &triangles,
                                        int resolution,
                                        const Bounds &bounds,
                                        VoxelStats &stats,
                                        int requested_threads)
{
    const uint64_t voxel_count = static_cast<uint64_t>(resolution) * resolution * resolution;
    std::vector<uint64_t> global_grid((voxel_count + 63) / 64, 0);
    const int thread_count = requested_threads > 0 ? requested_threads : OpenMPMaxThreads();

    float cell_size[3];
    ComputeCellSize(bounds, resolution, cell_size);

    uint64_t tested_voxels = 0;
    uint64_t estimated_flops = 0;

#pragma omp parallel for num_threads(thread_count) schedule(static) reduction(+ : tested_voxels, estimated_flops)
    for (size_t i = 0; i < triangles.size(); ++i)
    {
        VisitTriangleAABB(triangles[i], resolution, bounds, cell_size, [&](uint64_t index)
                          { AtomicSetBit(global_grid, index); }, tested_voxels, estimated_flops);
    }

    stats.tested_voxels = tested_voxels;
    stats.estimated_flops = estimated_flops;
    stats.occupied_voxels = CountBits(global_grid);
    return global_grid;
}

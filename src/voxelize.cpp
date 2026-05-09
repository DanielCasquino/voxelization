#include "voxelize.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
int ClampIndex(float value, float min_value, float cell_size, int resolution)
{
    int index = static_cast<int>(std::floor((value - min_value) / cell_size));
    return std::clamp(index, 0, resolution - 1);
}

void SetBit(std::vector<uint64_t> &bits, uint64_t index)
{
    bits[index / 64] |= (uint64_t{1} << (index % 64));
}

uint64_t CountBits(const std::vector<uint64_t> &bits)
{
    uint64_t total = 0;
    for (uint64_t word : bits)
        total += static_cast<uint64_t>(__builtin_popcountll(word));
    return total;
}
}

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
    for (int axis = 0; axis < 3; ++axis)
        cell_size[axis] = (bounds.max[axis] - bounds.min[axis]) / static_cast<float>(resolution);

    for (const Triangle &triangle : triangles)
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
                    SetBit(local_grid, index);
                    ++stats.tested_voxels;
                    stats.estimated_flops += 3;
                }
    }

    stats.occupied_voxels = CountBits(local_grid);
    return local_grid;
}

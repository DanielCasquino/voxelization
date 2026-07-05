#include <cstdint>
#include <vector>

struct Triangle
{
    float v[3][3]; // each row is a vertex, cols is x,y,z
};

struct Bounds
{
    float min[3];
    float max[3];
};

struct VoxelStats
{
    uint64_t occupied_voxels = 0;
    uint64_t tested_voxels = 0;
    uint64_t estimated_flops = 0;
};

Bounds ComputeBounds(const std::vector<Triangle> &triangles);

std::vector<uint64_t> Voxelize(const std::vector<Triangle> &triangles,
                               int resolution,
                               const Bounds &bounds,
                               VoxelStats &stats);

std::vector<uint64_t> VoxelizeOMPPrivate(const std::vector<Triangle> &triangles,
                                          int resolution,
                                          const Bounds &bounds,
                                          VoxelStats &stats,
                                          int requested_threads);

std::vector<uint64_t> VoxelizeOMPAtomic(const std::vector<Triangle> &triangles,
                                         int resolution,
                                         const Bounds &bounds,
                                         VoxelStats &stats,
                                         int requested_threads);

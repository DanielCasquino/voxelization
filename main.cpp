#include <mpi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>
#include "voxelize.h"

namespace
{
MPI_Datatype CreateTriangleType()
{
    MPI_Datatype triangle_type;
    MPI_Type_contiguous(9, MPI_FLOAT, &triangle_type);
    MPI_Type_commit(&triangle_type);
    return triangle_type;
}

MPI_Datatype CreateBoundsType()
{
    MPI_Datatype bounds_type;
    MPI_Type_contiguous(6, MPI_FLOAT, &bounds_type);
    MPI_Type_commit(&bounds_type);
    return bounds_type;
}

std::vector<int> BuildCounts(uint64_t total, int size)
{
    std::vector<int> counts(size, 0);
    for (int rank = 0; rank < size; ++rank)
    {
        const uint64_t start = (total * static_cast<uint64_t>(rank)) / static_cast<uint64_t>(size);
        const uint64_t end = (total * static_cast<uint64_t>(rank + 1)) / static_cast<uint64_t>(size);
        counts[rank] = static_cast<int>(end - start);
    }
    return counts;
}

std::vector<int> BuildDisplacements(const std::vector<int> &counts)
{
    std::vector<int> displacements(counts.size(), 0);
    for (size_t i = 1; i < counts.size(); ++i)
        displacements[i] = displacements[i - 1] + counts[i - 1];
    return displacements;
}
}

int main(int argc, char *argv[])
{
    int rank, size, ierr;

    ierr = MPI_Init(&argc, &argv);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    ierr = MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    if (argc < 2)
    {
        if (rank == 0)
            std::fprintf(stderr, "Usage: %s <mesh-file> [resolution]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    const int resolution = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 64;
    std::vector<Triangle> triangles;
    std::vector<Triangle> local_triangles;
    Bounds bounds{};
    double load_seconds = 0.0;
    MPI_Datatype triangle_type = CreateTriangleType();
    MPI_Datatype bounds_type = CreateBoundsType();

    if (rank == 0)
    {
        const double load_start = MPI_Wtime();
        Assimp::Importer imp;
        const aiScene *scene = imp.ReadFile(argv[1], aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
        if (!scene || !scene->HasMeshes())
        {
            std::fprintf(stderr, "Assimp failed to read mesh: %s\n", imp.GetErrorString());
            MPI_Abort(MPI_COMM_WORLD, 2);
        }

        const aiMesh *mesh = scene->mMeshes[0];
        triangles.resize(mesh->mNumFaces);

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            if (mesh->mFaces[i].mNumIndices != 3)
                continue;

            for (unsigned int j = 0; j < 3; ++j)
            {
                const aiVector3D vertex = mesh->mVertices[mesh->mFaces[i].mIndices[j]];
                triangles[i].v[j][0] = vertex.x;
                triangles[i].v[j][1] = vertex.y;
                triangles[i].v[j][2] = vertex.z;
            }
        }

        bounds = ComputeBounds(triangles);
        load_seconds = MPI_Wtime() - load_start;
    }

    uint64_t triangle_count = triangles.size();
    MPI_Bcast(&triangle_count, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    std::vector<int> counts = BuildCounts(triangle_count, size);
    std::vector<int> displacements = BuildDisplacements(counts);
    local_triangles.resize(counts[rank]);

    MPI_Scatterv(rank == 0 ? triangles.data() : nullptr,
                 counts.data(),
                 displacements.data(),
                 triangle_type,
                 local_triangles.data(),
                 counts[rank],
                 triangle_type,
                 0,
                 MPI_COMM_WORLD);
    MPI_Bcast(&bounds, 1, bounds_type, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    const double voxel_start = MPI_Wtime();

    VoxelStats local_stats{};
    std::vector<uint64_t> local_grid = Voxelize(local_triangles, resolution, bounds, local_stats);
    std::vector<uint64_t> global_grid(local_grid.size(), 0);

    MPI_Reduce(local_grid.data(),
               global_grid.data(),
               static_cast<int>(local_grid.size()),
               MPI_UINT64_T,
               MPI_BOR,
               0,
               MPI_COMM_WORLD);

    uint64_t total_tested_voxels = 0;
    uint64_t total_estimated_flops = 0;
    MPI_Reduce(&local_stats.tested_voxels, &total_tested_voxels, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_stats.estimated_flops, &total_estimated_flops, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    const double voxel_seconds = MPI_Wtime() - voxel_start;
    double max_voxel_seconds = 0.0;
    MPI_Reduce(&voxel_seconds, &max_voxel_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        uint64_t occupied = 0;
        for (uint64_t word : global_grid)
            occupied += static_cast<uint64_t>(__builtin_popcountll(word));

        const double flops_per_second = max_voxel_seconds > 0.0
                                            ? static_cast<double>(total_estimated_flops) / max_voxel_seconds
                                            : 0.0;

        std::printf("mesh=%s\n", argv[1]);
        std::printf("triangles=%llu\n", static_cast<unsigned long long>(triangle_count));
        std::printf("processes=%d\n", size);
        std::printf("resolution=%d\n", resolution);
        std::printf("partition=min:%d max:%d\n",
                    *std::min_element(counts.begin(), counts.end()),
                    *std::max_element(counts.begin(), counts.end()));
        std::printf("occupied_voxels=%llu\n", static_cast<unsigned long long>(occupied));
        std::printf("tested_voxels=%llu\n", static_cast<unsigned long long>(total_tested_voxels));
        std::printf("estimated_flops=%llu\n", static_cast<unsigned long long>(total_estimated_flops));
        std::printf("load_seconds=%.6f\n", load_seconds);
        std::printf("voxel_seconds=%.6f\n", max_voxel_seconds);
        std::printf("estimated_flops_per_second=%.2f\n", flops_per_second);
    }

    MPI_Type_free(&triangle_type);
    MPI_Type_free(&bounds_type);

    ierr = MPI_Finalize();
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    return 0;
}

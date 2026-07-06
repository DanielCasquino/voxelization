#include <mpi.h>
#ifdef VOXELIZATION_USE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
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

    bool TestBit(const std::vector<uint64_t> &bits, uint64_t index)
    {
        return (bits[index / 64] & (uint64_t{1} << (index % 64))) != 0;
    }

    struct Options
    {
        std::string mesh_path;
        int resolution = 64;
        std::string projection_path;
        std::string mode = "mpi";
        int threads = 0;
    };

    bool ParseOptions(int argc, char *argv[], Options &options)
    {
        std::vector<std::string> positional;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg.rfind("--mode=", 0) == 0)
                options.mode = arg.substr(7);
            else if (arg == "--mode" && i + 1 < argc)
                options.mode = argv[++i];
            else if (arg.rfind("--threads=", 0) == 0)
                options.threads = std::atoi(arg.substr(10).c_str());
            else if (arg == "--threads" && i + 1 < argc)
                options.threads = std::atoi(argv[++i]);
            else
                positional.push_back(arg);
        }

        if (positional.empty())
            return false;

        options.mesh_path = positional[0];
        if (positional.size() >= 2)
            options.resolution = std::max(1, std::atoi(positional[1].c_str()));
        if (positional.size() >= 3)
            options.projection_path = positional[2];

        if (options.mode == "mpi" || options.mode == "omp_private" || options.mode == "omp_atomic")
            return true;
#ifdef VOXELIZATION_USE_CUDA
        return options.mode == "cuda_atomic";
#else
        return false;
#endif
    }

    int ParseObjIndex(const std::string &token)
    {
        const size_t slash = token.find('/');
        const std::string head = slash == std::string::npos ? token : token.substr(0, slash);
        return std::atoi(head.c_str()) - 1;
    }

    bool LoadObjMesh(const std::string &path, std::vector<Triangle> &triangles, std::string &error)
    {
        std::ifstream input(path);
        if (!input)
        {
            error = "could not open OBJ file";
            return false;
        }

        std::vector<std::array<float, 3>> vertices;
        std::string line;
        while (std::getline(input, line))
        {
            std::istringstream stream(line);
            std::string tag;
            stream >> tag;
            if (tag == "v")
            {
                std::array<float, 3> vertex{};
                stream >> vertex[0] >> vertex[1] >> vertex[2];
                vertices.push_back(vertex);
            }
            else if (tag == "f")
            {
                std::vector<int> indices;
                std::string token;
                while (stream >> token)
                    indices.push_back(ParseObjIndex(token));

                if (indices.size() < 3)
                    continue;

                for (size_t i = 1; i + 1 < indices.size(); ++i)
                {
                    const int face_indices[3] = {indices[0], indices[i], indices[i + 1]};
                    Triangle triangle{};
                    for (int vertex = 0; vertex < 3; ++vertex)
                    {
                        const int index = face_indices[vertex];
                        if (index < 0 || static_cast<size_t>(index) >= vertices.size())
                        {
                            error = "OBJ face index out of range";
                            return false;
                        }
                        for (int axis = 0; axis < 3; ++axis)
                            triangle.v[vertex][axis] = vertices[index][axis];
                    }
                    triangles.push_back(triangle);
                }
            }
        }

        if (triangles.empty())
        {
            error = "OBJ file did not contain triangulable faces";
            return false;
        }
        return true;
    }

    bool LoadMesh(const std::string &path, std::vector<Triangle> &triangles, std::string &error)
    {
#ifdef VOXELIZATION_USE_ASSIMP
        Assimp::Importer imp;
        const aiScene *scene = imp.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
        if (scene && scene->HasMeshes())
        {
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
            return true;
        }

        error = imp.GetErrorString();
#endif

        const bool looks_like_obj = path.size() >= 4 && path.substr(path.size() - 4) == ".obj";
        if (looks_like_obj)
            return LoadObjMesh(path, triangles, error);

#ifdef VOXELIZATION_USE_ASSIMP
        return false;
#else
        error = "Assimp is not available and the fallback loader only supports OBJ files";
        return false;
#endif
    }

    void WriteZProjectionPGM(const std::string &path, const std::vector<uint64_t> &grid, int resolution)
    {
        std::ofstream out(path);
        out << "P2\n"
            << resolution << " " << resolution << "\n255\n";
        for (int y = resolution - 1; y >= 0; --y)
        {
            for (int x = 0; x < resolution; ++x)
            {
                bool occupied = false;
                for (int z = 0; z < resolution && !occupied; ++z)
                {
                    const uint64_t index = static_cast<uint64_t>(z) * resolution * resolution +
                                           static_cast<uint64_t>(y) * resolution +
                                           static_cast<uint64_t>(x);
                    occupied = TestBit(grid, index);
                }
                out << (occupied ? 0 : 255) << (x + 1 == resolution ? '\n' : ' ');
            }
        }
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

    Options options;
    if (!ParseOptions(argc, argv, options))
    {
        if (rank == 0)
        {
            std::fprintf(stderr,
                         "Usage: %s <mesh-file> [resolution] [projection.pgm] [--mode mpi|omp_private|omp_atomic|cuda_atomic] [--threads N]\n",
                         argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    if (options.mode != "mpi" && size != 1)
    {
        if (rank == 0)
            std::fprintf(stderr, "OpenMP/CUDA modes must be run with one MPI process. Use OMP_NUM_THREADS or --threads for CPU threads.\n");
        MPI_Finalize();
        return 1;
    }

    const int resolution = options.resolution;
    std::vector<Triangle> triangles;
    std::vector<Triangle> local_triangles;
    Bounds bounds{};
    double load_seconds = 0.0;
    const bool mpi_mode = options.mode == "mpi";
    MPI_Datatype triangle_type = MPI_DATATYPE_NULL;
    MPI_Datatype bounds_type = MPI_DATATYPE_NULL;

    if (rank == 0)
    {
        const double load_start = MPI_Wtime();
        std::string load_error;
        if (!LoadMesh(options.mesh_path, triangles, load_error))
        {
            std::fprintf(stderr, "Failed to read mesh: %s\n", load_error.c_str());
            MPI_Abort(MPI_COMM_WORLD, 2);
        }

        bounds = ComputeBounds(triangles);
        load_seconds = MPI_Wtime() - load_start;
    }

    uint64_t triangle_count = triangles.size();
    std::vector<int> counts;

    if (mpi_mode)
    {
        triangle_type = CreateTriangleType();
        bounds_type = CreateBoundsType();

        MPI_Bcast(&triangle_count, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        counts = BuildCounts(triangle_count, size);
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
    }
    else
    {
        counts = {static_cast<int>(triangle_count)};
        local_triangles = triangles;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double voxel_start = MPI_Wtime();

    VoxelStats local_stats{};
    std::vector<uint64_t> local_grid;
    if (options.mode == "mpi")
        local_grid = Voxelize(local_triangles, resolution, bounds, local_stats);
    else if (options.mode == "omp_private")
        local_grid = VoxelizeOMPPrivate(local_triangles, resolution, bounds, local_stats, options.threads);
    else if (options.mode == "omp_atomic")
        local_grid = VoxelizeOMPAtomic(local_triangles, resolution, bounds, local_stats, options.threads);
    else
    {
#ifdef VOXELIZATION_USE_CUDA
        local_grid = VoxelizeCUDAAtomic(local_triangles, resolution, bounds, local_stats);
#else
        if (rank == 0)
            std::fprintf(stderr, "cuda_atomic mode was not built because CUDA was not available.\n");
        MPI_Finalize();
        return 1;
#endif
    }

    const double voxel_seconds = MPI_Wtime() - voxel_start;
    std::vector<uint64_t> global_grid(local_grid.size(), 0);
    uint64_t total_tested_voxels = local_stats.tested_voxels;
    uint64_t total_estimated_flops = local_stats.estimated_flops;
    double max_voxel_seconds = voxel_seconds;

    if (mpi_mode)
    {
        MPI_Reduce(local_grid.data(),
                   global_grid.data(),
                   static_cast<int>(local_grid.size()),
                   MPI_UINT64_T,
                   MPI_BOR,
                   0,
                   MPI_COMM_WORLD);
        MPI_Reduce(&local_stats.tested_voxels, &total_tested_voxels, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&local_stats.estimated_flops, &total_estimated_flops, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&voxel_seconds, &max_voxel_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }
    else
    {
        global_grid = std::move(local_grid);
    }

    if (rank == 0)
    {
        uint64_t occupied = 0;
        for (uint64_t word : global_grid)
            occupied += static_cast<uint64_t>(__builtin_popcountll(word));

        const double flops_per_second = max_voxel_seconds > 0.0
                                            ? static_cast<double>(total_estimated_flops) / max_voxel_seconds
                                            : 0.0;

        std::printf("mesh=%s\n", options.mesh_path.c_str());
        std::printf("mode=%s\n", options.mode.c_str());
        std::printf("threads=%d\n", options.threads);
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

        if (!options.projection_path.empty())
        {
            WriteZProjectionPGM(options.projection_path, global_grid, resolution);
            std::printf("projection=%s\n", options.projection_path.c_str());
        }
    }

    if (mpi_mode)
    {
        MPI_Type_free(&triangle_type);
        MPI_Type_free(&bounds_type);
    }

    ierr = MPI_Finalize();
    if (ierr != MPI_SUCCESS)
        MPI_Abort(MPI_COMM_WORLD, ierr);

    return 0;
}

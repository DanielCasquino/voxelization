# Parallel Voxelization - CPD 2026-I

Final project for Computacion Paralela y Distribuida. The program voxelizes a
triangle mesh into a dense bit-packed voxel grid.

## Algorithm

For each triangle, the implementation computes its axis-aligned bounding box
(AABB) in grid coordinates and marks all candidate voxels inside that box. The
grid is stored as 64-bit words, so each word stores 64 voxels.

This is a conservative AABB approximation. It is useful for analyzing
parallelism, communication and overhead, but it is not an exact triangle-box
intersection test.

## Parallel Modes

- `mpi`: main distributed-memory implementation. Rank 0 loads the mesh, splits
  triangles with `MPI_Scatterv`, each rank writes a private grid, and rank 0
  merges grids with `MPI_Reduce + MPI_BOR`.
- `omp_private`: shared-memory comparison. Each OpenMP thread writes a private
  grid and the thread grids are merged with bitwise OR.
- `omp_atomic`: shared-memory comparison. Threads write a shared grid using
  atomic OR on 64-bit words.
- `cuda_atomic`: optional CUDA prototype when CMake finds a CUDA compiler. It
  launches one GPU thread per triangle and uses global `atomicOr`; this is a
  minimal comparison, not an optimized GPU voxelizer.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On NixOS:

```bash
nix shell nixpkgs#cmake nixpkgs#gcc nixpkgs#gnumake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi --command cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
nix shell nixpkgs#cmake nixpkgs#gcc nixpkgs#gnumake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi --command cmake --build build
```

## Run

MPI:

```bash
mpirun -np 4 ./build/voxelization models/blocky_creeper_like.obj 128 --mode mpi
```

OpenMP:

```bash
./build/voxelization models/blocky_creeper_like.obj 128 --mode omp_private --threads 4
./build/voxelization models/blocky_creeper_like.obj 128 --mode omp_atomic --threads 4
```

Projection image:

```bash
./build/voxelization models/blocky_creeper_like.obj 128 outputs/creeper_128.pgm --mode omp_atomic --threads 4
```

The output prints the mesh, mode, number of processes, threads, resolution,
occupied voxels, tested candidate voxels, estimated FLOPs, load time and
voxelization time.

## Models

Tracked small models:

- `models/triangle.obj`
- `models/tetra.obj`
- `models/blocky_creeper_like.obj`
- `models/blocky_skeleton_like.obj`

The Stanford Bunny is used for the report benchmark but is not committed as a
large binary asset. It can be prepared with:

```bash
python3 scripts/prepare_bunny.py
```

## Experiments

The main summarized experiment files are:

- `results/metrics.csv`: Stanford Bunny MPI runs.
- `results/local_summary_median_20260705.csv`: local multi-mode CPU summary.
- `results/khipu_cpu_small_summary_20260705.csv`: Khipu CPU smoke summary.
- `results/khipu_cuda_atomic_summary_20260705.csv`: Khipu CUDA atomic smoke summary.

To run new experiments:

```bash
python3 scripts/run_experiments.py --smoke
python3 scripts/run_experiments.py --mesh models/stanford-bunny/bun_zipper.ply --mode mpi
```

## Report

The final report is in:

```text
report/main.tex
report/main.pdf
```

It explains the PRAM model, Foster design, MPI/OpenMP/CUDA tradeoffs, speedup,
efficiency, overhead, Khipu validation and limitations.

# Overhead comparison: MPI, shared memory, and hybrid/GPU

This note is for the final report and oral defense. It explains why the current project keeps MPI as the main distributed implementation, how the OpenMP comparison modes work, and why CUDA/tile-based methods are treated as future work for this delivery.

## Problem variables

- `m`: number of mesh triangles.
- `r`: voxel grid resolution per axis, so the grid has `r^3` voxels.
- `B = sum_i b_i`: total candidate voxels visited by all triangle AABBs.
- `w = ceil(r^3 / 64)`: number of 64-bit words in the bit-packed voxel grid.
- `p`: number of MPI processes or CPU workers.
- `g`: number of GPU threads or effective GPU workers.
- `alpha`: latency cost per communication/synchronization step.
- `beta`: transfer cost per data item.
- `gamma`: cost of an atomic/lock update in shared memory.

The sequential work of the current conservative voxelizer is:

```text
T1 = Theta(m + B)
```

The measured region in the code is `voxel_seconds`; it includes local voxelization plus the final MPI reductions.

## Current implementation: distributed memory with MPI

The current code chooses distributed memory:

1. rank 0 loads the mesh;
2. `MPI_Scatterv` distributes triangles;
3. `MPI_Bcast` sends the global bounding box;
4. each rank builds a private bit grid;
5. `MPI_Reduce` with `MPI_BOR` merges all grids.

Model:

```text
Tcomp_MPI = Theta(B/p)
Tscatter  = Theta(p*alpha + m*beta)
Tbcast    = Theta(log(p) * (alpha + beta))
Treduce   = Theta(log(p) * (alpha + w*beta))

Tp_MPI = Theta(B/p + p*alpha + m*beta + log(p)*(alpha + w*beta))
To_MPI = p*Tp_MPI - T1
```

Why MPI is defensible:

- It is portable to multiple nodes, not only one machine.
- It avoids concurrent writes to the same grid by giving every rank a private grid.
- The final OR is expressed directly as `MPI_Reduce + MPI_BOR`.
- The main overhead is explicit and measurable: scattering triangles and reducing the bit grid.

Expected weakness:

- For small `B`, communication dominates.
- For large `r`, the `w = r^3/64` grid reduction becomes expensive.
- Partitioning by number of triangles can be imbalanced if some triangles cover much larger AABBs.

## Implemented comparison: shared memory with OpenMP

A shared-memory implementation keeps one process and uses threads. The implemented OpenMP modes assign triangles to threads, matching the MPI partitioning idea while changing the memory model.

There are two grid strategies in the code.

### Shared global grid with atomic OR

```text
Tcomp_OMP_atomic = Theta(B/p)
Tsync_atomic     = Theta(A * gamma + C_contention)
Tp_OMP_atomic    = Theta(B/p + C*gamma)
To_OMP_atomic    = p*Tp_OMP_atomic - T1
```

`A` is the number of atomic updates and `C_contention` is the extra cost when many threads update the same 64-bit word. This version saves memory because it stores one shared grid, but every voxel mark needs an atomic OR. With a bit-packed grid, two threads can update different bits in the same word and still race without atomic OR.

### Private grid per thread + OR reduction

```text
Tcomp_OMP_private = Theta(B/p)
Treduction_OMP    = Theta(w*p / q)
Tp_OMP_private    = Theta(B/p + w*p/q)
To_OMP_private    = p*Tp_OMP_private - T1
```

`q` is the effective memory bandwidth/parallel reduction factor. This resembles MPI but uses memory bandwidth instead of network communication.

Why OpenMP may have lower overhead on one machine:

- No `MPI_Scatterv` or network messages.
- Threads share the mesh and bounds in memory.
- Reduction can be faster if memory bandwidth is high.

Why it is not automatically better:

- Atomic updates can be costly and nondeterministic under contention.
- Private grids multiply memory use by `p`.
- It does not scale across multiple nodes.

## Tile-based alternative and why it is out of scope

A tile-based design assigns disjoint voxel chunks to workers:

```text
worker j owns tile j
worker j writes only voxels in tile j
```

This can eliminate atomics and the final OR reduction because every voxel has exactly one owner. The tradeoff is triangle filtering. A naive tile implementation checks every triangle against every tile:

```text
T_tile_naive = Theta(r^3 * m / p)
```

This can scale strongly in a formal sense because tiles are independent, but the total work can be much larger than the triangle-based algorithm:

```text
T_triangle = Theta(B)
T_tile_naive = Theta(r^3 * m)
```

Practical tile-based voxelizers therefore need spatial binning, sorting, BVH, octrees, or sparse grids to assign only relevant triangles to each tile. That is a valid state-of-the-art direction, but it changes the project from parallelizing voxelization to designing a spatial acceleration structure. For this delivery, fixed small/medium grids and triangle-based partitioning are a better fit.

## Alternative 2: GPU/CUDA or hybrid MPI + CUDA

A GPU implementation maps candidate voxel checks or triangle work to CUDA threads. A hybrid implementation keeps MPI between nodes and CUDA inside each node.

Simplified single-GPU model:

```text
Tcopy_H2D = Theta((m + w) * beta_pcie)
Tkernel   = Theta(B/g + A*gamma_gpu)
Tcopy_D2H = Theta(w * beta_pcie)
Tlaunch   = Theta(alpha_launch)

Tp_CUDA = Theta(Tcopy_H2D + Tlaunch + B/g + A*gamma_gpu + Tcopy_D2H)
To_CUDA = g_eff*Tp_CUDA - T1
```

`A` is the number of atomic/global-memory updates. If the GPU uses a bit grid, it needs atomic OR or a two-stage private/block reduction. The GPU can reduce compute time, but it adds transfer and launch overhead.

Hybrid MPI + CUDA:

```text
Tp_hybrid = Theta(B/(nodes*g) + Tcopy + Tlaunch + Tlocal_reduce_gpu + Tmpi_reduce_grid)
```

Why CUDA is attractive:

- Voxel marking is highly parallel.
- The problem statement mentions GPU voxelization in the project description.
- For large `B`, many candidate voxel operations can be amortized over thousands of threads.

Why CUDA is risky for this delivery:

- The laptop does not have a local GPU; Khipu has GPU access but should be used conservatively.
- A correct GPU bit-grid needs atomic OR or a careful block-level reduction.
- Results must be validated against the existing MPI output and measured with real hardware.
- A rushed CUDA port without measurements would weaken the report more than a well-justified MPI solution.

## Recommendation for the final report

Use MPI as the main distributed solution and OpenMP as the implemented shared-memory comparison. Present CUDA/tile-based designs as state-of-the-art alternatives and future work:

```text
We chose MPI as the main implementation because it models distributed memory directly: each process computes a private voxel grid and the final grid is obtained by `MPI_Reduce` with `MPI_BOR`. This avoids concurrent writes during voxel marking and lets us measure communication and reduction overhead explicitly. We also implemented two OpenMP variants to compare against shared memory: a private-grid version, which trades memory for no atomics, and a shared-grid atomic version, which saves memory but can suffer contention. CUDA and tile-based voxelization are strong state-of-the-art directions for larger workloads, but they require GPU-specific memory management or spatial indexing beyond the scope of this final version.
```

## Experimental overhead from current data

For the implemented MPI version, compute:

```text
S = T1 / Tp
E = S / p
To = p*Tp - T1
To/T1 = overhead ratio
```

Generate the table with:

```bash
python3 scripts/analyze_overhead.py
```

The output is:

```text
results/overhead_summary.csv
```

Interpretation pattern:

- If `To/T1` is small, the parallel work is amortizing overhead.
- If `To/T1` grows with `p`, adding more processes is increasing communication/synchronization cost.
- In the existing metrics, efficiency is high up to `p=4` and drops at `p=8`, which supports the conclusion that overhead begins to dominate for this input size.

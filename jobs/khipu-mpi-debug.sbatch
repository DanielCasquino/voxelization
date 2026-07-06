#!/bin/bash
#SBATCH --job-name=vox-mpi-debug
#SBATCH --partition=debug
#SBATCH --time=00:10:00
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --output=logs/%x-%j.out
#SBATCH --error=logs/%x-%j.err

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"

module list
module load cmake/4.3.2

srun --ntasks=1 bash -lc '
  set -euo pipefail
  mkdir -p logs results/khipu build-khipu-debug

  if ! pkg-config --exists assimp; then
    echo "ERROR: Assimp is not visible through pkg-config on this Khipu environment." >&2
    echo "Do not install heavy dependencies blindly on Khipu. Resolve dependency first or run MPI measurements on laptop." >&2
    exit 42
  fi

  cmake -S . -B build-khipu-debug -DCMAKE_BUILD_TYPE=Release
  cmake --build build-khipu-debug -j "${SLURM_CPUS_PER_TASK:-1}"
'

for p in 1 2 4; do
  srun --ntasks="$p" ./build-khipu-debug/voxelization models/triangle.obj 32
done | tee "results/khipu/mpi-debug-${SLURM_JOB_ID}.txt"

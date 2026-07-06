#!/bin/bash
#SBATCH --job-name=vox-cuda-atomic
#SBATCH --partition=debug-gpu
#SBATCH --time=00:05:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=2G
#SBATCH --gres=shard:1
#SBATCH --output=logs/vox-cuda-atomic-%j.out
#SBATCH --error=logs/vox-cuda-atomic-%j.err

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"

module load openmpi4/4.1.6 gnu12/12.4.0 cuda/12.8 >/dev/null 2>&1 || true
mkdir -p logs results/khipu-cuda-atomic-small

EXE=./build-khipu-cuda/voxelization
MESH=models/blocky_creeper_like.obj

if [[ ! -x "$EXE" ]]; then
  echo "ERROR: missing executable $EXE. Compile on login first." >&2
  exit 42
fi

nvidia-smi --query-gpu=name,memory.total --format=csv,noheader
for resolution in 64 128 256 512; do
  echo "mode=cuda_atomic resolution=${resolution}"
  mpirun -np 1 "$EXE" "$MESH" "$resolution" --mode cuda_atomic \
    | tee "results/khipu-cuda-atomic-small/cuda_atomic_r${resolution}_${SLURM_JOB_ID}.txt"
  echo "mode=omp_atomic resolution=${resolution} threads=4"
  OMP_NUM_THREADS=4 mpirun -np 1 "$EXE" "$MESH" "$resolution" --mode omp_atomic --threads 4 \
    | tee "results/khipu-cuda-atomic-small/omp_atomic_r${resolution}_t4_${SLURM_JOB_ID}.txt"
  echo "mode=mpi resolution=${resolution} p=1"
  mpirun -np 1 "$EXE" "$MESH" "$resolution" --mode mpi \
    | tee "results/khipu-cuda-atomic-small/mpi_r${resolution}_p1_${SLURM_JOB_ID}.txt"
done

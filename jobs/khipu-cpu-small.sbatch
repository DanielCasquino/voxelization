#!/bin/bash
#SBATCH --job-name=vox-cpu-small
#SBATCH --partition=debug
#SBATCH --time=00:05:00
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --output=logs/khipu-cpu-small-%j.out
#SBATCH --error=logs/khipu-cpu-small-%j.err

set -euo pipefail

module load openmpi4/4.1.6 gnu12/12.4.0 >/dev/null 2>&1 || true

mkdir -p logs results/khipu-cpu-small

EXE=./build-khipu-debug/voxelization
MESH=models/blocky_creeper_like.obj

if [[ ! -x "$EXE" ]]; then
  echo "ERROR: missing executable $EXE. Compile on login first." >&2
  exit 42
fi

for resolution in 32 64; do
  for p in 1 2 4; do
    echo "mode=mpi resolution=${resolution} p=${p}"
    mpirun -np "$p" "$EXE" "$MESH" "$resolution" --mode mpi \
      | tee "results/khipu-cpu-small/mpi_r${resolution}_p${p}_${SLURM_JOB_ID}.txt"
  done
done

for mode in omp_private omp_atomic; do
  for resolution in 32 64; do
    for threads in 1 2 4; do
      echo "mode=${mode} resolution=${resolution} threads=${threads}"
      OMP_NUM_THREADS="$threads" "$EXE" "$MESH" "$resolution" --mode "$mode" --threads "$threads" \
        | tee "results/khipu-cpu-small/${mode}_r${resolution}_t${threads}_${SLURM_JOB_ID}.txt"
    done
  done
done

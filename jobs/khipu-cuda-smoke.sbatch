#!/bin/bash
#SBATCH --job-name=vox-cuda-smoke
#SBATCH --partition=debug-gpu
#SBATCH --time=00:05:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=1G
#SBATCH --gres=shard:1
#SBATCH --output=logs/%x-%j.out
#SBATCH --error=logs/%x-%j.err

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"

module load cuda/12.8
mkdir -p logs results/khipu

SMOKE_BIN="${SMOKE_BIN:-build-khipu-cuda/cuda_smoke}"
export SMOKE_BIN

if [ ! -x "$SMOKE_BIN" ]; then
  echo "ERROR: missing executable $SMOKE_BIN" >&2
  echo "Compile on Khipu login/master first, per official docs:" >&2
  echo "  module load cuda/12.8" >&2
  echo "  mkdir -p build-khipu-cuda" >&2
  echo "  nvcc -O2 -arch=sm_75 cuda_smoke.cu -o build-khipu-cuda/cuda_smoke" >&2
  exit 42
fi

srun bash -lc '
  set -euo pipefail
  which nvcc
  nvcc --version | head -n 4
  nvidia-smi --query-gpu=name,memory.total --format=csv,noheader
  "$SMOKE_BIN" | tee "results/khipu/cuda-smoke-${SLURM_JOB_ID}.txt"
'

#!/bin/bash
#SBATCH --job-name=vox-env-smoke
#SBATCH --partition=debug
#SBATCH --time=00:05:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=512M
#SBATCH --output=logs/%x-%j.out
#SBATCH --error=logs/%x-%j.err

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"

module list
module load cmake/4.3.2

srun bash -lc '
  set -euo pipefail
  echo "host=$(hostname)"
  echo "pwd=$PWD"
  echo "cmake=$(cmake --version | head -n 1)"
  echo "mpicxx=$(command -v mpicxx)"
  mpicxx --version | head -n 1
  echo "assimp_pkg_config=$(pkg-config --modversion assimp 2>/dev/null || echo missing)"
  test -d models && find models -maxdepth 2 -type f | sort | head -n 20
'

#!/usr/bin/env python3
"""Run MPI voxelization experiments and consolidate the metrics as CSV."""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


DEFAULT_PROCESSES = [1, 2, 4, 8]
DEFAULT_RESOLUTIONS = [32, 64, 96, 128]
DEFAULT_REPEATS = 3
METRIC_KEYS = [
    "mesh",
    "triangles",
    "processes",
    "resolution",
    "occupied_voxels",
    "tested_voxels",
    "estimated_flops",
    "load_seconds",
    "voxel_seconds",
    "estimated_flops_per_second",
]


def parse_int_list(value: str) -> list[int]:
    try:
        result = [int(item.strip()) for item in value.split(",") if item.strip()]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid integer list: {value}") from exc
    if not result or any(item <= 0 for item in result):
        raise argparse.ArgumentTypeError("Values must be positive integers.")
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mesh", default="models/stanford_bunny.ply", type=Path)
    parser.add_argument("--build-dir", default=None, type=Path)
    parser.add_argument("--exe", default=None, type=Path)
    parser.add_argument("--processes", default="1,2,4,8", type=parse_int_list)
    parser.add_argument("--resolutions", default="32,64,96,128", type=parse_int_list)
    parser.add_argument("--repeats", default=DEFAULT_REPEATS, type=int)
    parser.add_argument("--results-dir", default=Path("results"), type=Path)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--smoke", action="store_true", help="Run a tiny quick validation set.")
    return parser.parse_args()


def choose_build_dir(explicit: Path | None) -> Path:
    if explicit is not None:
        return explicit
    if Path("cmake-build-debug").exists():
        return Path("cmake-build-debug")
    return Path("build")


def choose_mpi_launcher() -> str:
    for candidate in ("mpirun", "mpiexec"):
        path = shutil.which(candidate)
        if path:
            return path
    raise RuntimeError(
        "Could not find mpirun or mpiexec. On macOS run: brew install open-mpi"
    )


def choose_executable(explicit: Path | None, build_dir: Path) -> Path:
    if explicit is not None:
        return explicit
    candidates = [
        build_dir / "voxelization",
        Path("cmake-build-debug") / "voxelization",
        Path("build") / "voxelization",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return build_dir / "voxelization"


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def configure_and_build(build_dir: Path, exe: Path, no_build: bool) -> None:
    if exe.exists():
        return
    if no_build:
        raise RuntimeError(f"Executable not found: {exe}")

    cmake = shutil.which("cmake")
    if not cmake:
        raise RuntimeError("Could not find cmake.")

    configure_command = [cmake, "-S", ".", "-B", str(build_dir)]
    configure = run_command(configure_command)
    if configure.returncode != 0 and "Could NOT find OpenMP" in configure.stderr:
        libomp = Path("/opt/homebrew/opt/libomp/lib/libomp.dylib")
        if libomp.exists():
            configure = run_command(
                configure_command
                + [
                    "-DOpenMP_C_FLAGS=-Xpreprocessor -fopenmp",
                    "-DOpenMP_C_LIB_NAMES=omp",
                    f"-DOpenMP_omp_LIBRARY={libomp}",
                    "-DOpenMP_CXX_FLAGS=-Xpreprocessor -fopenmp",
                    "-DOpenMP_CXX_LIB_NAMES=omp",
                ]
            )
    if configure.returncode != 0:
        raise RuntimeError(
            "CMake configure failed.\n"
            f"stdout:\n{configure.stdout}\n"
            f"stderr:\n{configure.stderr}"
        )

    build = run_command([cmake, "--build", str(build_dir)])
    if build.returncode != 0:
        raise RuntimeError(
            "CMake build failed.\n"
            f"stdout:\n{build.stdout}\n"
            f"stderr:\n{build.stderr}"
        )

    if not exe.exists():
        raise RuntimeError(f"Build finished but executable was not found: {exe}")


def parse_metrics(stdout: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    for line in stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key in METRIC_KEYS:
            metrics[key] = value
    missing = [key for key in METRIC_KEYS if key not in metrics]
    if missing:
        raise RuntimeError(f"Program output is missing metrics: {', '.join(missing)}")
    return metrics


def raw_output_path(raw_dir: Path, mesh: Path, resolution: int, processes: int, repeat: int) -> Path:
    mesh_name = mesh.stem.replace(" ", "_")
    return raw_dir / f"{mesh_name}_r{resolution}_p{processes}_rep{repeat}.txt"


def compute_derived_metrics(rows: list[dict[str, str]]) -> None:
    baseline_times: dict[int, float] = {}
    grouped: dict[tuple[int, int], list[float]] = defaultdict(list)

    for row in rows:
        grouped[(int(row["resolution"]), int(row["processes"]))].append(float(row["voxel_seconds"]))

    for (resolution, processes), times in grouped.items():
        if processes == 1:
            baseline_times[resolution] = sum(times) / len(times)

    for row in rows:
        resolution = int(row["resolution"])
        processes = int(row["processes"])
        current_time = float(row["voxel_seconds"])
        baseline = baseline_times.get(resolution)
        if baseline is None or current_time <= 0.0:
            speedup = 0.0
        else:
            speedup = baseline / current_time
        efficiency = speedup / processes if processes > 0 else 0.0
        row["speedup"] = f"{speedup:.6f}"
        row["efficiency"] = f"{efficiency:.6f}"


def write_metrics_csv(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "mesh",
        "resolution",
        "processes",
        "repeat",
        "triangles",
        "occupied_voxels",
        "tested_voxels",
        "estimated_flops",
        "load_seconds",
        "voxel_seconds",
        "estimated_flops_per_second",
        "speedup",
        "efficiency",
    ]
    with path.open("w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row[key] for key in fieldnames})


def run_experiments(args: argparse.Namespace) -> list[dict[str, str]]:
    if args.repeats <= 0:
        raise RuntimeError("--repeats must be positive.")

    mesh = args.mesh
    if args.smoke:
        mesh = Path("models/triangle.obj")
        args.processes = [1, 2]
        args.resolutions = [16]
        args.repeats = 1

    if not mesh.exists():
        raise RuntimeError(
            f"Mesh not found: {mesh}. Run: python3 scripts/prepare_bunny.py"
        )

    build_dir = choose_build_dir(args.build_dir)
    exe = choose_executable(args.exe, build_dir)
    configure_and_build(build_dir, exe, args.no_build)
    mpi_launcher = choose_mpi_launcher()

    raw_dir = args.results_dir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, str]] = []
    total = len(args.processes) * len(args.resolutions) * args.repeats
    current = 0

    for resolution in args.resolutions:
        for processes in args.processes:
            for repeat in range(1, args.repeats + 1):
                current += 1
                command = [
                    mpi_launcher,
                    "-np",
                    str(processes),
                    str(exe),
                    str(mesh),
                    str(resolution),
                ]
                print(
                    f"[{current}/{total}] resolution={resolution} "
                    f"processes={processes} repeat={repeat}"
                )
                result = run_command(command)
                raw_path = raw_output_path(raw_dir, mesh, resolution, processes, repeat)
                raw_path.write_text(
                    "$ " + " ".join(command) + "\n\n"
                    + result.stdout
                    + ("\n[stderr]\n" + result.stderr if result.stderr else ""),
                    encoding="utf-8",
                )
                if result.returncode != 0:
                    raise RuntimeError(
                        f"Experiment failed. See raw output: {raw_path}\n"
                        f"stderr:\n{result.stderr}"
                    )
                metrics = parse_metrics(result.stdout)
                metrics["repeat"] = str(repeat)
                rows.append(metrics)

    compute_derived_metrics(rows)
    write_metrics_csv(args.results_dir / "metrics.csv", rows)
    return rows


def main() -> int:
    args = parse_args()
    rows = run_experiments(args)
    print(f"Wrote {len(rows)} rows to {args.results_dir / 'metrics.csv'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)

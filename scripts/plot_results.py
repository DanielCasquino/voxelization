#!/usr/bin/env python3
"""Generate experiment plots from results/metrics.csv."""

from __future__ import annotations

import argparse
import csv
import os
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metrics", default=Path("results/metrics.csv"), type=Path)
    parser.add_argument("--plots-dir", default=Path("results/plots"), type=Path)
    return parser.parse_args()


def load_rows(path: Path) -> list[dict[str, float | str]]:
    if not path.exists():
        raise RuntimeError(f"Metrics file not found: {path}")

    rows: list[dict[str, float | str]] = []
    numeric_keys = {
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
    }
    with path.open(newline="", encoding="utf-8") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            parsed: dict[str, float | str] = {}
            for key, value in row.items():
                parsed[key] = float(value) if key in numeric_keys else value
            rows.append(parsed)
    if not rows:
        raise RuntimeError(f"Metrics file is empty: {path}")
    return rows


def average_by(rows: list[dict[str, float | str]], y_key: str) -> dict[int, dict[int, float]]:
    grouped: dict[int, dict[int, list[float]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        resolution = int(row["resolution"])
        processes = int(row["processes"])
        grouped[resolution][processes].append(float(row[y_key]))
    return {
        resolution: {
            processes: mean(values)
            for processes, values in sorted(process_values.items())
        }
        for resolution, process_values in sorted(grouped.items())
    }


def import_pyplot():
    os.environ.setdefault("MPLBACKEND", "Agg")
    os.environ.setdefault("MPLCONFIGDIR", str(Path("results") / ".matplotlib"))
    Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)
    try:
        import matplotlib.pyplot as plt
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "matplotlib is not installed. Run: "
            "python3 -m venv .venv && .venv/bin/python -m pip install matplotlib"
        ) from exc
    return plt


def plot_metric(plt, data: dict[int, dict[int, float]], title: str, ylabel: str, output: Path) -> None:
    plt.figure(figsize=(9, 5.5))
    for resolution, values in data.items():
        x = list(values.keys())
        y = list(values.values())
        plt.plot(x, y, marker="o", label=f"resolution={resolution}")
    plt.xlabel("Processes")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, alpha=0.35)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output, dpi=160)
    plt.close()


def plot_theoretical_vs_experimental(plt, rows: list[dict[str, float | str]], output: Path) -> None:
    speedup_data = average_by(rows, "speedup")
    plt.figure(figsize=(9, 5.5))
    for resolution, values in speedup_data.items():
        x = list(values.keys())
        y = list(values.values())
        plt.plot(x, y, marker="o", label=f"experimental r={resolution}")
    all_processes = sorted({int(row["processes"]) for row in rows})
    plt.plot(all_processes, all_processes, linestyle="--", color="black", label="ideal T1 / p")
    plt.xlabel("Processes")
    plt.ylabel("Speedup")
    plt.title("Theoretical vs Experimental Speedup")
    plt.grid(True, alpha=0.35)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output, dpi=160)
    plt.close()


def plot_scalability(plt, rows: list[dict[str, float | str]], output: Path) -> None:
    grouped: dict[int, dict[int, list[float]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        processes = int(row["processes"])
        resolution = int(row["resolution"])
        grouped[processes][resolution].append(float(row["voxel_seconds"]))

    plt.figure(figsize=(9, 5.5))
    for processes, values in sorted(grouped.items()):
        x = sorted(values.keys())
        y = [mean(values[resolution]) for resolution in x]
        plt.plot(x, y, marker="o", label=f"p={processes}")
    plt.xlabel("Resolution")
    plt.ylabel("Voxelization time (s)")
    plt.title("Scalability by Problem Size")
    plt.grid(True, alpha=0.35)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output, dpi=160)
    plt.close()


def main() -> int:
    args = parse_args()
    rows = load_rows(args.metrics)
    args.plots_dir.mkdir(parents=True, exist_ok=True)
    plt = import_pyplot()

    plot_metric(
        plt,
        average_by(rows, "voxel_seconds"),
        "Voxelization Time vs Processes",
        "Voxelization time (s)",
        args.plots_dir / "time_vs_processes.png",
    )
    plot_metric(
        plt,
        average_by(rows, "speedup"),
        "Speedup vs Processes",
        "Speedup",
        args.plots_dir / "speedup_vs_processes.png",
    )
    plot_metric(
        plt,
        average_by(rows, "efficiency"),
        "Efficiency vs Processes",
        "Efficiency",
        args.plots_dir / "efficiency_vs_processes.png",
    )
    plot_metric(
        plt,
        average_by(rows, "estimated_flops_per_second"),
        "Estimated FLOPs/s vs Processes",
        "Estimated FLOPs/s",
        args.plots_dir / "flops_vs_processes.png",
    )
    plot_scalability(plt, rows, args.plots_dir / "scalability_by_resolution.png")
    plot_theoretical_vs_experimental(
        plt,
        rows,
        args.plots_dir / "theoretical_vs_experimental.png",
    )

    print(f"Wrote plots to {args.plots_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)

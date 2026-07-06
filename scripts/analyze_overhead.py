#!/usr/bin/env python3
"""Compute experimental overhead from voxelization metrics.

The course convention is:
  S = T1 / Tp
  E = S / p
  To = p * Tp - T1

This script uses the mean voxel_seconds for each (resolution, processes) pair.
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metrics", default=Path("results/metrics.csv"), type=Path)
    parser.add_argument("--out", default=Path("results/overhead_summary.csv"), type=Path)
    return parser.parse_args()


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def main() -> int:
    args = parse_args()
    groups: dict[tuple[int, int], list[dict[str, str]]] = defaultdict(list)
    with args.metrics.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            groups[(int(row["resolution"]), int(row["processes"]))].append(row)

    baseline: dict[int, float] = {}
    for (resolution, processes), rows in groups.items():
        if processes == 1:
            baseline[resolution] = mean([float(row["voxel_seconds"]) for row in rows])

    out_rows: list[dict[str, str]] = []
    for (resolution, processes), rows in sorted(groups.items()):
        if resolution not in baseline:
            continue
        tp = mean([float(row["voxel_seconds"]) for row in rows])
        t1 = baseline[resolution]
        speedup = t1 / tp if tp > 0 else 0.0
        efficiency = speedup / processes if processes > 0 else 0.0
        overhead = processes * tp - t1
        overhead_ratio = overhead / t1 if t1 > 0 else 0.0
        out_rows.append(
            {
                "resolution": str(resolution),
                "processes": str(processes),
                "T1_seconds": f"{t1:.9f}",
                "Tp_seconds": f"{tp:.9f}",
                "speedup": f"{speedup:.6f}",
                "efficiency": f"{efficiency:.6f}",
                "overhead_To_seconds": f"{overhead:.9f}",
                "overhead_ratio_To_over_T1": f"{overhead_ratio:.6f}",
            }
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(out_rows[0].keys()))
        writer.writeheader()
        writer.writerows(out_rows)

    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

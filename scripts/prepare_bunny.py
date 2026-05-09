#!/usr/bin/env python3
"""Download and prepare the Stanford Bunny dataset for the experiments."""

from __future__ import annotations

import argparse
import shutil
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path


DEFAULT_URL = "https://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz"
DEFAULT_MEMBER = "bunny/reconstruction/bun_zipper.ply"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=DEFAULT_URL, help="Dataset archive URL.")
    parser.add_argument(
        "--member",
        default=DEFAULT_MEMBER,
        help="PLY file path inside the Stanford Bunny tarball.",
    )
    parser.add_argument(
        "--output",
        default="models/stanford-bunny/bun_zipper.ply",
        type=Path,
        help="Output PLY path.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite the output file if it already exists.",
    )
    return parser.parse_args()


def download_archive(url: str, destination: Path) -> None:
    try:
        with urllib.request.urlopen(url, timeout=60) as response:
            with destination.open("wb") as out:
                shutil.copyfileobj(response, out)
    except Exception as exc:  # noqa: BLE001 - show a precise CLI-facing failure.
        raise RuntimeError(
            f"Could not download {url}. Check your network connection and try again."
        ) from exc


def extract_member(archive_path: Path, member_name: str, output_path: Path) -> None:
    try:
        with tarfile.open(archive_path, "r:gz") as archive:
            member = archive.getmember(member_name)
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"Archive member {member_name} is not a regular file.")

            output_path.parent.mkdir(parents=True, exist_ok=True)
            with output_path.open("wb") as out:
                shutil.copyfileobj(source, out)
    except KeyError as exc:
        raise RuntimeError(f"Could not find {member_name} in {archive_path}.") from exc
    except tarfile.TarError as exc:
        raise RuntimeError(f"Could not read tarball {archive_path}.") from exc


def write_attribution(models_dir: Path, dataset_url: str) -> None:
    readme_path = models_dir / "README.md"
    readme_path.write_text(
        "\n".join(
            [
                "# Models",
                "",
                "## Stanford Bunny",
                "",
                "- File: `stanford-bunny/bun_zipper.ply`",
                "- Source: Stanford 3D Scanning Repository",
                f"- URL: {dataset_url}",
                "- Original dataset: Stanford Computer Graphics Laboratory",
                "",
                "The bunny model is used here as academic benchmark data for the CPD",
                "voxelization experiments.",
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()
    output_path = args.output

    if output_path.exists() and not args.force:
        print(f"Dataset already exists: {output_path}")
        write_attribution(output_path.parent, args.url)
        return 0

    with tempfile.TemporaryDirectory(prefix="stanford-bunny-") as tmp_dir:
        archive_path = Path(tmp_dir) / "bunny.tar.gz"
        print(f"Downloading {args.url}")
        download_archive(args.url, archive_path)
        print(f"Extracting {args.member} -> {output_path}")
        extract_member(archive_path, args.member, output_path)

    write_attribution(output_path.parent, args.url)
    print(f"Ready: {output_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)

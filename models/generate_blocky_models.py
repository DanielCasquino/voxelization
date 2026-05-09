#!/usr/bin/env python3
"""Generate small blocky OBJ meshes for voxelization smoke tests."""

from pathlib import Path


def add_box(vertices, faces, x0, y0, z0, x1, y1, z1):
    base = len(vertices) + 1
    vertices.extend(
        [
            (x0, y0, z0),
            (x1, y0, z0),
            (x1, y1, z0),
            (x0, y1, z0),
            (x0, y0, z1),
            (x1, y0, z1),
            (x1, y1, z1),
            (x0, y1, z1),
        ]
    )
    quads = [
        (1, 2, 3, 4),
        (5, 8, 7, 6),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 8, 4),
        (4, 8, 5, 1),
    ]
    for a, b, c, d in quads:
        faces.append((base + a - 1, base + b - 1, base + c - 1))
        faces.append((base + a - 1, base + c - 1, base + d - 1))


def write_obj(path, object_name, boxes):
    vertices = []
    faces = []
    for box in boxes:
        add_box(vertices, faces, *box)

    with Path(path).open("w", encoding="utf-8") as out:
        out.write(f"o {object_name}\n")
        for vertex in vertices:
            out.write("v {:.4f} {:.4f} {:.4f}\n".format(*vertex))
        for face in faces:
            out.write("f {} {} {}\n".format(*face))


def main():
    root = Path(__file__).resolve().parent

    write_obj(
        root / "blocky_creeper_like.obj",
        "blocky_creeper_like",
        [
            (-0.45, 0.00, -0.22, 0.45, 1.10, 0.22),  # body
            (-0.55, 1.10, -0.32, 0.55, 1.85, 0.32),  # head
            (-0.55, -0.55, -0.32, -0.15, 0.00, 0.02),  # leg 1
            (0.15, -0.55, -0.32, 0.55, 0.00, 0.02),  # leg 2
            (-0.55, -0.55, 0.05, -0.15, 0.00, 0.38),  # leg 3
            (0.15, -0.55, 0.05, 0.55, 0.00, 0.38),  # leg 4
        ],
    )

    write_obj(
        root / "blocky_skeleton_like.obj",
        "blocky_skeleton_like",
        [
            (-0.22, 0.00, -0.12, 0.22, 0.90, 0.12),  # torso
            (-0.30, 0.90, -0.18, 0.30, 1.45, 0.18),  # head
            (-0.65, 0.45, -0.08, -0.22, 0.65, 0.08),  # left arm
            (0.22, 0.45, -0.08, 0.65, 0.65, 0.08),  # right arm
            (-0.22, -0.65, -0.08, -0.04, 0.00, 0.08),  # left leg
            (0.04, -0.65, -0.08, 0.22, 0.00, 0.08),  # right leg
        ],
    )


if __name__ == "__main__":
    main()

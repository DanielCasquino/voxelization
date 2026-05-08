# Codex Stewart README

## Objetivo de esta branch

Esta branch deja un avance minimo y explicable del proyecto `Voxelization` para CPD 2026-I, sin tocar la branch `main` original de Daniel Casquino.

La idea es que Codex/Stewart pueda continuar desde aqui con contexto claro:

- `main.cpp`: carga la malla con Assimp, inicializa MPI, distribuye datos y reporta metricas.
- `src/voxelize.cpp`: contiene la voxelizacion aproximada por cajas envolventes.
- `include/voxelize.h`: define `Triangle`, `Bounds`, `VoxelStats` y la interfaz del voxelizador.
- `models/`: modelos `.obj` pequenos para smoke tests.

## Estado actual

El programa ya compila y corre con MPI. Implementa una beta conservadora:

1. Rank 0 carga un `.obj`.
2. Rank 0 aplana la malla a triangulos.
3. Todos los procesos reciben triangulos y bounding box global con `MPI_Bcast`.
4. Cada rank procesa un rango de triangulos.
5. Cada rank marca voxels locales en una grilla de bits.
6. Rank 0 combina las grillas con `MPI_Reduce` usando `MPI_BOR`.

Limitacion importante: todavia no se calcula interseccion exacta triangulo-voxel. La beta marca los voxels dentro del bounding box de cada triangulo. Esto puede sobreestimar voxels ocupados, pero sirve para validar arquitectura paralela, comunicacion y metricas.

## Comandos verificados en NixOS

```bash
nix shell nixpkgs#cmake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c cmake -S . -B build
nix shell nixpkgs#cmake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c cmake --build build
nix shell nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c mpirun --allow-run-as-root -np 1 ./build/voxelization models/triangle.obj 32
nix shell nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c mpirun --allow-run-as-root -np 2 ./build/voxelization models/triangle.obj 32
```

## Siguiente mejora tecnica

Reemplazar el marcado por bounding box por un test de interseccion triangulo-voxel:

- construir el AABB del voxel;
- probar interseccion triangulo-AABB;
- marcar solo si hay interseccion real.

Despues medir:

- `T1`
- `Tp`
- `speedup = T1 / Tp`
- `efficiency = speedup / p`
- `estimated_flops_per_second`

## Para exposicion

Frase corta:

> Voxelizar es convertir una malla 3D de triangulos en una grilla discreta de voxels ocupados. Paralelizamos repartiendo triangulos entre procesos MPI; cada proceso produce una grilla local y luego combinamos todas con una reduccion OR.

Frase honesta sobre la beta:

> Esta version usa una aproximacion conservadora por bounding boxes. Puede marcar voxels de mas, pero ya valida la estructura paralela y las metricas. La siguiente mejora es implementar interseccion exacta triangulo-voxel.

# Voxelization - CPD 2026-I

Avance minimo para el proyecto de Computacion Paralela y Distribuida.

## Idea

Voxelizar es convertir una malla 3D de triangulos en una grilla 3D discreta. Cada celda de
esa grilla es un voxel. El programa marca que voxels estan ocupados por la malla.

Esta version usa una aproximacion conservadora: para cada triangulo calcula su caja
envolvente y marca los voxels dentro de esa caja. No es todavia una interseccion exacta
triangulo-voxel, pero deja armado el flujo paralelo y las metricas para el informe parcial.

## Paralelizacion

- Rank 0 carga la malla con Assimp y aplana las caras en un arreglo de `Triangle`.
- Rank 0 difunde triangulos y bounding box global a todos los procesos con `MPI_Bcast`.
- Cada rank procesa un rango contiguo de triangulos.
- Cada rank produce una grilla local de bits.
- Rank 0 combina las grillas con `MPI_Reduce` y `MPI_BOR`.

## Build

```bash
cmake -S . -B build
cmake --build build
```

En NixOS, si faltan dependencias:

```bash
nix shell nixpkgs#cmake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c cmake -S . -B build
nix shell nixpkgs#cmake nixpkgs#assimp.dev nixpkgs#assimp nixpkgs#openmpi -c cmake --build build
```

## Run

```bash
mpirun -np 1 ./build/voxelization models/triangle.obj 32
mpirun -np 2 ./build/voxelization models/triangle.obj 32
```

La salida incluye triangulos, procesos, resolucion, voxels ocupados, tiempo de voxelizacion
y FLOPs estimados.

## Betas para el informe parcial

1. Beta 1: carga de malla con Assimp y conversion a arreglo de triangulos.
2. Beta 2: voxelizacion secuencial aproximada por bounding boxes.
3. Beta 3: reparto MPI por triangulos, reduccion bitwise y metricas.

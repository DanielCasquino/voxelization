# Experimentos de Voxelization - CLion

Este documento describe el flujo automatico para preparar Stanford Bunny, ejecutar los
experimentos MPI y generar las graficas pedidas para el proyecto CPD 2026-I.

El codigo C++ no se modifica para estos experimentos. Los scripts ejecutan el binario
existente y parsean las metricas que imprime por `stdout`.

## Dependencias en macOS

```bash
brew install open-mpi libomp
python3 -m venv .venv
.venv/bin/python -m pip install matplotlib
```

Assimp ya debe estar instalado para compilar el proyecto:

```bash
brew install assimp
```

En macOS, `libomp` es keg-only. El script de experimentos reintenta CMake con las flags
de `/opt/homebrew/opt/libomp` si `find_package(OpenMP)` falla.

Si CLion no encuentra `mpirun`, abra CLion desde una terminal donde Homebrew este en el
`PATH`:

```bash
open -a "CLion" .
```

## Build en CLion

CLion usa CMake y normalmente compila en:

```text
cmake-build-debug/
```

El script de experimentos usa ese directorio por defecto si existe. Tambien puede indicar
otro directorio con `--build-dir`.

## Preparar Stanford Bunny

```bash
python3 scripts/prepare_bunny.py
```

Salida esperada:

```text
models/stanford-bunny/bun_zipper.ply
models/stanford-bunny/README.md
```

Fuente del dataset: Stanford 3D Scanning Repository,
https://graphics.stanford.edu/data/3Dscanrep/

## Smoke test

Antes de correr todos los experimentos:

```bash
python3 scripts/run_experiments.py --smoke
```

Esto ejecuta `models/triangle.obj` con resolucion baja y procesos `1,2`.

## Ejecutar todos los experimentos

Escala por defecto:

- procesos: `1,2,4,8`
- resoluciones: `32,64,96,128`
- repeticiones: `3`

Comando:

```bash
python3 scripts/run_experiments.py
```

Equivalente explicito para CLion:

```bash
python3 scripts/run_experiments.py --build-dir cmake-build-debug
```

En NixOS, se puede ejecutar dentro de un shell con CMake, OpenMPI, Assimp, Python y
matplotlib. En Windows se recomienda WSL2 o Docker para evitar friccion con MPI/Assimp
nativo.

Salidas:

```text
results/raw/
results/metrics.csv
```

`metrics.csv` contiene:

- `mesh`
- `resolution`
- `processes`
- `repeat`
- `triangles`
- `occupied_voxels`
- `tested_voxels`
- `estimated_flops`
- `load_seconds`
- `voxel_seconds`
- `estimated_flops_per_second`
- `speedup`
- `efficiency`

## Generar graficas

```bash
python3 scripts/plot_results.py
```

Salidas:

```text
results/plots/time_vs_processes.png
results/plots/speedup_vs_processes.png
results/plots/efficiency_vs_processes.png
results/plots/flops_vs_processes.png
results/plots/scalability_by_resolution.png
results/plots/theoretical_vs_experimental.png
```

## Configuracion recomendada en CLion

Crear una Run Configuration tipo Python o Shell Script para cada paso:

1. Preparar data:
   ```bash
   python3 scripts/prepare_bunny.py
   ```
2. Correr experimentos:
   ```bash
   python3 scripts/run_experiments.py --build-dir cmake-build-debug
   ```
3. Generar graficas:
   ```bash
   python3 scripts/plot_results.py
   ```

## Notas para el informe

- La metrica principal para tiempos es `voxel_seconds`, porque mide la region paralelizada.
- El speedup se calcula como `T1 / Tp` por cada resolucion.
- La eficiencia se calcula como `speedup / p`.
- La curva teorica usada en las graficas es el speedup ideal `Ts / p`.
- El tamano del problema se controla con la resolucion de la grilla.

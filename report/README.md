# Report

Punto de entrada para el informe parcial/final del proyecto de voxelizacion.

## Estructura sugerida

1. Problema y objetivo: convertir una malla triangular en grilla de voxels.
2. Metodo actual: voxelizacion conservadora por AABB de triangulo.
3. Paralelizacion principal: `Scatterv` de triangulos, grillas locales, `Reduce` OR.
4. Comparacion CPU: OpenMP con grilla privada por thread y OpenMP con grilla compartida atomica.
5. Analisis: `Ts`, `Tp`, speedup, eficiencia, overhead y escalabilidad.
6. Experimentos: Stanford Bunny/modelos simples, resoluciones, procesos/threads y repeticion de corridas.
7. Limitaciones: AABB sobreestima, desbalance con triangulos gigantes.
8. Trabajo futuro: test triangulo-caja, tile-based con filtrado espacial, CUDA, octree/sparse grid.

## Fuentes internas

- `docs/COURSE_ANALYSIS.md`: explicacion formal del algoritmo y el analisis.
- `docs/CODE_WALKTHROUGH.md`: guia para entender el codigo antes de adaptarlo.
- `README_2.md`: flujo de experimentos heredado de Stewart.
- `results/metrics.csv` o `results/<experimento>/metrics.csv`: metricas consolidadas.
- `results/plots/`: graficas para el informe.
- `report/REFERENCES.md`: fuentes externas sugeridas, citas APA y nota de uso de IA.

## Flujo LaTeX

Cuando se cree el informe en LaTeX, mantenerlo dentro de esta carpeta:

```text
report/main.tex
report/figures/
report/build/
```

Los PDFs finales pueden versionarse si son livianos y necesarios para entrega. Los
artefactos intermedios de LaTeX deben ignorarse.

## Siguientes pasos de limpieza

La version actual prioriza cerrar una implementacion defendible con tres modos:
`mpi`, `omp_private` y `omp_atomic`. Antes de una version final pulida, conviene:

1. Mover el parsing de CLI y el fallback OBJ fuera de `main.cpp`.
2. Compactar nombres para que el codigo se lea casi como pseudocodigo:
   `load -> partition -> voxelize -> reduce -> metrics`.
3. Separar `MeshIO`, `ExperimentOptions` y `RunStats` en helpers chicos.
4. Agregar `candidate_voxels_sum` y `grid_hash` al CSV para defender balance y correctitud.
5. Mantener CUDA, tile-based y octree como estado del arte/trabajo futuro salvo que haya tiempo para implementarlos y validarlos correctamente.

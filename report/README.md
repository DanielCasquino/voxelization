# Report

Punto de entrada para el informe parcial/final del proyecto de voxelizacion.

## Estructura sugerida

1. Problema y objetivo: convertir una malla triangular en grilla de voxels.
2. Metodo actual: voxelizacion conservadora por AABB de triangulo.
3. Paralelizacion: `Scatterv` de triangulos, grillas locales, `Reduce` OR.
4. Analisis: `Ts`, `Tp`, speedup, eficiencia y escalabilidad.
5. Experimentos: Stanford Bunny, resoluciones, procesos y repeticion de corridas.
6. Limitaciones: AABB sobreestima, desbalance con triangulos gigantes.
7. Trabajo futuro: interseccion exacta, particion hibrida, Morton/Z-order.

## Fuentes internas

- `docs/COURSE_ANALYSIS.md`: explicacion formal del algoritmo y el analisis.
- `docs/CODE_WALKTHROUGH.md`: guia para entender el codigo antes de adaptarlo.
- `README_2.md`: flujo de experimentos heredado de Stewart.
- `results/metrics.csv`: metricas consolidadas.
- `results/plots/`: graficas para el informe.

## Flujo LaTeX

Cuando se cree el informe en LaTeX, mantenerlo dentro de esta carpeta:

```text
report/main.tex
report/figures/
report/build/
```

Los PDFs finales pueden versionarse si son livianos y necesarios para entrega. Los
artefactos intermedios de LaTeX deben ignorarse.

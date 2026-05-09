# Code Walkthrough

Esta guia es para revisar el codigo antes de adaptarlo a un estilo mas tuyo. La idea no es
reescribir todavia, sino entender que hace cada bloque y que se puede compactar despues sin
romper el trabajo original de Daniel.

## Estilo objetivo

Referencia local: `q1.c` del otro curso. Rasgos utiles para este proyecto:

- flujo lineal y facil de seguir;
- nombres directos;
- comentarios cortos al lado de las decisiones importantes;
- pocas abstracciones si no reducen ruido real;
- funciones pequenas cuando encapsulan una idea clara.

Para este repo, el equivalente seria mantener:

```text
main.cpp          -> MPI, carga, reparto, reduccion, metricas
voxelize.h/cpp    -> estructuras, bounds, voxelizacion local
scripts/          -> experimentos y graficas
docs/             -> explicacion para exposicion
```

## `main.cpp`

### Includes

Incluye MPI, Assimp, utilidades STL y `voxelize.h`.

Lectura mental:

```text
MPI     -> procesos y comunicacion
Assimp  -> leer mallas
STL     -> vectores, strings, archivos
local   -> Triangle, Bounds, Voxelize
```

### Tipos MPI derivados

`CreateTriangleType()` dice a MPI que un `Triangle` son 9 `float` contiguos.
`CreateBoundsType()` dice que un `Bounds` son 6 `float` contiguos.

Por que existe: evita enviar bytes crudos y conecta con el curso: tipos derivados MPI.

Como lo explicaria en exposicion:

```text
En vez de serializar a mano, definimos el layout de las estructuras y MPI las comunica como
tipos del dominio.
```

### `BuildCounts` y `BuildDisplacements`

`BuildCounts(total, size)` calcula cuantos triangulos recibe cada rank.

La formula clave:

```cpp
start = total * rank / size;
end = total * (rank + 1) / size;
count = end - start;
```

Intuicion:

```text
rank k empieza cerca de k*(m/p)
```

La multiplicacion antes de dividir evita perder sobrantes por division entera.

`BuildDisplacements(counts)` convierte cantidades en offsets para `MPI_Scatterv`.

### Proyeccion 2D

`TestBit` revisa si un voxel esta ocupado en la grilla compacta.

`WriteZProjectionPGM` recorre cada columna `(x,y,*)`; si algun `z` esta ocupado, pinta el
pixel negro. Esto no es render 3D, solo una evidencia visual rapida.

### Inicializacion MPI

`MPI_Init`, `MPI_Comm_rank`, `MPI_Comm_size`.

Lectura mental:

```text
rank -> quien soy
size -> cuantos somos
```

Si MPI falla, aborta. Es ruido defensivo, pero correcto.

### Argumentos

Uso:

```bash
./voxelization <mesh-file> [resolution] [projection.pgm]
```

`resolution` controla `r`; la grilla tiene `r^3` voxels.

### Rank 0 carga la malla

Solo rank 0 usa Assimp:

1. Lee el archivo.
2. Fuerza triangulacion.
3. Copia vertices a `std::vector<Triangle>`.
4. Calcula `bounds`.
5. Mide `load_seconds`.

Esto conserva la idea base de Daniel: el programa empieza con una malla y la aplana a
triangulos.

### Reparto MPI

Todos reciben `triangle_count`.

Luego:

```text
counts/displacements -> plan de reparto
MPI_Scatterv          -> cada rank recibe sus triangulos
MPI_Bcast(bounds)     -> todos conocen el dominio global
```

`Scatterv` se usa porque el ultimo rank puede recibir uno mas cuando `m` no divide exacto a
`p`.

### Voxelizacion local

Cada rank llama:

```cpp
Voxelize(local_triangles, resolution, bounds, local_stats)
```

El punto importante: ya no todos procesan todo. Cada rank procesa su parte.

### Reduccion OR

Cada rank tiene una grilla local. Rank 0 necesita la union:

```cpp
MPI_Reduce(local_grid, global_grid, ..., MPI_BOR, 0, MPI_COMM_WORLD)
```

`MPI_BOR` hace OR bit a bit sobre palabras de 64 bits. Es justo la operacion semantica:
un voxel final esta ocupado si cualquier rank lo marco.

### Metricas

Se reducen por suma:

```text
tested_voxels
estimated_flops
```

Y el tiempo paralelo se reduce por maximo:

```text
voxel_seconds -> max entre ranks
```

Se usa max porque el programa termina cuando termina el rank mas lento.

## `include/voxelize.h`

Define las estructuras compartidas:

- `Triangle`: 3 vertices, cada vertice con `x,y,z`.
- `Bounds`: minimo y maximo global por eje.
- `VoxelStats`: contadores para metricas.

Tambien declara:

```cpp
Bounds ComputeBounds(...);
std::vector<uint64_t> Voxelize(...);
```

Esta separacion esta bien: `main` coordina MPI, `voxelize.cpp` hace geometria local.

## `src/voxelize.cpp`

### Helpers internos

`ClampIndex` convierte coordenada real a indice de voxel y asegura rango `[0, r-1]`.

`SetBit` marca un voxel en una grilla compacta.

`CountBits` cuenta voxels ocupados usando `popcount`.

### `ComputeBounds`

Recorre todos los vertices de todos los triangulos y calcula minimo/maximo por eje.

Si un eje queda degenerado, le suma `1.0f` para evitar division por cero.

### `Voxelize`

1. Crea una grilla local de bits.
2. Calcula el tamano de celda por eje.
3. Para cada triangulo, calcula su AABB.
4. Convierte ese AABB a indices de voxel.
5. Recorre todos los voxels candidatos y los marca.
6. Acumula `tested_voxels` y `estimated_flops`.

Limitacion actual:

```text
Marca todo el AABB del triangulo, no solo los voxels que intersectan exactamente el triangulo.
```

Esto es una voxelizacion conservadora. Sirve para beta paralela, pero la version final puede
reemplazar el predicado interno por interseccion triangulo-AABB.

## Que adaptar despues

Despues de revisar linea por linea, los cambios razonables de estilo serian:

- renombrar helpers si algun nombre no te sale natural;
- reducir boilerplate de checks MPI repetidos;
- agrupar prints de metricas en una funcion pequena;
- mantener comentarios cortos solo donde expliquen una decision paralela;
- no cambiar `Triangle`, `Bounds`, `Voxelize` ni el flujo MPI sin una razon clara.

No conviene hacer una reescritura grande antes de tener el informe y las metricas cerradas.

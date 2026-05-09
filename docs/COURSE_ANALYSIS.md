# Course Analysis: MPI Voxelization

Esta nota explica el proyecto con el lenguaje de CS4052: metodo, particionamiento,
comunicacion, PRAM, complejidad, speedup y eficiencia.

## Variables

- `m`: numero total de triangulos de la malla.
- `p`: numero total de procesos MPI.
- `rank`: identificador de un proceso MPI, en el rango `0..p-1`.
- `k`: variable matematica para hablar de un proceso cualquiera. En el codigo,
  `k` es lo mismo que `rank`.
- `r`: resolucion por eje de la grilla; la grilla completa tiene `r^3` voxels.
- `G_k`: grilla local producida por el proceso `k`.
- `G`: grilla final.

Ejemplo: si corro `mpirun -np 4`, entonces `p = 4` y existen ranks `0, 1, 2, 3`.

## Por que aparece `m*k/p`

Queremos repartir `m` triangulos entre `p` procesos sin solaparlos y sin perder ninguno.
La idea es partir el intervalo de indices `[0, m)` en `p` subintervalos contiguos.

Para el proceso `k`:

```text
start_k = floor(m*k/p)
end_k   = floor(m*(k+1)/p)
```

El proceso `k` trabaja los indices:

```text
[start_k, end_k)
```

El extremo derecho es abierto: incluye `start_k`, no incluye `end_k`.

Ejemplo con `m = 10` triangulos y `p = 3` procesos:

```text
k=0: start=floor(10*0/3)=0, end=floor(10*1/3)=3  -> indices 0,1,2
k=1: start=floor(10*1/3)=3, end=floor(10*2/3)=6  -> indices 3,4,5
k=2: start=floor(10*2/3)=6, end=floor(10*3/3)=10 -> indices 6,7,8,9
```

Propiedades:

1. No hay huecos: el `end` de un proceso es el `start` del siguiente.
2. No hay solapamiento: los intervalos son disjuntos.
3. Cubren todo: el primero empieza en `0` y el ultimo termina en `m`.
4. La carga queda balanceada: cada proceso recibe `floor(m/p)` o `ceil(m/p)` triangulos.

En el codigo, esto se materializa con `BuildCounts` y `BuildDisplacements`, y luego
se usa `MPI_Scatterv`.

## Por que usamos OR

La salida de voxelizacion es una grilla binaria:

```text
G[v] = 1 si el voxel v esta ocupado
G[v] = 0 si el voxel v esta vacio
```

Cada proceso ve solo algunos triangulos, por eso construye una grilla local:

```text
G_k[v] = 1 si algun triangulo del proceso k ocupa v
```

La grilla final debe decir si cualquier proceso encontro ocupacion:

```text
G[v] = G_0[v] OR G_1[v] OR ... OR G_{p-1}[v]
```

Esto funciona porque la ocupacion es una propiedad existencial:

```text
v esta ocupado <=> existe al menos un triangulo que toca v
```

Y el OR representa exactamente "existe al menos uno":

```text
0 OR 0 OR 0 = 0   nadie lo marco
0 OR 1 OR 0 = 1   alguien lo marco
1 OR 1 OR 0 = 1   varios lo marcaron, sigue siendo ocupado
```

Tambien es seguro si dos procesos marcan el mismo voxel: no necesitamos contar cuantas
veces ocurre; solo importa si esta ocupado o no.

En MPI usamos:

```cpp
MPI_Reduce(local_grid, global_grid, ..., MPI_UINT64_T, MPI_BOR, 0, MPI_COMM_WORLD);
```

La grilla esta empacada en palabras de 64 bits. `MPI_BOR` aplica OR bit a bit, de modo que
combina 64 voxels por palabra.

## Metodo Foster

### Partitioning

Particionamos por triangulos. Cada proceso recibe un bloque contiguo de la lista de
triangulos. Es una particion estatica y simple.

Ventaja: cada triangulo se procesa de forma independiente.

Riesgo: si algunos triangulos cubren muchos voxels y otros pocos, puede haber desbalance.

### Communication

El codigo usa tres comunicaciones colectivas:

1. `MPI_Scatterv`: reparte triangulos desde rank 0.
2. `MPI_Bcast`: todos reciben el bounding box global.
3. `MPI_Reduce + MPI_BOR`: rank 0 combina las grillas locales.

Ademas usa `MPI_Datatype` derivado para comunicar `Triangle` y `Bounds` como estructuras
del dominio, no como bytes crudos.

### Agglomeration

Cada proceso recibe varios triangulos, no una tarea por voxel. Esto sube la granularidad:
mas computo local por mensaje.

### Mapping

El mapping actual es:

```text
rank k -> triangulos [floor(m*k/p), floor(m*(k+1)/p))
```

## PRAM

Modelo conceptual: CRCW con resolucion por OR.

Por que:

- Muchos procesadores pueden leer triangulos concurrentemente.
- Muchos procesadores podrian marcar el mismo voxel.
- Si todos escriben el mismo valor `1`, el conflicto se resuelve naturalmente con OR.

En MPI no escribimos realmente en memoria compartida. Simulamos ese PRAM con grillas locales
y una reduccion OR final.

## Complejidad

Sea:

- `b_i`: cantidad de voxels candidatos para el triangulo `i`.
- `B = sum_i b_i`: total de voxels candidatos.
- `w = ceil(r^3 / 64)`: palabras de 64 bits de la grilla.
- `alpha`: latencia.
- `beta`: costo por dato.

Secuencial:

```text
Ts = O(m + B)
```

Paralelo:

```text
Tcomp = O(B/p)
Tcomm_scatter = O(p(alpha + (m/p) beta))
Tcomm_bounds  = O(log p (alpha + beta))
Tcomm_reduce  = O(log p (alpha + w beta))

Tp = O(B/p + p(alpha + (m/p) beta) + log p(alpha + w beta))
```

Speedup:

```text
S = Ts / Tp
```

Eficiencia:

```text
E = S / p
```

Conclusion esperada:

- Con `r` y `m` pequenos, la comunicacion puede dominar.
- Con modelos grandes, como Stanford bunny, hay mas computo para amortizar comunicacion.
- La reduccion de la grilla puede ser el cuello de botella cuando `r` crece mucho.

## Visualizacion

El tercer argumento del binario escribe una proyeccion `.pgm`:

```bash
mkdir -p outputs
mpirun -np 4 ./build/voxelization models/stanford-bunny/bun_zipper.ply 96 outputs/bunny_96.pgm
```

La imagen es una proyeccion sobre el eje `z`: si existe algun voxel ocupado en una columna
`(x,y,*)`, el pixel sale negro.

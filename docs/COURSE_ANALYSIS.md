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

Por que no memoria compartida para el bounding box:

- MPI esta pensado para memoria distribuida. Dos ranks pueden estar en la misma maquina,
  pero tambien podrian estar en nodos distintos. `MPI_Bcast` funciona en ambos casos.
- El bounding box es muy pequeno: son solo dos puntos 3D, o sea 6 numeros `float`. El costo
  `O(log p)` existe en el modelo, pero en la practica es despreciable frente al reparto de
  triangulos y la reduccion de una grilla de `r^3` bits.
- Usar memoria compartida aqui complicaria el diseno sin ganar casi nada. Si optimizamos,
  conviene mirar primero la reduccion de la grilla y el balance de carga.

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

PRAM es un modelo teorico de paralelismo con memoria compartida. Sirve para razonar
algoritmos sin preocuparse todavia por detalles de MPI, cache o red.

CRCW significa:

- Concurrent Read: varios procesadores pueden leer la misma posicion de memoria al mismo
  tiempo.
- Concurrent Write: varios procesadores pueden intentar escribir la misma posicion de
  memoria al mismo tiempo.

El problema de CRCW es: si dos procesadores escriben distinto valor en la misma posicion,
quien gana? Hay varias reglas posibles. En este proyecto usamos una regla tipo OR:

```text
si cualquiera escribe 1, el resultado final es 1
```

Eso calza con voxelizacion porque un voxel solo necesita saber si esta ocupado.

Por que:

- Muchos procesadores pueden leer triangulos concurrentemente.
- Muchos procesadores podrian marcar el mismo voxel.
- Si todos escriben el mismo valor `1`, el conflicto se resuelve naturalmente con OR.

En MPI no escribimos realmente en memoria compartida. Simulamos ese PRAM con grillas locales
y una reduccion OR final.

Sobre `MPI_Win_allocate_shared`: si, se puede usar, pero no reemplaza limpiamente esta
reduccion en todos los casos.

- `MPI_Win_allocate_shared` crea memoria compartida solo entre ranks del mismo nodo fisico.
  Si ejecutamos en varias maquinas, ya no basta.
- Si muchos ranks escriben la misma grilla compartida, necesitamos sincronizacion o atomicos
  para evitar carreras. Para bits empacados en `uint64_t`, dos procesos podrian actualizar
  bits distintos de la misma palabra y pisarse si no se hace OR atomico.
- MPI RMA tiene operaciones atomicas como `MPI_Fetch_and_op`/`MPI_Accumulate`, pero eso
  vuelve el codigo mas complejo y puede ser mas lento si hay mucha contencion.

Por eso, para el curso y para una primera version defendible, es mejor:

```text
cada rank escribe su grilla local sin locks
al final se combinan todas con MPI_Reduce + MPI_BOR
```

Es mas simple de explicar, portable a clusters y correcto por construccion.

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

Notas sobre esos terminos:

- `B` no es la cantidad de voxels de la grilla final. La grilla final tiene `r^3` voxels.
  `B` es el trabajo total que hacen los triangulos: para cada triangulo miramos su bounding
  box discreto, y `b_i` cuenta cuantos voxels candidatos revisa ese triangulo. Entonces
  `B = sum_i b_i`. Si los triangulos son chicos, `B` puede ser mucho menor que `m*r^3`.
  Si hay un triangulo enorme, su `b_i` puede ser grande.
- `B/p` asume balance ideal: cada proceso recibe aproximadamente la misma cantidad de
  trabajo. El codigo reparte por numero de triangulos, no por `b_i`, asi que puede haber
  desbalance si algunos triangulos cubren muchos mas voxels que otros.
- `beta` se usa aqui como segundos por dato, no datos por segundo. En varios libros aparece
  como el inverso del bandwidth. Si mando `n` datos, el costo se modela como
  `alpha + n*beta`.
- `Tcomm_bounds` usa `log p` porque un broadcast eficiente suele implementarse como arbol:
  rank 0 envia a algunos ranks, esos reenvian a otros, y asi. No necesariamente manda `p`
  mensajes directos desde rank 0.
- `Tcomm_reduce` tambien suele ser un arbol, pero al reves: las hojas mandan grillas
  parciales, nodos intermedios combinan con OR, y rank 0 recibe el resultado final.

Por que `Reduce` y no `Gather`:

- `MPI_Gather` juntaria todas las grillas completas en rank 0. Luego rank 0 tendria que
  hacer el OR secuencialmente.
- `MPI_Reduce` combina mientras comunica. Los ranks intermedios ya van aplicando OR, por
  eso llega a rank 0 una sola grilla final.
- Usamos `Gather` cuando necesitamos conservar todos los resultados individuales. Usamos
  `Reduce` cuando necesitamos una agregacion: suma, maximo, minimo, OR, AND, etc.

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

Eso significa: paralelizar conviene mas cuando hay suficiente trabajo por proceso. Si el
modelo es muy pequeno, el programa puede pasar mas tiempo comunicando que calculando, y
usar mas procesos no necesariamente mejora. Si el modelo tiene muchos triangulos o la
resolucion sube, hay mas trabajo local y el overhead de comunicacion pesa menos.

Pero tampoco es "mas grande siempre es mejor". Cuando `r` crece, la grilla tiene `r^3`
voxels y la reduccion mueve `ceil(r^3/64)` palabras. Por eso puede aparecer otro cuello de
botella: combinar las grillas.

Sobre el triangulo gigante: si un triangulo cubre muchisimos voxels, repartir por triangulos
puede dejar a un rank con mucho mas trabajo. Hay alternativas:

1. Particion dinamica por triangulos: los ranks piden mas triangulos cuando terminan. Mejora
   balance, pero requiere un esquema master-worker o colas distribuidas.
2. Particionar por bloques espaciales: dividir la grilla en tiles 3D y asignar bloques a
   procesos. Un triangulo grande puede tocar varios bloques, pero el trabajo se reparte
   mejor. A cambio, hay que decidir que triangulos intersectan cada bloque.
3. Estimar costo por bounding box: antes de repartir, calcular `b_i` para cada triangulo y
   hacer particion balanceada por suma de costos, no por cantidad de triangulos.
4. Hibrido: mantener MPI por rangos de triangulos para la beta, y proponer balance por
   costo `b_i` como mejora futura.

Para este proyecto, la opcion 3 es una mejora razonable para mencionar en el informe:
conserva la estructura actual y conecta directamente con el analisis `B = sum_i b_i`.
Implementarla no es imprescindible para la primera version, pero si nos piden escalabilidad,
es la mejora mas facil de defender.

## Visualizacion

El tercer argumento del binario escribe una proyeccion `.pgm`:

```bash
mkdir -p outputs
mpirun -np 4 ./build/voxelization models/stanford-bunny/bun_zipper.ply 96 outputs/bunny_96.pgm
```

La imagen es una proyeccion sobre el eje `z`: si existe algun voxel ocupado en una columna
`(x,y,*)`, el pixel sale negro.

Si queremos ver algo 3D en Nix hay varias opciones:

1. Simple para ahora: generar proyecciones 2D (`.pgm`/`.png`) desde distintos ejes. Sirve
   para evidencia rapida y no agrega dependencias pesadas al proyecto.
2. Intermedio: exportar voxels ocupados como `.obj`, dibujando cada voxel como cubo o cada
   voxel como punto. Luego se puede abrir con MeshLab, Blender o cualquier visor 3D.
3. Mas pesado: instalar/usar Blender desde Nix y renderizar una imagen. Es buena evidencia
   visual, pero no deberia ser dependencia obligatoria del codigo paralelo.

Para la exposicion, lo mas pragmatico es exportar `.obj` o `.ply` de voxels ocupados y abrirlo
con Blender/MeshLab solo como herramienta de visualizacion. El algoritmo paralelo no debe
depender de Blender.

## Morton / Z-order

Morton order, tambien llamado Z-order, reordena coordenadas 3D intercalando bits de `x`, `y`
y `z`. La idea es que voxels cercanos espacialmente tiendan a quedar cerca en memoria.

Opinion para el proyecto: si, conviene dejarlo como mejora futura u out of scope, no como
requisito principal.

Por que:

- Nuestra comunicacion principal actual reduce toda la grilla con OR. Cambiar el orden de
  indices no reduce automaticamente la cantidad total de datos enviados.
- Morton puede mejorar localidad de cache y ayudar si despues particionamos por bloques
  espaciales, pero complica la explicacion y el codigo.
- Para el curso, es mas fuerte presentar primero una paralelizacion correcta y medible:
  `Scatterv` por triangulos, computo local, `Reduce` con OR, analisis `Tp`, `S`, `E`.

Como frase de informe:

```text
Se deja Morton/Z-order como trabajo futuro. Puede mejorar localidad de memoria y servir para
una particion espacial por bloques, pero no cambia por si sola el costo asintotico de reducir
la grilla completa en la version actual.
```

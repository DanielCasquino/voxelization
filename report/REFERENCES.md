# Referencias para el informe

Este archivo es una guia para armar la seccion de bibliografia del informe parcial.
La rubrica del proyecto CPD 2026-I pide describir las fuentes usadas, incluyendo IA, y
explicar su impacto en el desarrollo.

## Que pide la entrega parcial

La entrega parcial no reemplaza el desarrollo de codigo. Hay dos niveles en el PDF:

1. Para el proyecto Voxelization, el desarrollo debe incluir PRAM, codigo paralelo en
   C++, registro de por lo menos 3 versiones beta, tiempos, speedup, FLOPs vs. `p` y
   analisis de escalabilidad.
2. Para el avance parcial, el documento que se evalua debe explicar el estado de ese
   desarrollo en formato de informe.

El informe parcial debe contener:

- nombre del proyecto e integrantes;
- porcentaje de participacion de cada integrante;
- introduccion del proyecto y solucion planteada;
- metodo: algoritmo/PRAM y metricas legibles desde el pseudocodigo;
- bibliografia: fuentes usadas y su impacto, incluyendo IA.

La rubrica parcial tambien califica presentacion oral. Por eso, para Canvas conviene
subir el informe y los archivos del proyecto que evidencien el avance: codigo, docs,
metricas, graficas y registro de versiones/betas.

## Fuentes tecnicas a citar

### Dataset: Stanford Bunny

Usar para justificar el modelo de prueba principal.

Referencia APA sugerida:

Stanford Computer Graphics Laboratory. (n.d.). *The Stanford 3D Scanning Repository*.
Stanford University. https://graphics.stanford.edu/data/3Dscanrep/

En el texto:

> Se uso Stanford Bunny como malla de prueba por ser un dataset estandar de graficos
> por computadora (Stanford Computer Graphics Laboratory, n.d.).

Impacto en el proyecto:

> Esta fuente aporto el modelo `bun_zipper.ply` usado para medir tiempos, speedup,
> eficiencia y escalabilidad con una malla triangular real.

### MPI y comunicacion paralela

Usar para justificar `MPI_Scatterv`, `MPI_Bcast`, `MPI_Reduce` y la reduccion por OR.

Referencias APA sugeridas:

Open MPI Project. (n.d.). *MPI_Scatterv(3) man page*. https://www.open-mpi.org/doc/v4.0/man3/MPI_Scatterv.3.php

Open MPI Project. (n.d.). *MPI_Bcast(3) man page*. https://www.open-mpi.org/doc/v4.1/man3/MPI_Bcast.3.php

Open MPI Project. (n.d.). *MPI_Reduce(3) man page*. https://www.open-mpi.org/doc/current/man3/MPI_Reduce.3.php

En el texto:

> La distribucion de triangulos se implementa con `MPI_Scatterv`, el envio de
> metadatos globales con `MPI_Bcast` y la fusion de grillas locales con
> `MPI_Reduce` usando una operacion OR sobre voxeles ocupados (Open MPI Project,
> n.d.).

Impacto en el proyecto:

> Estas fuentes se usaron para validar que la estrategia de comunicacion corresponde
> al modelo de memoria distribuida: cada proceso recibe un bloque de triangulos,
> calcula una grilla local y el proceso raiz obtiene la grilla final con una reduccion.

### Assimp

Usar para justificar la lectura de modelos 3D.

Referencia APA sugerida:

Open Asset Import Library. (n.d.). *Assimp: Open Asset Import Library*. https://www.assimp.org/

En el texto:

> La carga de mallas se apoya en Assimp, que convierte varios formatos 3D a una
> estructura comun en memoria (Open Asset Import Library, n.d.).

Impacto en el proyecto:

> Esta fuente justifica por que el codigo puede trabajar con modelos externos sin
> escribir un parser propio de OBJ/PLY.

### Voxelizacion conservadora y prueba triangulo-caja

Usar para explicar la idea geometrica detras de marcar voxeles candidatos alrededor de
cada triangulo. El codigo actual usa una version simple por AABB; estas referencias
sirven como fundamento y como trabajo futuro para reemplazar la sobreestimacion por una
prueba geometrica mas exacta.

Referencias APA sugeridas:

Akenine-Moller, T. (2001). Fast 3D triangle-box overlap testing. *Journal of Graphics
Tools, 6*(1), 29-33. https://doi.org/10.1080/10867651.2001.10487535

Schwarz, M., & Seidel, H.-P. (2010). Fast parallel surface and solid voxelization on
GPUs. *ACM Transactions on Graphics, 29*(6), Article 179.
https://doi.org/10.1145/1882261.1866201

En el texto:

> La voxelizacion de mallas triangulares suele formularse como el marcado de voxeles
> intersectados por triangulos. En este avance se usa una aproximacion conservadora
> por caja envolvente alineada a la grilla; una mejora natural seria aplicar una prueba
> triangulo-caja como la de Akenine-Moller (2001), relacionada con tecnicas modernas
> de voxelizacion paralela (Schwarz & Seidel, 2010).

Impacto en el proyecto:

> Estas fuentes ayudaron a separar lo que el codigo implementa actualmente de lo que
> seria una mejora: el AABB es simple y paralelizable, pero marca voxeles de mas; una
> prueba triangulo-caja reduce falsos positivos a mayor costo por voxel candidato.

### Diseno de programas paralelos

Usar para conectar el analisis con el curso: particion, comunicacion, agregacion de
resultados y metricas de rendimiento.

Referencia APA sugerida:

Foster, I. (1995). *Designing and building parallel programs: Concepts and tools for
parallel software engineering*. Addison-Wesley. https://www.mcs.anl.gov/~itf/dbpp/

En el texto:

> El diseno sigue el patron de descomponer el trabajo en tareas independientes,
> asignarlas a procesos y combinar resultados con comunicacion colectiva (Foster, 1995).

Impacto en el proyecto:

> Esta fuente apoyo la decision de paralelizar por triangulos: la mayor parte del
> trabajo local no depende de otros procesos y la dependencia global aparece solo al
> fusionar la grilla final.

## Como citar IA

La IA no debe aparecer como si fuera una fuente academica que prueba un hecho. En este
proyecto conviene citarla como herramienta de apoyo, porque la rubrica pide justificar
fuentes externas incluyendo IA.

Referencia APA sugerida:

OpenAI. (2026). *ChatGPT* [Large language model]. https://chat.openai.com/

Nota de uso sugerida para el informe:

> Se utilizo ChatGPT/Codex de OpenAI como asistente para organizar la documentacion,
> revisar la derivacion de complejidad, proponer estructura del informe y explicar el
> codigo MPI. Las respuestas fueron verificadas contra el codigo del repositorio, la
> rubrica del curso, documentacion oficial de MPI/Assimp y fuentes tecnicas citadas.
> La IA no fue usada como fuente primaria de resultados experimentales; las metricas y
> graficas provienen de ejecuciones del codigo del proyecto.

Si el profesor pide prompts, agregar un apendice breve:

```text
Anexo: uso de IA

Herramienta: ChatGPT/Codex de OpenAI.
Fecha de uso: mayo de 2026.
Uso: apoyo para organizar informe, explicar el algoritmo, revisar complejidad y preparar
referencias.
Prompts representativos:
- "Explica por que la complejidad secuencial de la voxelizacion es O(m + B)."
- "Conecta el codigo MPI de voxelizacion con PRAM, speedup, eficiencia y escalabilidad."
- "Prepara referencias APA para Stanford Bunny, MPI, Assimp y voxelizacion conservadora."
Validacion: las salidas se contrastaron con el codigo, la rubrica CPD 2026-I y fuentes
tecnicas externas.
```

## Referencias que no conviene poner

- No citar Wikipedia si ya existe la fuente original.
- No citar ChatGPT como prueba de que una formula o algoritmo es correcto.
- No poner referencias inventadas. Si no hay enlace, DOI o fuente verificable, no entra.
- No mezclar modelos no usados en experimentos como si fueran resultados reales.

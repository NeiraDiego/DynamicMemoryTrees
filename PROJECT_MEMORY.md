# Dynamic Memory Trees: Project Memory

## Objective

Build compact dynamic C++ data structures that can insert and remove bits. The
long-term target is the dynamic range min-max tree for balanced-parentheses
ordinal trees described in Section 6 of Navarro and Sadakane's *Fully
Functional Static and Dynamic Succinct Trees*.

El proyecto se separa deliberadamente en dos capas: una secuencia dinámica de
bits compacta y un índice de agregados sobre esa secuencia. Esta separación
permite validar el manejo de memoria antes de añadir las operaciones de árboles
ordinales.

## Decisions made on 2026-11-18

- Use C++23 and CMake.
- The first executable milestone is a CPU `dynamic_bitvector`, not the full
  ordinal tree.
- Prioritize correctness, invariant checks, tests, and memory measurements over
  immediately meeting the paper's exact `2n + O(n/log n)` bound.
- Store persistent references as compact relative handles, never as raw memory
  pointers. This keeps a future GPU/serialization representation feasible.

## Implemented first milestone

- Segments hold from `L` to `2L` bits, except that a whole vector of at most
  `2L` bits is allowed in one shorter segment.
- A size-class pool packs equal-length segments contiguously in fixed `2L`-bit
  cells. A 32-bit handle encodes the size class and ordinal within that pool.
- Removing a pool entry moves the class's final entry into its place. Internal
  back-pointers update the moved AVL leaf; these pointers are CPU-only and are
  not part of the persistent representation.
- An AVL tree indexes segments by accumulated bit length. It supports access,
  overwrite, insertion, and deletion.
- If deletion creates an underfull segment, the current implementation rebuilds
  the segment partition. This keeps the invariant but is intentionally a
  correctness-first fallback, not the final `O(log n)` neighbor rebalance.

## Arquitectura actual

### Capa de almacenamiento

La secuencia se parte en segmentos. Cada segmento contiene los bits verbatim,
sin codificación adicional, y tiene longitud entre `L` y `2L`. Solo la raíz
puede ser más corta que `L`, cuando toda la secuencia tiene a lo sumo `2L`
bits. `L` se pasa al constructor y actualmente admite valores de 1 a 256.

Los segmentos del mismo tamaño pertenecen al mismo `Pool`. Cada `Pool` los
empaqueta uno detrás de otro en `Cell`s de `2L` bits. El primer segmento no
tiene por qué comenzar al inicio de una celda y un segmento puede cruzar el
límite entre dos celdas. Por tanto, la capacidad no se desperdicia por cada
segmento individual: solamente puede quedar espacio sin usar al final de cada
pool.

La ubicación persistente de un segmento es un `Handle` de 32 bits. Sus 9 bits
superiores codifican la clase de tamaño y sus 23 bits inferiores codifican el
ordinal del segmento dentro de ese pool. El offset físico se deriva como
`ordinal * longitud_del_segmento`; no se guarda un puntero a la celda ni un
offset de máquina de 64 bits.

Al liberar un segmento, el último segmento del mismo pool se copia sobre el
hueco liberado. Este procedimiento mantiene el pool compacto. Como el segmento
movido cambia de ordinal, se actualiza su handle mediante un back-pointer de
CPU al nodo hoja que lo referencia. Ese back-pointer es metadato transitorio y
no forma parte del formato persistente o serializable.

### Capa de índice

Un árbol AVL binario balanceado ordena las hojas según la posición de sus
segmentos en la secuencia. Las hojas contienen un handle y la longitud del
segmento. Los nodos internos no almacenan bits: combinan los metadatos de sus
dos hijos.

Cada nodo contiene el número total de bits de su subárbol. Así, una posición
global se localiza descendiendo por el AVL: si está dentro de la longitud del
hijo izquierdo se visita ese hijo; de lo contrario se resta esa longitud y se
visita el derecho. Inserciones, borrados y cambios de bit actualizan el camino
hasta la raíz. Las rotaciones AVL vuelven a calcular los agregados de los nodos
afectados.

Al insertar un bit en un segmento de `2L` bits, se divide en dos segmentos de
al menos `L` bits. Al borrar y producir un segmento menor que `L`, la versión
actual reconstruye la partición completa de segmentos. Es correcta pero no
cumple todavía el rebalanceo local en `O(log n)` del paper.

### Capa Range Min-Max

El mismo AVL funciona como Range Min-Max Tree. La interpretación actual es la
de una secuencia de paréntesis: bit `1` suma `+1` y bit `0` suma `-1`. Para
cada nodo se guardan agregados relativos al inicio de su subárbol:

- `e` o `excess`: suma total de los deltas del subárbol.
- `m` o `minimum`: mínimo de los excesos prefijos después de consumir un bit.
- `M` o `maximum`: máximo de los excesos prefijos después de consumir un bit.
- `n` o `minimum_count`: número de veces que aparece el mínimo `m`.
- `ones`: número de bits con valor `1`.

Para dos hijos `left` y `right`, los valores se combinan así:

```text
e = e(left) + e(right)
m = min(m(left), e(left) + m(right))
M = max(M(left), e(left) + M(right))
n = n(left) si m(left) < e(left) + m(right)
    n(right) si m(left) > e(left) + m(right)
    n(left) + n(right) en otro caso
```

Estos valores son locales al subárbol. El exceso global de una posición se
obtiene acumulando los excesos de los hermanos izquierdos recorridos desde la
raíz. Esta elección simplifica actualizaciones y rotaciones.

## Public scope for milestone one

- `size`, `empty`, `at`, `set`, `insert`, `erase`, `to_vector`, and memory
  statistics.
- Differential randomized tests against a plain bit vector.

## API y semántica

- `size()` y `empty()`: número de bits y comprobación de secuencia vacía.
- `at(position)`: devuelve el bit en una posición de base cero.
- `set(position, value)`: sobrescribe un bit y recalcula los agregados del
  camino afectado.
- `insert(position, value)`: inserta antes de `position`. Se admite insertar
  en `size()` para añadir al final.
- `erase(position)`: elimina el bit de base cero en `position`.
- `to_vector()`: materializa la secuencia para depuración, pruebas o exportación
  temporal; no es una operación succinta.
- `range_min_max()`: devuelve `e`, `m`, `M` y `n` de toda la secuencia.
- `excess_before(position)`: exceso de los bits en el rango `[0, position)`;
  `position == size()` es válido.
- `rank1(position)`: número de unos en `[0, position)`.
- `select1(occurrence)`: posición del `occurrence`-ésimo uno, contando desde
  cero. Devuelve `nullopt` si no existe.
- `find_first_excess(begin, target)`: primera posición `i >= begin` cuyo exceso
  global después de consumir el bit `i` es `target`. Devuelve `nullopt` si no
  existe.
- `find_close(opening_position)`: si el bit en `opening_position` es `1`, busca
  el primer retorno al exceso existente inmediatamente antes de ese bit. En una
  secuencia de paréntesis correctamente balanceada, es su paréntesis de cierre.

Las operaciones que reciben posiciones inválidas lanzan `std::out_of_range`.
Las búsquedas y `select1` usan `std::optional` para representar resultados
ausentes que no son errores de rango.

## Visualización de árboles pequeños

`include/dmt/tree_visualizer.hpp` expone
`visualize_small_tree(const DynamicBitVector&)`. La función valida que la
secuencia sea un único árbol de paréntesis balanceados y produce una
representación ASCII del árbol ordinal. El primer `1` crea la raíz y el último
`0` la cierra, pero no se imprimen como aristas: los `1` internos crean hijos y
los `0` internos retornan al padre. Los nodos se numeran por orden de apertura.
Si la secuencia no es válida, la función lanza `std::invalid_argument`.

## Glosario

- **Bitvector dinámica**: secuencia de bits que admite inserción y borrado, a
  diferencia de una bitmap estática de longitud fija.
- **Segmento**: bloque lógico contiguo de la secuencia, almacenado verbatim.
  Es la unidad que apunta una hoja del AVL.
- **`L`**: límite inferior de longitud de segmento. Una celda mide `2L` bits.
  En el paper se propone `L = log^2 n`; en esta implementación es configurable
  para facilitar pruebas y mediciones.
- **Cell**: bloque físico fijo de `2L` bits que contiene datos de uno o más
  segmentos del mismo pool. No es un nodo del árbol.
- **Pool**: clase de tamaño para todos los segmentos de una misma longitud. Es
  dueño de sus cells y de la lista de propietarios de sus entradas compactadas.
- **Size class**: índice de un pool. La clase está determinada por la longitud
  exacta del segmento, no por un intervalo redondeado.
- **Handle**: referencia relativa compacta de 32 bits a una entrada de pool. No
  es un puntero de memoria. En el código se llama `Handle`; el término correcto
  es *handle*, no *handler*.
- **Ordinal**: índice de un segmento dentro de su pool. Junto con la clase de
  tamaño permite reconstruir su posición física.
- **Back-pointer**: puntero interno desde una entrada de pool al nodo hoja que
  la posee. Solo se usa al compactar un pool para reparar el handle del
  segmento movido.
- **AVL**: árbol binario autoequilibrado. Su diferencia de alturas entre hijos
  se mantiene a lo sumo en uno mediante rotaciones.
- **Hoja**: nodo AVL que representa exactamente un segmento y contiene su
  handle.
- **Nodo interno**: nodo AVL que representa la concatenación de sus hijos y
  conserva longitudes y agregados combinados.
- **Exceso**: suma de `+1` por bit `1` y `-1` por bit `0`. Para paréntesis, es
  la profundidad relativa.
- **Exceso prefijo**: exceso acumulado después de procesar cada bit desde el
  inicio de un rango.
- **Range Min-Max Tree**: índice jerárquico que conserva mínimo y máximo de
  excesos por rango. Permite descartar subárboles enteros durante búsquedas.
- **`rank1`**: cantidad de unos antes de una posición.
- **`select1`**: posición de una ocurrencia de uno según su ordinal.
- **Forward search**: búsqueda hacia la derecha de un valor de exceso objetivo.
- **`nullopt`**: valor de `std::optional` que expresa que una búsqueda válida no
  encontró resultado.

## Implemented range min-max layer

- The AVL now stores per-node excess `e`, minimum `m`, maximum `M`, and the
  occurrence count `n` for `m`, using the composition formulas from the paper.
- The public API exposes aggregate values, prefix excess, `rank1`, `select1`,
  forward search for a global excess, and matching-close lookup.
- In this implementation `1` contributes `+1` and `0` contributes `-1`.

## Deferred work

- Local, worst-case `O(log n)` merge/borrow handling for underfull segments.
- Dynamic changes to `L` and the deamortized three-part technique.
- Universal lookup tables and word-parallel local rank/select/search.
- Backward search, RMQ position queries, and the remaining balanced-parentheses
  and ordinal-tree operations.
- GPU upload/view and GPU kernels.

## Design constraints for later phases

- Do not add persisted raw pointers to segment, node, or allocator layouts.
- Keep CPU-only maintenance metadata separate from serializable arena data.
- Extend AVL aggregates in-place for the range min-max tree rather than adding
  a second indexing tree.
- Benchmarks must report useful bits, cell capacity, pool metadata, and node
  metadata separately.
- El formato destinado a GPU debe describir pools, cells y handles mediante
  arrays planos de tamaños definidos; no puede depender de `std::unique_ptr`,
  `std::vector` ni de los back-pointers de CPU.
- La semántica actual de `m` y `M` excluye el exceso inicial cero del rango y
  considera prefijos después de consumir un bit. Cualquier futura RMQ debe
  mantener esta convención o documentar explícitamente una conversión.

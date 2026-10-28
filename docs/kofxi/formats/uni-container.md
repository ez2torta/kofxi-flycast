# Contenedor UNI — índice de secciones

- **Estado del traspaso:** `IN_REVIEW`.
- **Afirmación:** la fuente describe UNI de personajes como contenedor binario
  con índice de secciones y datos de personajes/recursos. El formato permite
  localizar secciones, pero el parser debe validar offsets y tamaños antes de
  consumirlos.
- **Dominio:** archivos `.UNI` examinados en el trabajo origen; no asumir todas
  las variantes KOF, regiones ni otros juegos Atomiswave.
- **No demuestra:** que cada sección esté decodificada, que un parser sea seguro,
  ni que layouts de PS2 sean idénticos a AW.
- **Fuentes:** `SRC-030e72078152` (`docs/architecture/formats/uni.md`);
  corpus referido allí `SRC-7e05c5887661` (`docs/research/animation/uni-structure-corpus.md`).
- **Corrección importante:** el documento antiguo conserva modelos refutados de
  Sec10. Para animaciones usar exclusivamente la síntesis con sus correcciones en
  [`uni-animation-model.md`](uni-animation-model.md), no tablas obsoletas de
  `uni.md`.

## Estructura descrita por la fuente

La fuente informa cabecera de `0x60` bytes, cuenta de secciones indicada como 22,
offset del bloque de configuración en `0x60` y entradas de sección empaquetadas
en `u32`: byte alto identifica sección y 24 bits bajos dan offset absoluto dentro
del archivo. En corpus citado aparecen IDs 1–20 y 22; no aparece ID 21.

Estos son **claims del documento origen**, no código wire ya aprobado. Antes de
implementar: comprobar si el conteo 22 significa slots/cantidad frente a IDs
presentes, endianess, offsets válidos, duplicados, orden, tamaños, overflow,
aliases y archivos truncados. La lectura debe usar límites de archivo y rechazar
rangos fuera de bounds sin overflow.

## Procedencia y validación pendiente

El origen registra una auditoría histórica de 43 archivos UNI y remite a
`docs/research/animation/uni-structure-corpus.md`. No se migraron los archivos de
juego ni el corpus binario. Obtener, si es redistribuible, un fixture sintético
pequeño o una referencia legal generada localmente, con SHA y resultado esperado.
Revisar explícitamente revisiones/refutaciones dentro del doc fuente antes de
marcar `MIGRATED`. Consúltese el claim ledger para cada afirmación atómica.

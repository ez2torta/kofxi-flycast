# Resolución runtime del actor player — alcance AW/PS2 separado

- **Estado:** `BLOCKED` para consumo por código; síntesis inicial de fuentes.
- **Pregunta:** cómo resolver actores de KOF XI en la RAM guest de Atomiswave
  para overlays read-only sin dirección host fija.
- **Fuentes:** `SRC-51e5e81d506e` (`docs/plans/plan_estudio_flycast.md`),
  `SRC-78f88ea4b99f` (`docs/architecture/runtime/player-memory-map.md`, PS2),
  `SRC-eaf28803d338` (`docs/architecture/runtime/uni-ram.md`).
- **No demuestra:** layout universal entre AW/PS2, versiones/regiones, chain válida
  en todo estado, ni acceso seguro desde el core. Las capturas citadas son
  históricas; no se portaron ni reprodujeron aquí.

## Espacios que no deben confundirse

1. **Dirección virtual SH-4 guest:** por ejemplo valores `0x8Cxxxxxx` citados en
   los estudios. Son valores que usa el juego/emulador.
2. **Offset/dirección física guest:** resultado de traducción SH-4 hacia RAM AW;
   depende de mapa y alias emulado.
3. **Índice relativo a un bloque:** por ejemplo offset de campo dentro del actor
   o sección de un UNI cargado.
4. **Dirección host:** puntero de memoria del proceso Flycast; cambia por ASLR,
   allocator, arquitectura y sesión. No persistir, transmitir ni compilar en una
   tabla del perfil.

La fórmula de la herramienta externa de captura es un método histórico para
traducir una dirección SH-4 en una RAM encontrada por firma `MUTEKI`; no se
promueve a API de runtime. El core debe usar su traducción/lectura guest propia y
validar rangos/alias en el tick apropiado.

## Hallazgos Atomiswave reportados, aún candidatos

El plan de estudio AW reporta:

- `camera` de 8 bytes y bloques `team` de `0x1F8` en ubicaciones observadas
  `0x27CAA8`, `0x27CB50` y `0x27CD48` (el documento asocia dos teams).
- Un actor de tamaño aproximado `0x584`; siete slots de cajas de 10 bytes a
  `player+0x314`, máscara activa citada en `+0x39E`.
- Cadena de resolución: `team.entries[point] → entry.data → −0x614`.
- Campos citados incluyen acción `+0xEC`, categoría `+0x204`, referencias de
  animación/scripts alrededor de `+0x200/+0x208/+0x250/+0x3F8/+0x3FC`, selector
  de banco `+0x226`, offsets de sprite `+0x2A8/+0x2AA`.

Son transcripciones del plan fuente, no un struct ABI aprobado. En concreto, las
ubicaciones absolutas RAM son **ejemplos de una muestra**, no constantes: pueden
cambiar por boot, asignador, personaje, orden o versión. El resolver debe hallar
las raíces desde el contexto guest y validar índices/punteros antes de cada
lectura; guardar identidad/build y estados de jugador presentes/ausentes.

## No fusionar con el mapa PS2

`player-memory-map.md` se identifica explícitamente como captura de memoria
PCSX2/PS2. Su tabla describe offsets relativos útiles como hipótesis de campos,
pero no prueba que AW conserve tamaño, anchura ni semántica. Hay contradicciones
que requieren resolución, por ejemplo:

| Campo | Plan/captura AW reportada | Otra fuente de origen | Estado |
|---|---|---|---|
| `player+0x200` `animDataPtr` | u16 en `plan_estudio_flycast.md` | `uni-ram.md` afirma puntero SH-4 de 4 bytes; mapa PS2 dice u16 | No resuelto; no definir struct compartido. |
| `player+0x2A4` | u16 en el plan AW | mapa PS2 lo lista como u8 | Anchura por plataforma/build pendiente. |
| Resolución de player | AW usa tabla team/entry y resta relativa `0x614` | mapa PS2 presenta estructura player directa | Modelos distintos; no usar un `playerTable` plano para AW sin prueba. |
| Regiones/bases | AW reporta offsets RAM observados | mapa PS2 offsets relativos al struct | No sumar una base host ni extrapolar entre emuladores. |

La fuente `uni-ram.md` incluye mapas UNI concretos y concluye carga/copia en RAM
para las capturas usadas. Su tabla antigua Sec10 contiene un modelo luego
corregido; usar `formats/uni-animation-model.md` para esa semántica, no repetir
la interpretación obsoleta. Otro desacuerdo documentado: `animDataPtr` podría
ser puntero SH-4 completo o valor de anchura distinta según contexto/build; no
dejar que el nombre de campo decida el tipo.

## Qué hace falta para desbloquear K02

1. Fijar imagen/revisión AW, región/idioma, BIOS/config, SHA de build Flycast y
   método de inicio; distinguir RAM fría/caliente y orden de equipos.
2. Obtener dos o más capturas naturales de RAM/estado completas o lecturas vía
   interfaz interna en puntos anotados. No modificar RAM para crear fixtures.
3. Comparar campo por campo las cadenas, tamaños, anchuras, endianess, ranges,
   alias y lifecycle; probar dos personajes y P1/P2/tag/cambio de actor.
4. Correlacionar actor con dirección guest calculada por el core; abrir un
   `MemoryRead`/telemetry API acotado que tenga validación y sea thread-safe.
5. Probar lectura en tick definido, actor nulo/invalidado, transición de tag,
   reset, estado ausente y reanudación. Cero writes y cero cambio en estado guest.
6. Solo entonces crear ficha versionada `KOF-MEM-PLAYER-AW` y habilitar overlay
   por identidad compatible; rechazar builds no reconocidas de forma segura.

Reutilizar `scripts/prepare_flycast_reference.py` o herramientas existentes solo
tras leer sus contratos y permisos. La captura vieja de proceso es evidencia
histórica, no una dependencia runtime multiplataforma para Windows/Linux/Android/Switch.

# Decisiones de migración

Registro append-only de elecciones de alcance. Las fichas no pueden borrar
interpretaciones históricas refutadas; añadir una decisión nueva con fuente.

| Fecha | ID | Decisión | Motivo/evidencia | Revisión necesaria |
|---|---|---|---|---|
| 2026-10-28 | MIG-001 | El inventario es de descubrimiento, no lista de material aprobado para copiar. | 1.573 candidatos bajo patrones explícitos; incluye informes históricos, scripts no relevantes y datos JSON. | K00/M00 verifica patrón completo y clasifica cada fila. |
| 2026-10-28 | MIG-002 | No fijar revisión Git de fuente ni afirmar aceptación independiente. | `kofxi_aw_modding` tiene cambios locales sin commit estable; hash por archivo identifica bytes observados, no una release común. | Regenerar tras congelar revisión compartida. |
| 2026-10-28 | MIG-003 | No trasladar el mapa PS2 como mapa Atomiswave sin separar y resolver contradicciones. | `player-memory-map.md` se titula PS2; `plan_estudio_flycast.md` reporta offsets AW, mientras algunos tamaños/campos difieren. | M02 contrasta imágenes/revisiones y captura AW natural. |
| 2026-10-28 | MIG-004 | Primer batch: resúmenes UNI, modelo de animación, LZSS y PVP/PVPL; conservar claims como candidatos/históricos. | Son útiles para parsers/inspector y tienen referencias claras, pero fixtures/corpus no se trasladaron. | Revisión independiente, pruebas de parser y scope AW. |
| 2026-10-28 | MIG-005 | Decomp/recomp, scripts de runtime y dumps no se copian en masa. | Se conserva el origen; elegir herramientas por utilidad, dependencia y licencia en M04/M05. | Clasificar inventario con inclusión/exclusión/blocker y motivo. |
| 2026-10-28 | MIG-006 | No fusionar player-memory-map PS2 con observaciones AW en un struct común. | El mapa de origen declara PS2 y hay diferencias de anchura (`+0x200`, `+0x2A4`) y resolución de actores. | Captura natural AW por build y comparación de campos/cadena antes de K02. |

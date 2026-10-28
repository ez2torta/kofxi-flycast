# Índice de traspaso de investigación KOF XI

Fuentes están en checkout separado `kofxi_aw_modding`; las rutas siguientes son
relativas a su raíz. No se importaron datos/artefactos porque su procedencia,
licencia, coste y formato deben revisarse individualmente. Este índice es mapa
de lectura, **no catálogo validado ni aceptación**.

| Tema | Fuente localizada | Posible uso | Qué validar antes de consumir |
|---|---|---|---|
| Modelo/flujo UNI y animaciones | `docs/architecture/formats/uni-animations.md`; `docs/plans/plan_estudio_flycast.md` | Asset inspector, viewer y etiquetas de acción/frame | Revisión AW versus PS2, versión del parser, fixtures y límites; no asumir estructuras idénticas. |
| Memoria y estructuras de juego | `docs/plans/plan_estudio_flycast.md` (incluye player/team/cámara/hitbox y offsets observados) | Resolver actores y dibujar overlay | Especificidad por build, punteros/alias, dirección guest frente a host, regiones/idiomas y ejecución natural. |
| Contraste AW/PS2 | `docs/architecture/runtime/player-memory-map.md` (PS2); `docs/architecture/runtime/uni-ram.md`; plan AW arriba | Evitar portear offsets de PS2 como Atomiswave | Hay desacuerdos explícitos de anchuras y modelo de resolución; ficha bloquea integración hasta captura AW. |
| Captura runtime/Dojo | `docs/research/runtime/flycast-dojo-reference-2026-10-22.md` | Diseñar observación y replay | Probes Lua no daban CPU/IRQ; contador Dojo no equivale a vblank durante todo el arranque. |
| Comparación original | `scripts/prepare_flycast_reference.py`, `capture_flycast_v3.py`, `flycast_ref_log.py`, `analyze_flycast_ref.py`, `compare_recomp_flycast_ref.py` | Reusar preparación/captura/análisis | Revisar contratos; no están copiados, no constituyen automáticamente replay determinista ni referencia CPU completa. |
| Recuperación de funciones | `docs/plans/plan-maestro-decomp-port.md`; `docs/research/runtime/` | Diagnóstico temporal, ABI, memoria, IRQ y dependencias | Evidencia para dominios concretos, no aceptación global ni autorización para alterar el core. |
| Rendimiento histórico | `docs/research/runtime/r05-windows-performance-baseline-2026-10-27.md` | Contexto para diseñar perfilado | Mide recomp, no Flycast; no transferir porcentajes al fork. |

## Fichas por crear tras validar fuentes

- `KOF-FMT-UNI`: estructura/relaciones, parser, límites y fixtures por revisión.
- `KOF-FMT-PVR`: formatos de textura, paletas, offsets/orientación y reglas de
  render de XI; separar datos del asset de decisiones del renderer.
- `KOF-FMT-COMP`: algoritmos de compresión y firmas de recursos con pruebas de
  entrada/salida, límites y procedencia. No inferir algoritmo por apariencia.
- `KOF-MEM-PLAYER`: cadena team → entry → data → actor; resolver dinámico, no
  dirección host fija.
- `KOF-MEM-HITBOX`: campos/cajas observados, escala y tick; alcance por
  personaje/estado y diferencias entre caja dibujada y colisión real.
- `KOF-PLAY-ACTIONS`: acción/anim/frame/timer y pilotos hit/block/whiff;
  explicitar personajes y casos no observados.

Son IDs pendientes, no afirmaciones verificadas en este fork.

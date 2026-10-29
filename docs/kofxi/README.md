# Conocimiento específico de KOF XI

Esta sección conserva investigación del juego y convierte hallazgos en features
QoL verificables. No es un segundo roadmap del emulador: las prioridades de
producto siguen en [`../roadmap.md`](../roadmap.md), y los entregables del
catálogo pasan al [backlog](../tasks/backlog.md).

## Principios

1. **Separar juego y emulador.** El core emula Atomiswave; parsers, overlays,
   entrenamiento y análisis de KOF XI viven en un perfil/módulo identificable.
2. **Procedencia por hallazgo.** Citar archivo/informe/commit de origen, hash o
   identidad de la build investigada, método y artefactos. La relación con el
   repo `kofxi_aw_modding` está en [`../evidence/research-handoff.md`](../evidence/research-handoff.md).
3. **Confianza explícita.** Usar `verified`, `corroborated`, `observed`,
   `hypothesis` o `unknown`, y explicar el alcance; un test dirigido no se
   transforma en aceptación natural.
4. **Estabilidad explícita.** Distinguir formato/regla estable, dirección fija
   solo para una build, puntero dinámico resuelto en runtime, estado transitorio
   y dato desconocido. Registrar región/revisión/idioma cuando afecte.
5. **Lectura primero.** Overlay de hitboxes, frame data, input display y replay
   no deben escribir RAM ni cambiar resultados de la partida vanilla.
6. **Entrenamiento aislado.** Reset, grabación, reproducción, dummy y cambios
   de estado declaran qué escriben; entrenamientos modificados llevan identidad
   de sesión distinta y no se mezclan con competitivo ni sus replays.
7. **Validación cruzada.** Probar en una ejecución natural y varios estados; no
   inferir roster/cobertura universal de una captura o personaje.

## Catálogo

| ID | Área | Estado inicial | Datos ya localizados |
|---|---|---|---|
| KOF-FMT | Formatos y compresión | Por catalogar | UNI y recursos gráficos; contratos detallados pendientes de importar con evidencia. |
| KOF-MEM | Memoria/runtime | Hallazgos parciales; plan de laboratorio | [Estado/replay/deltas](studies/state-replay-memory-plan.md): contrastar mapa AW/PS2, medir escrituras y validar snapshots antes de excluir estado. Avance documental: [borrador P0 de identidad/auditoría](studies/state-replay-p0-baseline.md), [propuesta temporal a T00/T08](studies/state-replay-p0-temporal-contract-proposal.md), [diseño P1](studies/state-replay-p1-lab-design.md) y [estudio de replay Fightcade/Dojo](studies/fightcade-flycast-replay-study.md); sin ejecución ni aceptación E1/E2. |
| KOF-PLAY | Gameplay/frame data | Parcial | Pilotos de acciones y transiciones; no generalizar al roster. |
| KOF-QOL | Features de usuario | Propuesta | Hitboxes, training, frame data, input display, repetición y análisis. |
| KOF-ROM | Cartucho Atomiswave | Pipeline local aclarado; atribución dinámica pendiente | [Imagen procesada vs. AWCartridge](studies/aw-rom-processing.md) distingue ROMs `.dec` de `fs_extracted/`; no es una feature aceptada ni autoriza redistribuir ROMs. |

Los estados describen el catálogo en **este repo**; no declaran que la
investigación original no exista. Ver el mapa en
[`research-index.md`](research-index.md) y el estado de traspaso en
[`migration/README.md`](migration/README.md). No se copian ROMs, BIOS ni RAM
dumps. El único save versionado, con autorización expresa, es el fixture
[`tests/fixtures/kofxi/unlocked-spanish.nvmem`](../../tests/fixtures/kofxi/unlocked-spanish.nvmem).

Para importar/restaurar el fixture en un build local, ver
[`local-test-save.md`](local-test-save.md).

## Esquema requerido por entrada

Protocolo KOF-ROM: [benchmark de cartucho/G1ROM/recursos](studies/resource-benchmark-plan.md),
con alternativas, gates numéricos y estimaciones de esfuerzo. Hay
[pilotos B/C/D](studies/experimental-rom-backends.md), sin A/B controlado aceptado.
El [plan de estado, replay y memoria](studies/state-replay-memory-plan.md) fija el
laboratorio para comprobar equivalencia y medir deltas; incluye la conclusión
provisional de `fs_extracted` y separa snapshots locales de paquetes de red.

Cada ficha se crea desde [`ENTRY-TEMPLATE.md`](ENTRY-TEMPLATE.md) e incluye:

- ID y pregunta que responde.
- Juego: revisión/hash cuando esté disponible, región, idioma, BIOS/DIP/config y
  build del emulador usada.
- Afirmación exacta, confianza, estabilidad y dominio cubierto/no cubierto.
- Dirección con espacio (SH-4/physical/RAM offset/host), base, tipo, alineación,
  cadena de punteros y método de resolución.
- Formato, endianess, stride, compresión y bounds, si corresponde.
- Receta reproducible, inputs/estado inicial, observaciones y controles.
- Fuente/informe del repo de investigación y artefactos con hash/ruta, sin
  incrustar contenido protegido o dumps masivos.
- Impacto: lector/herramienta propuesta; si escribe, bytes/rangos, rollback,
  determinismo y riesgo online.
- Qué evidencia concreta cambiaría el estado o invalidaría el hallazgo.

## Capas de integración recomendadas

```text
KOF XI data catalog + parsers de assets
             |
Game profile / runtime resolver (versionado, sin offsets host fijos)
             |
Read-only telemetry API -> overlay / hitboxes / frame data / input display
             |
Training controller opt-in -> writes auditadas, replay/profile no competitivo
             |
Flycast core (genérico, no conoce nombres/estructuras privadas de XI)
```

Un puntero host jamás forma parte de un archivo de datos. La resolución se hace
desde RAM guest y se valida en cada sesión. Si un dato cambia entre builds,
rechazar con diagnóstico explícito en vez de leer/escribir una dirección antigua.

## QoL: orden de prototipos

1. **Asset inspector offline:** decodificar UNI/recursos contra fixtures
   autorizados; probar round-trip o comparación de bytes cuando corresponda.
2. **Overlay de solo lectura:** actor/cámara, animación y hitboxes; comprobar
   escala, offset de sprite y actualización en el mismo tick observado.
3. **Frame-data lab:** inputs, cambios de acción, hit/block/whiff y marcas de
   tick; etiquetar hitstop y casos sin cobertura.
4. **Training no invasivo:** pausa/step si el core lo permite, input display y
   replay determinista antes de cualquier editor de RAM.
5. **Training con intervención:** reset a estado, dummy configurable o setters;
   perfil separado, save-state compatible declarado y auditoría de cada write.
   Comprobar rollback/desync antes de permitir juego online.

Ninguna feature está implementada o aceptada por crear este catálogo.

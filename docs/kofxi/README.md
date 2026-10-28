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
| KOF-MEM | Memoria/runtime | Hallazgos parciales | Algunas estructuras y offsets documentados; distinguir direcciones SH-4, offset RAM y dirección host. |
| KOF-PLAY | Gameplay/frame data | Parcial | Pilotos de acciones y transiciones; no generalizar al roster. |
| KOF-QOL | Features de usuario | Propuesta | Hitboxes, training, frame data, input display, repetición y análisis. |

Los estados describen el catálogo en **este repo**; no declaran que la
investigación original no exista. Ver el mapa en
[`research-index.md`](research-index.md). No se copian ROMs, BIOS, RAM dumps,
saves ni artefactos pesados.

## Esquema requerido por entrada

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

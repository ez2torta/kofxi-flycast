# Playbook de migración de conocimiento desde `kofxi_aw_modding`

Estado: plan; migración por ejecutar. Autoridad de prioridad: `docs/roadmap.md`.
Tarea coordinadora: K00 en `docs/tasks/backlog.md`.

## 1. Resultado que buscamos

Que una persona o LLM, trabajando solo en este checkout, pueda responder:

1. Qué se sabe del juego, recurso o subsistema.
2. Para qué versión/plataforma/región vale, y qué podría cambiar.
3. Qué observación o prueba respalda cada afirmación.
4. Cómo repetir la prueba sin acceso a contexto conversacional o a archivos
   privados no declarados.
5. Cómo usar el hallazgo sin afectar la emulación genérica ni el juego vanilla.

**No significa copiar todos los documentos o bytes.** La migración es completa
cuando cada fuente candidata del inventario queda clasificada como migrada,
resumida/enlazada, excluida con motivo o bloqueada con requisito para reanudar.
Los hechos útiles tienen ficha canónica con límites y evidencia. No se promueve
un claim por copiar texto.

## 2. Repositorios y límites

- Origen primario: checkout local `E:/github/kofxi_aw_modding` (al momento de
  este plan contiene cambios locales; no fingir un commit fuente estable).
- Destino: este repo `kofxi-flycast`; su roadmap es la autoridad del fork.
- Laboratorio hermano histórico citado por el estudio: `kof-combo-hitboxes`.
  No asumir que está disponible ni copiar sus artefactos automáticamente.
- Ninguna ruta `E:/...` es una dependencia portable. El manifiesto debe usar
  ruta relativa al root fuente y conservar SHA-256 de cada archivo leído.
- No migrar ROM/BIOS, ejecutables propietarios, memoria cruda, saves, dumps,
  capturas grandes, credenciales o información privada. Registrar solo identidad,
  hash legalmente compartible, resultados y la ubicación fuera del repo.
- Antes de copiar código/datos, revisar licencia, procedencia, dependencia y
  redistribución. Resumen factual y enlace de procedencia no equivalen a permiso
  para redistribuir un artefacto propietario.

## 3. Qué migrar, en qué nivel

| Nivel | Contenido | Acción |
|---|---|---|
| L0 — índice | Fuente, tema, hash, estado, destino, motivo | Registrar todos los Markdown/scripts candidatos; permite medir cobertura sin copiar. |
| L1 — canon | Conclusión vigente, formato/estructura, mapa y recetas que el producto necesita | Redactar una ficha breve, autocontenida, con fuente y alcance. |
| L2 — evidencia | Informes experimentales, pruebas, refutaciones, fixtures pequeños y artefactos permitidos | Conservar informes íntegros cuando aporten trazabilidad; no reemplazar evidencia con el canon. |
| L3 — herramienta | Parser, decoder, extractor, logger, comparator | Portar solo si es útil al fork, después de revisar licencia/deps; tests golden sin ROM/dumps protegidos. |
| X — no migrar | Historial redundante, prompts, análisis exclusivo del recomp, artefactos grandes/privados | Registrar ruta/categoría y motivo; enlazar si ayuda a entender límites. |

**No editar en el origen** para acomodarlo al destino. Si se corrige una
interpretación durante el traspaso, crear canon nuevo con nota de diferencia;
dejar el histórico intacto. Si un documento tiene una sección refutada, conservar
la corrección y citarla en la ficha, no copiar silenciosamente la parte vieja.

## 4. Taxonomía de destino

```text
docs/kofxi/
  formats/       UNI, PAL/PVP, PVR, LZSS y filesystem/cartucho
  runtime/       espacios de direcciones, resolvers, structs, ticks, inputs
  gameplay/      acciones, animación, hitboxes, daño y transiciones
  tools/         contratos/portabilidad de utilidades, no volcados
  qos/           diseño/contratos de overlay, lab y training
  evidence/      informes pequeños curados y manifiestos
  migration/     este playbook y el registro fuente→destino
```

Cada página canónica empieza con: **estado**, **afirmación**, **versión/dominio**,
**qué no demuestra**, **fuentes**, **cómo validar** y **siguiente uso en Flycast**.
Usar la plantilla `ENTRY-TEMPLATE.md` para un hallazgo; usar ficha por formato o
subsistema cuando haya una colección grande de campos.

## 5. Lotes y dependencias (paralelizables)

Primero se crea el registro L0. Cada lote solo consume filas asignadas. Las
personas no editan la misma ficha; conflictos se resuelven vía K00.

| Lote | Fuentes iniciales del repo origen | Destino/resultado | Dependencias | Puede avanzar junto a |
|---|---|---|---|---|
| M00 Inventario y procedencia | Índices `docs/architecture/`, `docs/research/`, `docs/plans/`, `scripts/`; contar también JSON/CSV relevantes, no artefactos | `migration/source-manifest.csv`, hashes y priorización; sin claims nuevos | Ninguna; lo coordina K00 | Ninguno antes de aprobar el esquema |
| M01 Formatos/assets | `docs/architecture/formats/{uni,uni-animations,pal,lz-pvp,lzss,g1rom-filesystem}.md`; `docs/research/animation/` y parsers `src/` | fichas UNI/PAL/PVP/LZSS/ROM y tabla de correcciones/refutaciones; candidatos de fixtures/tooling | M00 | M02, M03, M04 sobre rutas disjuntas |
| M02 Runtime/memoria | `docs/architecture/runtime/{player-memory-map,uni-ram,runtime-globals}.md`; `docs/plans/plan_estudio_flycast.md`; capturas runtime | mapa guest, cadenas de resolución, matriz build-specific y protocolo de lectura | M00; identificar AW vs PS2 antes de canon | M01, M03, M04 |
| M03 Animación/hitbox/gameplay | `docs/architecture/formats/uni-animations.md`; `docs/research/animation/`, pilotos Ash/Shen/otros en `docs/research/runtime/` | chain UNI→acción→animación, geometría/tick, matriz de coverage/personaje y claims | M00, insumo M01/M02 cuando haga falta | M01, M02, M04 |
| M04 Captura, replay y validadores | scripts de Flycast/ref, herramientas de `scripts/`, planes de runtime | catálogo ejecutable: propósito, CLI, inputs/outputs, deps/licencia, tests y decisión portar/no portar | M00; no importar corpus privado | M01–M03 si interfaces ya son conocidas |
| M05 Decomp/ABI/temporización pertinente | plan maestro, informes `docs/research/runtime/` y subplanes | solo hallazgos que afecten perfil XI, timing/dispositivos o diagnóstico del core; índice por área y límites | M00; filtrado manual por utilidad al producto | M01–M04, sin duplicar catálogo gameplay |
| M06 QoL integration | resultados aceptados M01–M04 | requisitos/pruebas para overlay, inspector y training; tasks K02–K04 actualizadas | M01–M04; G1/G2 para efecto runtime/rollback | documentación de KOF-QOL |
| M07 Auditoría de completitud | registro entero, enlaces, hashes, review cruzado | filas cerradas/blocked/excluded, métricas de cobertura y handoff reproducible | M00–M06 | No paralelizar la aceptación final |

M00 descubre el tamaño real. Este repositorio ya conoce **13 Markdown** bajo
`docs/architecture/`, **63** bajo `docs/research/animation/` y **303** bajo
`docs/research/runtime/` en el inventario del 28-oct-2026; eso no significa que
los 379 sean útiles para Flycast. Después de M00, actualizar counts, lista exacta
y priorización; el glob inicial estaba limitado a resultados y no sustituye el
manifiesto completo.

### Orden de aceptación

1. Aprobar esquema de manifiesto y hash/source identity (K00/M00).
2. Migrar M01–M04; revisar claims útiles para KOF-XI QoL.
3. Filtrar M05 por relevancia; no importar automáticamente decomp/recomp.
4. M06 convierte datos aceptados en interfaces/tests/features propuestas.
5. M07 audita trazabilidad, falsos vigentes, portabilidad y bloqueos.

## 6. Workflow exacto para una persona o LLM

En cada lote:

1. Leer `AGENTS.md`, roadmap, este playbook, tarea asignada y plantilla.
2. Leer las reglas del repo fuente (`AGENTS.md`, conventions) y confirmar que el
   worktree está limpio/identificado. No inferir commit de fuentes locales.
3. Reservar IDs/rutas del lote y comprobar tareas activas. No asignar agentes por
   inferencia; la delegación debe autorizarse explícitamente.
4. Para cada fuente, guardar en el manifiesto: ruta relativa, SHA-256, tamaño,
   categoría, título/fecha, estado, fuente relacionada y decisión.
5. Leer documento completo y enlaces primarios; buscar correcciones, refutaciones,
   evidencia más nueva y contradicciones. No usar solo título o resumen generado.
6. Extraer claims atómicos: uno por fila/entrada, con fuente/sección/fecha,
   observación exacta, tipo de prueba (estático, dirigido, runtime natural,
   original/hardware), dominio, confianza y límite.
7. Clasificar `SUPPORTED`, `SUPERSEDED`, `CONTRADICTED`, `HISTORICAL`,
   `NOT_RELEVANT`, `UNVERIFIABLE` o `BLOCKED`. Todo claim de formato/decomp debe
   indicar AW/PS2/otro; no combinar plataformas por nombre parecido.
8. Redactar destino conciso que responda una tarea concreta. Separar hecho,
   inferencia y propuesta; el modelo no completa huecos con conocimiento general.
9. Añadir pruebas/fixtures con identidad y licencia permitida; no subir archivos
   solo porque están en el repo origen. Revisar bounds y datos sensibles.
10. Verificar links relativos, exactitud de SHA, contradicciones, spellcheck,
    generación de parser/test cuando aplique. Otro revisor compara destino contra
    fuente y marca qué claims no quedaron fieles.
11. Actualizar manifiesto fila por fila y tarea/roadmap. Registrar qué no se
    migró y por qué. No marcar lote completo si hay filas sin resolución.

### Prompt de trabajo reutilizable

```text
Migra solo las filas [IDs] del manifiesto asignadas al lote [Mxx]. Lee AGENTS.md,
el roadmap, el playbook y fuentes completas, incluidas correcciones/enlaces.
No alteres el repositorio fuente. No inventes equivalencia entre AW y PS2 ni
rellenes vacíos. Por claim entrega fuente exacta, evidencia/tipo, build/dominio,
confianza, estabilidad, límites y decisión de migración. Actualiza únicamente
las rutas reservadas y el manifiesto. No copies ROM/BIOS/dumps/saves/privados.
Si fuentes se contradicen o el SHA/versión no se determina, registra BLOCKED o
CONTRADICTED y no lo presentes como vigente. Valida enlaces/tests y devuelve
resumen: filas resueltas, pendientes, decisiones, pruebas y límites.
```

## 7. Estados y esquema del manifiesto

Estados de **migración** (distintos de confianza de un claim):

- `TODO`: aún no auditado.
- `IN_REVIEW`: fuente/leaks/relaciones en análisis.
- `MIGRATED`: canon creado, todas las afirmaciones trazables y revisado.
- `LINKED`: la fuente debe seguir externa/histórica; resumen y link portables listos.
- `EXCLUDED`: no útil o no redistribuible, con motivo y ubicación origen.
- `BLOCKED`: falta fuente, permiso, build, hardware o fixture; declarar qué lo desbloquea.

Campos mínimos de `migration/source-manifest.csv`:

```text
source_id,source_path,sha256,size_bytes,category,title,source_revision,
platform_game_revision,evidence_type,claim_ids,priority,migration_status,
destination_paths,related_sources,license_review,exclusion_or_blocker,
reviewer,review_date
```

`claim_ids` debe resolver a filas de un segundo `migration/claim-ledger.csv`:
`claim_id,statement,source_id,section,observed_or_inferred,evidence_type,
domain,confidence,stability,contradictions,limit,destination_anchor,status`.
Guardar un CSV como UTF-8, paths siempre relativos y sin celdas con listas ambiguas
(separador `;` documentado o filas relacionales). Añadir `schema_version` y fecha
en README/manifiesto. Si el inventario crece, generar summary; no editarlo a mano.

**Confianza:** `UNKNOWN`, `HYPOTHESIS`, `OBSERVED`, `CORROBORATED`, `VERIFIED`.
**Evidencia:** `STATIC`, `DIRECTED_DIFFERENTIAL`, `NATURAL_RUNTIME`, `INDEPENDENT_EMULATOR`,
`HARDWARE`, `SOURCE_CODE`, `CORPUS`. No existe ranking automático que vuelva
`VERIFIED` una observación dirigida: registrar dominio y criterio del proyecto.

## 8. Migrar herramientas sin traer el laboratorio entero

Para cada script, incluir ficha: propósito, comando mínimo, args, outputs,
dependencias, versión de Python/SDK, datos de entrada permitidos, side effects,
salidas deterministas, tests y licencia. Separar lector de memoria del harness
de recomp y del sistema de captura. Reescribir paths absolutos en CLI/config;
no portar nombres tipo `kofxi_boot.exe` o rutas de usuario como defaults.

Orden sugerido: parser/decoder offline UNI/LZSS → validators de assets → lectura
de RAM a través de interfaz documentada → captura/replay → comparador. Nunca
portar un escritor RAM para uso online antes de definir identidad de perfil,
auditoría y exclusión de competitivo.

## 9. Criterios para cerrar la migración

- M00 contiene todos los archivos candidatos y paths/hash verificados, con un
  estado final por fila (no se exige copiar todas las filas).
- Cada claim utilizado por el código/UI tiene cita exacta, revisión/dominio,
  confianza, estabilidad y límites; los contradichos/superados no aparecen como
  tablas vigentes sin etiqueta.
- M01–M04 revisados por una persona distinta del migrador; M05 queda clasificado.
- Herramientas portadas pasan pruebas independientes de los artefactos origen,
  con casos negativos, truncados y malformed; no dependen de carpetas locales.
- No se incluyó contenido restringido/privado y las licencias revisadas.
- Otra persona/LLM puede seguir pasos desde este repo limpio y llegar al mismo
  canon/validación; toda fuente no portable está explícitamente BLOCKED.
- README del catálogo enlaza fichas; K00/M00–M07 y K01–K04 reflejan evidencia real.

Esto cierra **traspaso documental/técnico definido**, no prueba por sí solo el
overlay, frame data, fidelidad emulador, training ni cobertura total del juego.

# Estudio de estado, replay y memoria de KOF XI Atomiswave

Fecha: 2026-10-29. Estado: **plan de investigación, no implementación ni resultados**.
Autoridad de prioridad/estado: [roadmap rector](../../roadmap.md). Complementa el
[plan del producto](../../plans/plan-flycast-kofxi-online.md), los
[contratos S2/S3/S5](../../specifications/README.md) y el
[benchmark de recursos](resource-benchmark-plan.md); no crea un roadmap paralelo.

## 1. Preguntas y decisiones de partida

1. ¿Podemos grabar una sesión larga desde un estado inicial completo y reproducir
   los mismos inputs **consumidos por el guest**, detectando la primera divergencia?
2. ¿Qué memoria/dispositivos cambian realmente por tick, quién escribe y cuánto
   cuesta guardarlos/restaurarlos durante combate, cargas y transiciones?
3. ¿Qué datos de personajes/escenario son mutables, compartibles por época de carga,
   reconstruibles o aún desconocidos? ¿Qué regiones parecen estables pero afectan
   el futuro cuando cambia una acción/personaje/round?
4. ¿Cuánto ahorran snapshots incrementales sin eliminar estado? Solo después:
   ¿puede definirse un estado semántico reducido y demostrarse suficiente?
5. ¿Son equivalentes los proveedores ROM A/B/C/D durante replay y restauración?

No se presupone que los deltas sean grandes ni que basten posiciones/vida/acción.
No hay tamaño objetivo de paquete respaldado por mediciones todavía. El primer
entregable es un laboratorio que pueda refutar hipótesis, no un protocolo online.

**Red y rollback local son problemas distintos:** la partida determinista transmite
principalmente inputs/control; los snapshots/deltas son normalmente locales. Un
snapshot para incorporación de espectadores o recuperación tiene otro contrato.
Enviar posiciones/deltas de actores requeriría diseñar autoridad/reconciliación;
no es una sustitución automática de rollback.

## 2. Responsables, límites y coordinación

Responsable de este diseño: agente de esta sesión. Antes de implementar, tomar el
lote específico con responsable, rama, rutas y dependencias en el backlog:

| Frente | Trabajo propuesto | Rutas propuestas / condición |
|---|---|---|
| T01 | Identidad, recorder/player y corpus | `benchmarks/replay/`, `docs/evidence/`, lanzadores en `scripts/`; código nuevo por acordar |
| K02 | Resolver y catálogo AW de solo lectura | `docs/kofxi/runtime/`; módulo de telemetría identificado y revisado antes de añadir hooks |
| T05 | Heatmap, tamaño/coste de deltas y RAM/CPU | `benchmarks/resources/`, `docs/kofxi/studies/` |
| T00/T08 | Frontera temporal, escritores y save/restore | Coordinar antes de tocar scheduler, serialización, dynarec, memwatch o GGPO |
| T06 | Evaluación separada GGPO/GekkoNet | ADR + spike aislado posterior a contrato de estado; no sustituir el backend de red por este documento |

Solo este diseño documental queda autorizado aquí. La instrumentación inicial es
diagnóstica, offline y lectura no invasiva de RAM. Replay inyecta inputs y restaura
estados mediante interfaces explícitas; no parchea variables para fabricar casos.
No escribir ni reorganizar el repo hermano. Las fases dependen de evidencia, no de
una fecha prometida. Si falta hardware/contrato se registra bloqueo, no se elimina.

## 3. Base existente y procedencia

Fuentes de `kofxi_aw_modding`, HEAD observado
`425d694541693018f7991245894d290f0f267621`, archivos siguientes sin modificaciones
locales al consultarlos. Identidad del documento no equivale a identidad de ROM de
sus capturas históricas; completar build/región/idioma antes de reutilizar un claim.

| Fuente (ruta dentro del repo hermano) | Qué aporta / confianza y límite | SHA-256 del documento |
|---|---|---|
| `docs/architecture/runtime/player-memory-map.md` | PS2/PCSX2: campos de actores/equipos e hitboxes. Hipótesis para AW, no ABI aprobado. Región `+0x10D..+0x18C` reportada estable en 1.546 frames, no inmutabilidad universal. | `5279E428069254E107F3466B5B39262F1B3795AD61EC03BE60641733A1D56774` |
| `docs/plans/plan_estudio_flycast.md` | Cadena AW team/entry/player, cursores y framecaps históricos. Es plan con afirmaciones de observaciones, no reproducción nueva aquí. Distingue RAM dumps de savestates completos. | `9B33E5F043DB4A653918D89E18C8C471A554430E655452C008FD200D1AE17A20` |
| `docs/architecture/runtime/uni-ram.md` | Copias UNI en RAM y compartición entre jugadores en corpus Ash/Oswald/Shen; candidato a base compartida por carga. No demuestra ausencia de escrituras en todo el roster. | `3B4348F1E694511C54F155B0ACED342D084E42132026228F974E450234FE4974` |
| `docs/research/runtime/action-record-map-2026-10-22.md` | Descriptores de ataque transitorios y correcciones al mapa acción/daño. Evidencia del recomp, no automáticamente Flycast original. | `B54434E021ACD18321915BF1DEBB39E97AE311B0E178327145F47762066F2219` |

Leer además las correcciones enlazadas desde cada fuente antes de consumirla. Aquí
[player-memory-resolution.md](../runtime/player-memory-resolution.md) ya registra
contradicciones AW/PS2: anchura de `+0x200`, de `+0x2A4` y cadena de actores. Su
estado bloqueado para código no se levanta solo por tener un mapa documental.

Ninguna dirección host forma parte del modelo. Registrar por hallazgo: dirección
SH-4/alias/offset físico, raíz resuelta, tipo/anchura/endianess, propietario, lifecycle,
lectores/escritores, método, cobertura y confianza (`observed/hypothesis/verified`
con dominio explícito). Una dirección observada en una carga no es una constante.

## 4. Experimento y entregables por fase

### P0 — Identidad y frontera temporal

- Fijar ROM/BIOS por hashes locales, región/idioma/DIPs, NVRAM inicial, EXE/commit,
  flags/toolchain, backend A/B/C/D, renderer/audio/mando y host/energía/térmica.
- Auditar `TEST_AUTOMATION` existente (`core/input/gamepad_device.cpp`, replay en
  `core/hw/pvr/spg.cpp`) y el consumo de controles AW/monedas: no declarar que ese
  grabador ya cumple este contrato ni activarlo sin revisar sus limitaciones.
- Definir punto seguro de captura/restauración: relación entre tick, ciclo SH-4,
  VBlank y polls de entrada; un frame visual puede contener varios polls.
- Inventariar estado serializado y omitido por subsistema, RTC/NVRAM/dispositivos,
  hilos/colas pendientes y entradas externas. No modificar tiempos para alinearlos.

**Salida/gate:** manifest versionado y contrato `capture/restore/step/input-consumed`
con propiedad de buffers e hilos revisado por T00/T08. Una captura de solo 16 MiB
RAM no sustituye CPU/FPU/scheduler/IRQ/DMA/VRAM/ARAM/PVR/TA/audio/ARM7/DSP.

### P1 — Grabador/reproductor de sesiones largas con estado completo

- Capturar un savestate completo local en la frontera acordada y estado inicial de
  entradas mantenidas; congelar identidad de partida y recursos. Desactivar online,
  cheats, fast-forward y autoload no controlado. No restaurar un estado de otra
  build/plataforma suponiendo compatibilidad.
- Grabar ambos jugadores, direcciones/botones/Start, monedas/servicio y cualquier
  entrada que el guest consulte. Registrar lo **consumido** por poll y su tiempo
  guest, no solo eventos SDL/teclas ni tiempo host. Especificar política SOCD.
- Archivo incremental por chunks acotados: versión, orden/contador, longitud, checksum,
  flush periódico y marcador de final. Recuperar prefijo completo tras interrupción;
  no considerar un replay truncado como sesión terminada correctamente.
- Replay bloquea entrada física de juego; conservar un control host separado para
  cancelar. Política explícita de EOF, pausa/resume, reset y carga manual de estado:
  registrar discontinuidad o finalizar segmento, nunca seguir silenciosamente.
- Calcular anclas de referencia completas por subsistema; hashes del actor son
  diagnósticos complementarios, no criterio suficiente. No incluir punteros/padding
  host en hashes canónicos. Si el estado nativo no es canónico, comparar campos
  definidos y justificar cada exclusión sin enmascarar la primera divergencia.

**Salida:** paquete local `manifest + initial.state + inputs + anchors + metrics`,
player CLI y resumen de primer fallo/tick/poll/subsistema. Diseño de formato no es
un compromiso de API pública. Piloto de 5 min reproducido 3 veces desde proceso
nuevo con mismo backend; gate: cero divergencias y EOF/duración/inputs completos.
Si falla, corregir primero el baseline; no reducir snapshots para esconderlo.

### P2 — Resolver AW y mapa semántico read-only

- Resolver equipos, slots activos/reserva y actores con validación de rangos,
  alineación y punteros; no usar `playerTable` PS2 como verdad AW.
- Reconfirmar posiciones/velocidades, facing, vida/guard/stun, stocks, acción,
  animación/duración/hitstop, hitboxes y descriptores activos, cursores de scripts.
- Extender a proyectiles/efectos/agarres, colas/allocator y tareas, RNG/timers,
  buffers/comandos de input, cámara/límites de escenario y flujo de round/KO.
- Resolver los bloques UNI y referencias desde actores: no inferir layout por
  coincidencia parcial de bytes. Identificar datos compartidos y aliases.
- Detectar creación/destrucción de actores, tag y reload: un puntero válido ayer
  puede apuntar hoy a un objeto diferente. Publicar ausencia/invalidación explícita.

**Salida/gate:** catálogo versionado con procedencia y tests naturales P1/P2,
dos personajes inicialmente, espejo/tag/KO/reset/loadstate. Campos no resueltos
siguen desconocidos. Cobertura inicial no se promueve a todo el roster.

### P3 — Heatmap de escrituras y tamaño real del cambio

- Comparar imágenes en fronteras seguras para medir bytes diferentes y tamaños
  de delta. Complementar con tracking de escrituras: un byte escrito y restaurado
  entre anclas no aparece en un simple diff, pero puede afectar otra tarea.
- Inventariar y probar todos los escritores: intérprete, stores de dynarec/fastmem,
  DMA, store queues, aliases, restauración de estado y escrituras de dispositivos.
  RAM/VRAM/ARAM y estado no-memory se cuentan por separado.
- Tracing pesado dirigido para PC escritor/anchura/rango/ciclo y lectores relevantes;
  captura ligera distinta para rendimiento. Medir y publicar overhead de ambos.
- Usar la clasificación P2 para etiquetar heatmap, nunca para ignorar bytes desconocidos.
  Comparar granularidad 256 B, 1 KiB y 4 KiB offline antes de elegir dirty pages.
- Informar bytes escritos, bytes diferentes, páginas sucias/total, repetición y
  cobertura por fase; distinguir CPU cambiada, texturas y recursos estáticos.

**Salida/gate:** CSV/JSON agregados por tick/fase, mapa de escritores con cobertura,
tamaños p50/p95/p99/máximo y bytes/s. Seguimiento dirty debe cubrir todos los diffs
de referencia y controles de cada escritor; documentar lo aún no instrumentado.
No ofrecer un dirty tracker incompleto como base de rollback.

### P4 — Clasificación y dependencias, especialmente UNI y stage

Clasificar cada región con lifecycle, no con un único label permanente:

| Clase candidata | Tratamiento a ensayar |
|---|---|
| ROM/recurso host inmutable | Identidad compartida; no copiar por tick |
| Código/tablas UNI en RAM sin cambios en una época | Base compartida **con bytes retenidos**, detección de writes y cambio de época en reload |
| Actores/equipos/proyectiles/RNG/tareas/input | Estado mutable; conservar hasta demostrar una representación equivalente |
| Caché host reconstruible | Invalidar/reconstruir con costes y efectos observables medidos |
| Stage/efecto supuestamente cosmético | Hipótesis; trazar relaciones con RNG, allocator, tareas, cámara, colisión, audio y calendario |
| Desconocido | Conservar; priorizar por bytes/coste/actividad, no descartar |

Una región estable en N frames no prueba inmutabilidad: probar cambios de personaje,
tag, supers, cargas y rounds. Mantener versiones/base mientras existan snapshots
que las referencien; no reconstruir RAM guest desde una ROM ignorando relocations
o modificaciones del loader. Stage visual y cámara/límites no son equivalentes.

**Salida/gate:** grafo actor→scripts/tablas→tareas→dispositivos, lectores/escritores
observados y candidatos justificados. Para un efecto visual, demostrar independencia
o mantenerlo dentro del estado. No excluirlo simplemente del hash para obtener PASS.

### P5 — Snapshots incrementales que conserven el estado completo

- Control A: snapshot completo. Candidato B: base + versiones de páginas/campos
  dirty, sin exclusiones semánticas. Checkpoints completos periódicos para limitar
  cadenas y restauración; medir full copy, XOR/delta y compresión como alternativas.
- Conservar estado no-memory y temporalidad de dispositivos; definir si el delta
  reconstruye hacia delante o deshace hacia atrás. Probar restauraciones arbitrarias,
  descarte de futuro tras branching, eviction, refcounts y reinicio de época.
- Al restaurar, comparar bytes/estado completos y después re-ejecutar los mismos
  polls/inputs. Invalidar cachés derivadas correctamente, incluido código generado
  cuando cambie memoria de código. No copiar punteros host entre instancias.
- Medir profundidades locales 2/4/8/12 frames como puntos de ensayo, no presupuesto
  aprobado; snapshot cada tick frente a captura espaciada, incluyendo resimulación.

**Salida/gate:** 10.000 restauraciones acumuladas en el corpus, cero divergencias
inmediatas y futuras, límites de memoria y p99 de capture/restore/resim. Repetir
x86-64/ARM64 en equipos disponibles antes de interoperabilidad. Si no hay ahorro
material o el p99 perjudica el margen, conservar el control y registrar el negativo.

### P6 — Estado semántico reducido (solo si P1–P5 lo justifican)

Ensayar un candidato por vez, perfil diagnóstico separado: guardar campos tipados,
compartir regiones demostradas o reconstruir cachés. Empezar por una región pequeña,
comparar primero contra el estado completo y luego simular futuro con corpus amplio.
La simulación de un emulador sigue necesitando estado CPU/dispositivos pertinente;
un registro de posiciones no lo reemplaza. Si se opta por omitir animación del stage
y aceptar divergencia visual, es **otro contrato/perfil** a decidir, no vanilla
equivalente. No afirmar suficiencia universal por éxito de una partida.

**Salida/gate:** lista exacta de bytes/campos excluidos/reconstruidos, prueba de
lifecycle/dependencias, controles adversarios y primera divergencia. Cero diferencias
no explicadas en el contrato aceptado; si falla, volver al snapshot completo o delta
de P5. No recortar el corpus para salvar una hipótesis.

### P7 — Corpus largo, backends y evaluación de netcode

- Reproducir el mismo paquete en A/B/C/D con el mismo EXE/config compatible,
  separando validación host inicial, selección, carga, combate y fin de stage.
- Replays externos de Flycast Dojo/Fightcade pueden investigarse como corpus
  exploratorio; el [estudio del enlace Fightcade](fightcade-flycast-replay-study.md)
  documenta límites y no los acepta como P1/E2 sin identidad/anclas verificadas.
- Aislar formato de replay/adaptador de estado del proveedor de red. Evaluar
  GekkoNet en ADR/spike: integración save/load/step, replay/checksums, propiedad y
  lifecycle de buffers, predicción, rollback profundo, espectadores y transporte.
  Fijar commit/licencia/API y comprobar plataformas; no asumir capacidades por README.
- GGPO sigue como control inicial según S4; cambiarlo requiere una decisión explícita
  de contrato, no una modificación silenciosa de especificación. Fallo de la librería
  y fallo de estado del emulador se diagnostican por separado.

**Salida:** informe comparativo y recomendación seguir/descartar/bloqueado; no se
integra red online antes de integridad de estado. Las pruebas locales de replays
no acreditan cross-play ni fidelidad contra hardware AW.

## 5. Corpus, métricas y controles

Escalado propuesto (cantidades de diseño, **no resultados**):

1. Sanidad: secuencias de 30–60 s con neutral, caminar, saltar, hit/block/whiff,
   hitstop, proyectil, agarre, super y tag, ambos jugadores y un caso espejo.
2. Piloto: 5 min, 3 replays idénticos con todos los inputs efectivos.
3. Cobertura: equipos distintos y todos los slots; KO/doble KO si se alcanza
   naturalmente, timeout, victoria/continuación/game over, cambios de stage/equipo;
   ampliar a roster/idiomas AW y casos no observados según catálogo, sin afirmar
   cobertura de acciones ausentes. Reset/test/service en segmentos separados.
4. Largo: paquete de 60 min reproducido 3 veces; soak de 2 h para recursos/lifecycle,
   con tramos de combate/carga repetidos. No sustituirlo por 2 h de menú quieto.
5. Rendimiento: 10 pares A/B alternando orden, misma secuencia e identidad, con
   análisis por fase e IC95 % según plan rector. Matriz obligatoria Windows/Linux/
   Android/Switch y x86-64/ARM64; equipos ausentes quedan bloqueados, no aprobados.

Métricas: delta bruto/comprimido por tick, base retenida, total de snapshots/colas/
índices, private commit, working set/RSS y picos, memoria mapeada/caché OS y GPU
cuando medible; CPU por hilo/proceso, capture/restore/hash/resim ms, p50/p95/p99/máximo,
plazos incumplidos, pacing y latencia. No llamar FPS presentado al promedio VBlank.
Presupuestos absolutos y umbral de mejora material se fijan **antes** del A/B con
T05/G1; si faltan, el resultado es exploratorio, no aceptación G3.

Controles negativos: input omitido/alterado, replay truncado/corrupto, identidad
equivocada, discontinuidad de estado y checksum incorrecto deben fallar de forma
explícita. Dirty tracking se prueba con todos los escritores y aliases, sin omitir
DMA. Baseline de estado incluye RNG/RTC/colas/dispositivos; no igualar solo la imagen.
Para hipótesis de exclusión, no editar RAM para fabricar capturas naturales: usar
ramas de ejecución/restauración instrumentadas separadas y declarar cualquier
intervención diagnóstica. Ausencia de error logueado no demuestra ausencia de desync.

## 6. Artefactos, entregas y condición de cierre

Artefactos pesados/derivados del juego en carpeta local ignorada por Git: savestates,
RAM/VRAM/ARAM, replays con estado incrustado, copias runtime de NVRAM y trazas masivas.
No extender la excepción del fixture NVRAM aprobado. Versionar únicamente esquema,
herramientas, datos sintéticos y métricas/resúmenes sin contenido protegido; revisar
que rutas/logs no contengan credenciales ni secretos. Checksums de integridad no son
firmas de seguridad ni permiso para redistribuir recursos.

Entregas verificables:

- [ ] E1 manifest/contrato temporal y cobertura de estado P0 revisados.
- [ ] E2 recorder/player largo, recuperación de chunks y primera divergencia P1.
- [ ] E3 catálogo AW con fuentes/anchuras/lifecycle y resolver seguro P2.
- [ ] E4 heatmap/deltas/costes y cobertura de escritores P3.
- [ ] E5 clasificación y dependencias, candidatos de exclusión justificados P4.
- [ ] E6 restauraciones y medición de snapshot completo vs incremental P5.
- [ ] E7 decisión explícita sobre estado semántico P6 (incluido resultado negativo).
- [ ] E8 replay A/B/C/D, corpus largo/matriz y decisión de netcode separada P7.

El estudio puede concluir con una exclusión **rechazada** o un bloqueo concreto.
No requiere demostrar que un snapshot de actores sea suficiente. Informar qué se
midió, qué sigue desconocido, bytes/costes reales y recomendación; actualizar tarea
y roadmap al cerrar/bloquear. La implementación futura requiere los gates anteriores.

## 7. Conclusión provisional de `fs_extracted` y relación con este estudio

Se conserva D como **candidato experimental de memoria**, no como default de
producción. La [corrida humana documentada](experimental-rom-backends.md#corrida-humana-d-del-2026-10-28-2308)
observó 331,60 MiB private y 328,51 MiB residente finales, frente a 546,13/520,18 MiB
de C anterior. Son ~215/~192 MiB menos en esas ejecuciones; no un A/B controlado.
El pico de commit fue 445,16 MiB en D frente a 551,27 MiB en C: la validación inicial
todavía pesa. La CPU sostenida quedó cercana a C, sin mejora material demostrada.

D valida 1.057 entradas/230.533.911 bytes y 10.584 checks de bordes/PIO; mapea 1.047
archivos no vacíos y retiene ~12,15 MiB de fallback cifrado. Esto respalda continuar
las pruebas, **no** demuestra igualdad de estado/rollback ni hardware. File-backed
no significa memoria gratuita: contabilizar working set y memoria/caché OS.

No elimina la descompresión guest ni vuelve inmutables las copias UNI en RAM guest.
La reducción del cartucho host y la del snapshot local son ahorros diferentes.
Usaremos D como candidato en el replay, conservando A/B/C como controles; todavía
faltan mismo build/inputs, anclas de estado, sesiones largas, p99 y portabilidad
(mapping D actual solo Windows). No añadir otra optimización antes de poder medir
estas diferencias con el laboratorio.

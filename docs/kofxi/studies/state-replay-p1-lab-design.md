# P1 — Diseño del laboratorio de replay con estado completo (borrador)

Fecha: 2026-10-29. Estado: **diseño, no implementación ni resultados**. No acredita
E2. Autoridad: [roadmap](../../roadmap.md); fases y puertas en
[plan de estado, replay y memoria](state-replay-memory-plan.md) §P1. Identidad y
auditoría previas en [P0](state-replay-p0-baseline.md), que este documento asume.

Alcance: forma del paquete, definición de "input consumido", estrategia de anclas,
boceto de CLI, corpus y controles. No autoriza tocar scheduler, serialización, GGPO ni
memwatch. El dueño autorizó separadamente un hook temporal post-load en `shop_readm`,
solo Debug/laboratorio y coordinado con T08, para obtener evidencia T00; esto no habilita
recorder/player integrado ni cambios de contrato compartido. No es un compromiso de API
pública.

## 1. Dependencias

| Depende de | Qué aporta | Estado |
|---|---|---|
| P0 (§1-§6 del baseline) | identidad, frontera temporal, build del laboratorio | borrador |
| T00 | contrato `capture/restore/step/input-consumed`, propiedad de buffers/hilos | NEEDS_EVIDENCE; dueño fija lectura SH-4/dynarec, coin fuente-por-slot→A0 verificado, ciclo `sh4_sched_now64()`+ordinal y step VBlank+drain; falta traza/validar predicado de drain y resto formal |
| T08 | revisión de rollback/memwatch y de la distancia a un estado canónico | Dueño asume revisión; decide checkpoint con `mapleInputState[4]` completo+cursor; validar serializer/restore, atomicidad e hilo sigue pendiente |
| T01 | replay, estado inicial, corpus y anclas | subtarea recorder/player asignada a esta sesión; implementación core bloqueada por T00/T08 |

Sin P0 cerrado, el recorder no puede declarar identidad ni comparar sesiones.

## 2. Semántica de captura y "input consumido"

1. **Reloj candidato** = `sh4_sched_now64()` en la frontera de observación. Vblank
   es contexto, no se declara tick ni equivale a cada input consultado.
2. **Evento consumido (decisión parcial T00)** = lectura SH-4 real del valor, incluida
   la ruta dynarec activa; respuesta Maple entregada es solo telemetría. Para coin, se
   reproducen fuentes por slot, se ejecuta la transformación AW original y se verifica
   cada retorno A0 `0x280`.
3. **Estado inicial** = savestate completo en la frontera acordada, incluyendo los
   hilos, colas y buffers que T00/T08 declaren como parte del contrato.
4. Nada de punteros host en el paquete. Nada de alinear tiempos para "cuadrar".

Replay debe reproducir los **mismos valores observables por el guest**, no las mismas
teclas pulsadas; si el guest no realizó el acceso, el evento no debería existir en el
flujo efectivo. Los puntos precisos aún no están aprobados; ver la
   [propuesta temporal T00/T08](state-replay-p0-temporal-contract-proposal.md). El
   hook temporal está implementado para Debug Windows x64; la cobertura runtime
   requerida por T00 sigue sin estar demostrada.

## 3. Forma del paquete (boceto)

Un paquete local por sesión:

| Componente | Contenido mínimo | Notas |
|---|---|---|
| `manifest.json` | versión de esquema, identidad (ROM/BIOS/NVRAM/build/backend), frontera de captura, política SOCD, límites declarados | reproducible por otro equipo |
| `initial.state` | savestate completo en la frontera | binario local, no versionado |
| `inputs.scr` | chunks acotados: versión, contador de orden, longitud, checksum, payload de eventos consumidos | prefijo válido tras interrupción |
| `anchors.jsonl` | anclas por subsistema en bordes elegidos | diagnóstico, no criterio único |
| `metrics.csv` | CPU/memoria/timing por fase y por tick | sin trazado pesado |

Reglas de integridad:

- marcador de **final correcto** explícito: un `inputs.scr` sin marcador es un
  prefijo válido, no una sesión terminada;
- checksum por chunk y global; versión de esquema obligatoria;
- flush periódico; recuperar el prefijo completo tras corte;
- ninguna ruta/log con credenciales ni secretos.

## 4. Anclas y primera divergencia

- Anclas **por subsistema** (CPU/FPU, RAM, VRAM, ARAM, scheduler, IRQ/DMA,
  dispositivos, PVR/TA), calculadas en la frontera de captura y al final de cada
  bloque de comparación.
- Excluir del hash canónico punteros/padding host; justificar cada exclusión y no
  ocultar la primera divergencia con ella. Si el estado nativo no es canónico,
  comparar campos definidos, no bytes crudos.
- Hashes de actor (K02) son **complementarios**; un hash de actor no sustituye el
  hash de subsistema.
- Al primer fallo: registrar tick, índice de poll, subsistema y bytes/campos
  distintos. El laboratorio existe para **refutar**, no para producir verde.

## 5. Player (boceto de CLI, no API)

```text
kofxi-replay run   --package <dir> [--verify-anchors] [--stop-on-divergence]
kofxi-replay check --package <dir>          # valida chunks/checksum/manifest
kofxi-replay info  --package <dir>          # identidad y cobertura
```

Política explícita:

- el replay **bloquea la entrada física** de juego; se conserva un control host
  separado para cancelar;
- EOF, pausa/resume, reset y carga manual de estado: registrar discontinuidad o
  cerrar segmento, nunca continuar en silencio;
- no restaurar un estado de otra build/plataforma suponiendo compatibilidad.

## 6. Corpus y controles negativos

Sanidad (30–60 s) con neutral, caminar, salto, hit/block/whiff, hitstop, proyectil,
agarre, super y tag, ambos jugadores y un espejo. Piloto de 5 min reproducido **3
veces desde proceso nuevo** con el mismo backend; gate: cero divergencias y
EOF/duración/inputs completos. Si falla, se corrige el baseline; **no** se reducen
snapshots para esconderlo.

Controles negativos que deben fallar de forma explícita: input omitido o alterado,
replay truncado o corrupto, identidad equivocada, discontinuidad de estado, checksum
incorrecto.

## 7. Métricas

- capture/restore/hash/resim en ms: p50/p95/p99/máximo;
- CPU por hilo/proceso; memoria: private commit, working set/RSS y picos;
- overhead del harness medido por separado y restado del A/B;
- por fase: validación host inicial, selección, carga, combate, fin de stage
  (no diluir el coste de carga en la media de combate).

Presupuestos absolutos y umbral de mejora material los fija T05/G1 **antes** del A/B;
hoy no existen, así que cualquier delta sería exploratorio, no aceptación G3.

## 8. Bloqueos y siguiente paso

- Bloqueado para implementación hasta que P0 cierre el contrato temporal con T00/T08
  y se decida la frontera de captura; P2 (K02) sigue bloqueado para anclas semánticas.
- Siguiente paso propuesto: revisar este boceto con T00/T01/T08, fijar la frontera de
  captura y el build de §6 del baseline, y solo entonces escribir el primer recorder
  mínimo (sin scheduler/serialización salvo lo que el contrato autorice).

## Límites

Diseño documental; no hay paquete real, ni anclas, ni CLI, ni medición de overhead.
La forma del paquete puede cambiar al revisarse con T00/T08.

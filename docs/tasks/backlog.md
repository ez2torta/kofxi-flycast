# Backlog inicial — tareas listas para repartir

Todas PENDING, responsables sin asignar. Una persona toma una fila en su PR de
inicio indicando nombre, rama y fecha; después mantiene estado y enlaces a evidencia.
Cada fila puede convertirse en issue de GitHub conservando su ID. No hay issues
remotos creados todavía. El roadmap decide prioridad.

| ID / frente | Responsable / estado | Dependencias | Rutas reservables | Entregable y aceptación |
|---|---|---|---|---|
| T00 Integración/contratos | Sin asignar / PENDING | Ninguna | docs/specifications/, docs/decisions/ | Aprobar tick/input/estado/versionado v0 con revisores de núcleo y red; mapa de responsables y conflictos. |
| T01 Base/laboratorio | Sin asignar / PENDING | Ninguna para inventario; T00 para adaptador | benchmarks/, docs/evidence/ | SHA/toolchain, build stock y recorrido a resultado; auditar scripts históricos y diseñar replay con anclas verificadas. |
| T02 Windows/Linux | Sin asignar / PENDING | Ninguna | docs/platforms/desktop.md; workflows propios a acordar | Recetas Release, equipos mínimos, smoke tests y artefactos; x86-64/AArch64 identificados. |
| T03 Android | Sin asignar / PENDING | Ninguna | docs/platforms/android.md; shell/android/ tras coordinación | APK base en dispositivo, controles/red/renderer, medición térmica exploratoria; registrar API/SoC/toolchain. |
| T04 Switch | Sin asignar / PENDING | Equipo/toolchain | docs/platforms/switch.md; shell/switch/ tras verificar estructura | Entorno y build documentados, ejecución real portátil/dock; BLOCKED con motivo si no hay medios. |
| T05 RAM/CPU/GPU | Sin asignar / PENDING | Ninguna para diseño; T01 para baseline | benchmarks/resources/, docs/evidence/ | Contabilidad completa y formato portable; perfil stock y presupuestos por dispositivo, sin optimizar aún. Hooks core mediante T00. |
| T06 Matchmaking | Sin asignar / PENDING | T00 para congelar API | services/matchmaking/ | Auditar soluciones existentes; ADR de stack, OpenAPI/eventos/mock y pruebas de sala/cola/cancelación; contrato de cliente nativo común. |
| T07 Entrada/audio/vídeo | Sin asignar / PENDING | Ninguna para receta; T01 para medición | benchmarks/latency/, docs/evidence/ | Método entrada-a-fotón, colas/polling y equipo; baseline USB/BT/integrado sin confundir tiempos software con sensor. |
| T08 Determinismo/rollback | Sin asignar / PENDING | T00,T01,T02 y un destino ARM64 real | core/network/, serialización/memwatch identificadas en T00 | 10 000 restauraciones y replays x86-64/ARM64; primera divergencia por subsistema; puerta G2. |
| T09 Optimización medida | Sin asignar / PENDING | T05,T07,T08 | Una ruta de hotspot por PR, declarada antes | Hipótesis y métrica preregistradas, A/B y corpus G3; conservar resultados negativos. |
| T10 Red/servicio | Sin asignar / PENDING | T06,T08 y builds de destino | services/, benchmarks/network/ | Staging, relay, netem y matriz cross-play; carga/fallos según G4/§4.2. |
| T11 Empaquetado/aceptación | Sin asignar / PENDING | T09,T10,T02–T04 | packaging a definir, docs/evidence/ | Paquetes cuatro destinos, sets humanos G5, reproducción externa y G6. |
| K01 Catálogo UNI/PVR/compresión | Sin asignar / IN_PROGRESS (primera transcripción) | Ninguna para inventario; T01 para fixtures | docs/kofxi/formats/ | Cuatro fichas iniciales y 8 claims trazados; falta revisión independiente, resolución de scope AW/PS2, fixtures, bounds y decisión de licencia. |
| K02 Resolver runtime actores/hitboxes | Sin asignar / IN_PROGRESS (integración BLOCKED) | K01 para escalas/recursos; T01 para ticks; captura AW fijada | docs/kofxi/runtime/, telemetry API a coordinar | Primera ficha contrasta AW/PS2; no implementar consumo hasta capturas AW que resuelvan anchuras/cadena. Después: resolver por build, sesiones naturales, read-only, sin host pointers. |
| K03 QoL: overlay/input/frame data | Sin asignar / PENDING | K02,T07,T08 | Perfil XI/UI, rutas a verificar | Overlay read-only sincronizado al tick; falla de forma segura en builds no reconocidas; evidencia con cobertura declarada. |
| K04 Training opt-in | Sin asignar / PENDING | T08,K03 | Perfil XI/training, rutas a verificar | Record/playback/reset; writes auditadas, identidad no vanilla, offline por defecto y riesgo online evaluado. |
| K00 Migración de conocimiento | Sin asignar / IN_PROGRESS | Ninguna | docs/kofxi/migration/, playbook | M00 inventario generado; M01 primera transcripción iniciada. Completar M00–M07: fichas revisadas, claims trazables, herramientas seleccionadas y auditoría según §9 de MIGRATION-PLAYBOOK. |

## Reglas para evitar bloqueos entre equipos

- T01–T07 pueden investigar/medir sus dominios en paralelo. Los contratos nuevos
  se integran vía T00 antes de depender de estructuras/funciones inventadas.
- Rutas de código listadas son áreas propuestas: verificar ubicación real antes
  de reservar. CMake raíz, scheduler, serialización y GGPO tienen un integrador.
- Una optimización por PR; no mezclar refactor de transporte con memoria.
- Servicio puede usar clientes simulados, pero su PASS no acepta cross-play real.
- Cada tarea cerrada enlaza receta/resultados/límites; si está bloqueada anota
  dependencia, responsable de resolverla y evidencia que permitirá reanudarla.

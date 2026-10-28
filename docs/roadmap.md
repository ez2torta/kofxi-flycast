# Roadmap rector — KOF XI Flycast

Fecha inicial: 2026-10-28. Base upstream:
`1b63c8a3677cc8eb1292863c4fe8fc8f94674fc8` (master al clonar).

## Autoridad y alcance

Este documento gobierna prioridades y estado del fork. El
[plan completo](plans/plan-flycast-kofxi-online.md) contiene puertas/criterios;
el [backlog](tasks/backlog.md) los descompone, sin sustituirlos.
La investigación/decompilación continúa teniendo autoridad en su repositorio
`kofxi_aw_modding`; aquí se aceptan solo resultados con procedencia y límites.

Obligatorios: Windows, Linux, Android, Nintendo Switch, determinismo x86-64/ARM64,
cross-play, matchmaking propio desplegable, perfil ligero, ritmo guest correcto,
baja latencia y evidencia reproducible. macOS/otros: matriz de extensión.
Un equipo no disponible bloquea su aceptación, no elimina la plataforma.

## Estado real

| Hito | Estado | Evidencia / siguiente entrega |
|---|---|---|
| S0 — Preparación de repositorio | Preparado localmente | Historial upstream, rama `kofxi-main`, documentación; ver procedencia. Sin commit propio/publicación todavía. |
| G0 / FCT-00 — Bases y viabilidad | Pendiente | Builds originales, instalaciones aisladas, recorrido natural, equipos/toolchains. |
| G1 — Laboratorio | Pendiente | Inputs/anclas, medición portable, presupuestos y baseline. K01 puede avanzar en paralelo. |
| G2 — Integridad rollback | Pendiente | 10 000 restauraciones, estado completo y comparación x86-64/ARM64. |
| G3 — Optimización | Pendiente | A/B y recursos por dispositivo; conservar comportamiento. |
| G4 — Red y cross-play | Pendiente | Matriz plataformas/red/relay con hashes confirmados. |
| G5 — Servicio y jugadores | Pendiente | Matchmaking desplegable, capacidad medida y sets humanos. |
| G6 — Entrega | Pendiente | Paquetes y aceptación de las cuatro plataformas obligatorias. |
| H — Fidelidad hardware | Pendiente | Defecto y referencia AW independiente; evidencia pertinente. |
| K00 — Migración de conocimiento | En progreso | M00 generó 1.573 filas; 1.566 siguen TODO y 7 fuentes están en revisión; 11 claims candidatos/históricos, falta revisión y M02–M07. |
| K01 — Formatos | En progreso | Resúmenes UNI/animaciones/LZSS/PVP-PVPL; fuente hash registrada, scope/fixtures/licencia por aceptar. |
| K02 — Runtime/memoria | En progreso, bloqueado para integración | Se transcriben observaciones AW y diferencias con mapa PS2; faltan capturas AW fijadas para resolver anchuras/cadena. |
| K03–K04 — QoL XI | Pendiente | K00/M02–M04; resolver mapa AW/PS2 antes de overlay y training. |

## Primer sprint: FCT-00

1. Coordinar contratos y responsables: T00.
2. Ejecutar en paralelo T01–T05 y T07 con sus rutas asignadas.
3. T06 diseña protocolo/mock en paralelo; integración depende del contrato T00.
   T05 tiene una [propuesta de benchmark ROM/G1ROM/recursos](kofxi/studies/resource-benchmark-plan.md)
   coordinable con T01/T00/K01/K02/T08: cifras de diseño y esfuerzo estimado, sin
   ahorro ni backend aceptados todavía. Hay [prototipos B/C offline](kofxi/studies/experimental-rom-backends.md)
   seleccionables para medición local; arranque verificado, combate A/B y equivalencia
   de estado aún pendientes, sin aceptación de optimización.
4. Cerrar inventario/baseline exploratorio; luego implementar G1 y fijar
   presupuestos antes de optimizar. T08–T11 dependen de sus entregables previos.
5. Abrir el catálogo KOF XI (`docs/kofxi/`) como frente de conocimiento con
   procedencia y confianza. Sus fichas avanzan junto al laboratorio;
   overlays/training esperan los contratos runtime necesarios.
6. K00 comienza por M00 (inventario); M01–M04 pueden paralelizarse solo con
   IDs/rutas reservadas y revisión de procedencia. El playbook fija la aceptación.

No hay optimizaciones ni superioridad acreditadas. No asignar fechas de cierre
sin conocer equipos y build. Bloqueos y cambios de alcance se registran aquí
con evidencia y decisión; nunca se rebajan puertas tras medir para producir PASS.

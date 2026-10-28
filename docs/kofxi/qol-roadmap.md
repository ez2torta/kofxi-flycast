# QoL de KOF XI — propuesta técnica

La prioridad se deriva del [roadmap del proyecto](../roadmap.md); esto cataloga
features, no autoriza cambios todavía.

| Feature | Modo | Dependencias | Criterio inicial |
|---|---|---|---|
| Inspector UNI/PVR/compresión | Offline | Fichas KOF-FMT, parser y corpus pequeño autorizado | Interpretar assets seleccionados, bounds seguros y comparación contra referencia. |
| Overlay de actores/cámara | Observación read-only | Resolver KOF-MEM versionado, telemetry API | Sin writes; lecturas correctas en sesiones frescas de builds declaradas; deshabilitar si resolver falla. |
| Hitboxes/hurtboxes | Observación read-only | KOF-MEM-HITBOX, escala/offset, sincronía de tick | Overlay alineado y validado en escenarios conocidos; etiquetar tipo de caja y cobertura. |
| Frame data | Instrumentación read-only | KOF-PLAY-ACTIONS, eventos/ticks, replay | Repetición da mismo informe; separar startup, hitstop y ramas sin cobertura. |
| Input display/replay | Read-only sobre inputs consumidos | Tick/input común y G1 | Presentar enviado y consumido sin confundirlos ni alterar scheduler. |
| Training básico | Local opt-in | Replay, pausa/step, estado/configuración | Reset/record/playback reproducibles y separados del vanilla. |
| Dummy/editor de estado | Escritura opt-in | Contratos memoria, auditoría, save/restore, identidad de perfil | Toda write listada; offline por defecto, no ranking vanilla, riesgo online probado. |
| Replay/observación online | Inputs/estado confirmado | Protocolo y consentimiento | Espectador fuera de ruta crítica; no mutar sesión. |

No asumir que los nombres hurtbox/hitbox corresponden a campos internos: validar
taxonomía contra código y situaciones conocidas. Estado inactivo, proyectiles y
efectos pueden requerir resolvers distintos.

Primer lote de datos: completar UNI y resolver de actores con fuentes, construir
un parser/inspector aislado y decidir si el overlay consume el resolver sin tocar
el core. Después crear tarea separada para overlay. Hallazgos dirigidos conservan
identidad, alcance y fixture.

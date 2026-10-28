# Proyecto KOF XI Flycast

Leer `docs/roadmap.md` antes de elegir trabajo. El contrato completo está en
`docs/plans/plan-flycast-kofxi-online.md`; tareas en `docs/tasks/backlog.md`.
El roadmap es la autoridad de estado/prioridad. No mantener roadmaps rivales.

- Conservar estructura, historial, licencias y compatibilidad upstream.
- Windows/Linux/Android/Switch, cross-play y matchmaking son requisitos.
- Leer las especificaciones del área antes de editar y registrar decisiones.
- Tomar una tarea con responsable, rutas y dependencias explícitas; consultar
  tareas activas antes de tocar código compartido. El backlog no autoriza por sí
  solo agentes delegados, commits, pushes o despliegues automáticos.
- Cambios en scheduler, serialización, GGPO o dynarec requieren coordinación
  entre los responsables; evitar PR simultáneas sobre el mismo contrato.
- No atribuir fidelidad hardware a igualdad con Flycast/Dojo. No convertir
  resultados dirigidos en aceptación global ni cifras objetivo en resultados.
- Cada cambio funcional requiere evidencia pertinente: receta, build, hardware,
  controles, resultado y límites. Actualizar tarea/roadmap al cerrar o bloquear.
- No versionar ROM/BIOS, dumps, saves, credenciales ni artefactos masivos.
- Mantener conocimiento específico del juego en `docs/kofxi/`; cada dato lleva
  build/región, fuente, método, confianza, estabilidad y límites. No promover
  hipótesis a contrato ni depender de direcciones absolutas sin resolverlas.
- Features QoL específicas (training/hitboxes/frame data) se aíslan del núcleo
  genérico; competitivo vanilla es lectura no invasiva y cualquier modificación
  de RAM tiene perfil/identidad separada.
- Antes de tocar un subsistema consultar también sus instrucciones locales.

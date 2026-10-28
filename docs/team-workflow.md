# Trabajo en equipo

Rama de integración inicial: `kofxi-main`. Ramas de tareas: `task/Txx-descripcion`.
Usar worktrees/checkouts separados; no compartir un directorio de build entre
ramas. No rebases del historial publicado sin acuerdo. Mantener upstream fijado
y actualizarlo en PR específica con matriz de regresión.

1. Tomar tarea en backlog y declarar responsable, rutas, dependencias y revisor.
2. Si cambia contrato compartido, proponer ADR antes del cambio dependiente.
3. Entregar PR pequeña con problema, hipótesis, receta, resultados y límites.
4. Revisión de integrador para scheduler/serialización/GGPO/dynarec y de plataforma
   afectada para backend. No aprobar por compilar en otra plataforma.
5. Actualizar estado en roadmap y tarea solo con evidencia enlazada.

CI upstream heredada debe auditarse antes de habilitar publicaciones del fork;
badges upstream no representan nuestros resultados. T02–T04 proponen builds del
fork con submódulos fijados y sin asumir secretos ni infraestructura upstream.
Las pruebas largas/hardware tienen informe identificable aunque no corran en CI.

Formato de PR: ID; rutas; dependencias; cambios; comandos; resultados; regresiones;
artefactos/hash; límites; actualización documental. Los objetivos porcentuales
no se presentan como mediciones. No incluir contenido del juego ni datos privados.

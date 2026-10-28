# ADR 0001 — Fork con historial y autoridad propia

Estado: aceptado para preparación local, 2026-10-28.

Se conserva Flycast completo, su historial y estructura. Nombre provisional
`kofxi-flycast`, rama `kofxi-main`, `upstream` apuntando al original. El futuro
`origin` corresponderá al repositorio GitHub elegido por el usuario.

`docs/roadmap.md` gobierna el fork. La investigación de KOF XI conserva su propio
repositorio; sus resultados se consumen con procedencia. No duplicar estados.

Matchmaking reside inicialmente en `services/matchmaking/` para compartir
versionado/contratos; stack pendiente de T06. No implementar un servidor simulado
y presentarlo como backend de producción. Los adaptadores de plataforma rodean
el núcleo común. Primero pruebas y contratos, después optimizaciones.

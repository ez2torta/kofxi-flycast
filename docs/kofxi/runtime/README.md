# Runtime KOF XI — memoria, actores y tiempo

Las direcciones de esta área pertenecen al **espacio de direcciones guest** o a
un offset de RAM, nunca son punteros host portables. Resolverlos a través de un
API del emulador; no escanear el proceso externamente como arquitectura final.

## Estado inicial

- [Espacios de direcciones y actor player](player-memory-resolution.md): primer
  cruce de documentos y contradicciones. **BLOCKED para integración** hasta
  disponer de fixture/captura AW actual y resolver anchuras/cadenas.
- El mapa `player-memory-map.md` de origen declara en su encabezado que es PS2.
  No copiarlo como layout Atomiswave.
- El tiempo de juego/frame/tick requiere ficha propia; `animFrameIndex` no se
  equipara a tick de simulación por nombre.

Claims relacionados están en `../migration/claim-ledger.csv`; cada revisión debe
identificar build, región, personaje, modo y punto de captura.

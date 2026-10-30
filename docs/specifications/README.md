# Especificaciones iniciales v0

Estado: contratos de diseño, no APIs implementadas. Cambios incompatibles requieren
decisión en `docs/decisions/` y revisión de las tareas dependientes.

## S1 — Plataforma y recursos

Windows x86-64; Linux x86-64/AArch64; Android AArch64; Switch AArch64 obligatorio.
T02–T04 registran versión mínima, backend, toolchain, modelo, RAM, pantalla,
transporte de mando y red. Switch debe identificar entorno/toolchain real.
Backends específicos rodean el núcleo común, sin alterar timing guest.

Antes de G3 publicar por dispositivo números de RAM estable/pico, snapshots,
cachés, GPU/CPU ms, frames en vuelo, audio-buffer ms y entrada-a-fotón. Los números
están pendientes de G1: no presentar una casilla vacía como presupuesto aprobado.
Objetivos y pruebas térmicas en §4.3 del plan; calidad equivalente en A/B.

## S2 — Inputs y tiempo

Definir un tick de simulación con relación explícita a vblank/ciclo guest y polls.
Input por jugador: tick, máscara normalizada, política SOCD/perfil; separar
captura host, consumo guest y entrega de red. Los anchos/endianness wire se fijan
en T00/T06 antes de implementar protocolo. No asumir que contador Dojo es vblank.
Registrar confirmación/predicción; igualar inputs consumidos, no solo teclas enviadas.

## S3 — Estado y rollback

Estado canónico por subsistema: CPU/FPU, RAM/VRAM/ARAM, audio/ARM7/DSP, scheduler,
IRQ/DMA, dispositivos y PVR/TA pertinente. Excluir solo datos host reconstruibles
documentados. Nada de punteros/padding nativo en hashes interoperables.
No prometer compatibilidad de savestates nativos entre builds. Versionar esquema
canónico y simular en ambos dynarecs; snapshots nativos pueden ser locales.
Contratos a fijar: save(tick), restore(tick), step(inputs), hash_confirmed(tick),
vida útil de buffers, límite de profundidad y política de invalidación.
G2 es requisito previo a optimizar; toda escritura pertinente, incluida DMA,
participa en seguimiento de memoria. No reducir el estado a actores del juego.

## S4 — Matchmaking y transporte

Backend Linux, endpoint común HTTPS + eventos; sesión, presencia, salas por código,
cola pública, cancelación y negociación de conexión. Identidad de compatibilidad:
versión de simulación/protocolo/perfil, ROM/BIOS/estado inicial; hash del binario
registrado por destino, no exigido idéntico entre plataformas.
GGPO se conserva inicialmente. Partida P2P con relay de respaldo, matchmaking fuera
de la ruta crítica. T06 evalúa qué existe upstream/Dojo antes de escoger librerías.

Máquina de estados propuesta: idle → queued/room → negotiating → playing → ended;
cancelación/timeout por transición, reconexión explícita e IDs idempotentes.
T06 entrega OpenAPI/esquema de eventos, errores y versionado con mock. Tecnologías,
puertos, autenticación y esquema wire exacto se eligen mediante ADR, no por este
borrador. Capacidad y fallos se aceptan con §4.2 del plan.

## S5 — Evidencia

Manifest de identidad + inputs + métricas + logs + hashes confirmados por ancla +
primera divergencia + comandos + límites. Rendimiento sin trazado pesado;
medir overhead del harness. CPU/GPU, tiempo guest, pacing y red por separado.
Repeticiones/IC95 %/umbrales del plan obligatorios. Usar plantilla de evidencia.
Un PASS de compilación no acepta juego, rollback, plataforma ni netcode.

### Decisiones parciales del piloto B/D (2026-10-29; no contrato global v0)

El dueño del proyecto asume T00 y fija para este piloto: `input-consumed` requiere
lectura real SH-4 (incluida la ruta dynarec ejecutada), con ciclo guest
`sh4_sched_now64()` y ordinal monótono; coin se reproduce desde fuentes por slot,
ejecutando la transformación original y verificando cada retorno A0 `0x280`; un step
parte de una frontera quiescente, pasa el siguiente VBlank y termina después de drenar
DMA/callbacks ya iniciados, pudiendo contener cero o más lecturas. El checkpoint lógico
T08 incluye `mapleInputState[4]` completo y cursor replay. No se presume compatibilidad
de savestates entre backends/builds.

El dueño autoriza solo un hook temporal post-load de `shop_readm` para Debug/laboratorio,
bajo coordinación T08. El hook no habilita recorder/player, cambios de scheduler o
serialización, ni comparación B/D. T00 sigue `NEEDS_EVIDENCE` hasta obtener traza SH-4
real/dynarec y validar el predicado de drain; T08 requiere revisión de serializer,
restore y atomicidad. Ver [propuesta temporal](../kofxi/studies/state-replay-p0-temporal-contract-proposal.md)
y [paquete de revisión](../kofxi/studies/state-replay-p0-t05-review-package.md).

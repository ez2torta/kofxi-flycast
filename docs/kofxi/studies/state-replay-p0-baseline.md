# P0 — Identidad, frontera temporal y auditoría del laboratorio (borrador E1)

Fecha: 2026-10-29. Estado: **borrador de estudio, sin ejecución nueva ni verificación**.
No acredita E1, G0 ni G1. Prioridad/aceptación: [roadmap](../../roadmap.md); pertenece
al [plan de estado, replay y memoria](state-replay-memory-plan.md) §P0 y su entregable
E1 ("manifest/contrato temporal y cobertura de estado P0 revisados").

Alcance de esta sesión: **solo documento y auditoría estática de código**. No se
arrancó Flycast, no se jugó, no se modificaron ROM, BIOS, NVRAM ni savestates, y no
se escribió código de núcleo. Requiere revisión de T00/T01/T08 antes de tratarse
como contrato.

## 1. Identidad local fijada

| Campo | Valor |
|---|---|
| Repo HEAD | `5c81ba49d76e6cafdf7f1b4028e8f7c63c9ba719` (2026-10-29) |
| Rama | `experiment/kofxi-replay-state-lab` |
| Base upstream | `1b63c8a3677cc8eb1292863c4fe8fc8f94674fc8` (según procedencia) |
| EXE stock preservado | `build-windows-msvc/Release/flycast.exe`, SHA-256 `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B` |
| EXE diagnóstico histórico | `build-windows-msvc-profile/Release/flycast.exe`, SHA-256 `DD605A839C5C0C6240F3EC048D26A515D4152613DCF63FCE0EEC66328ADCA55A` |
| Build de laboratorio actual | `build-windows-msvc-lab/Release/flycast.exe`, SHA-256 `ED9A55C0AAEC951BE319D3A820A4CAC149BFF09BB92735C2E42E065CA50EFDAD`; `AW_CARTRIDGE_PROFILE` + `TEST_AUTOMATION=ON` ([ADR 0002](../../decisions/0002-lab-build-identity.md)) |
| Generador | Visual Studio 17 2022, x64 (`CMakePresets.json`) |
| ROM | `roms/kofxi.zip`, SHA-256 `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44` (233.064.957 bytes) |
| BIOS | `roms/awbios.zip`, SHA-256 `DC44E2FA5F3184F6502E2544B1590223437C366CDFC9E3983CCB32D83C350772` |
| NVRAM runtime del perfil | `build-windows-msvc-profile/Release/data/kofxi.zip.nvmem`, SHA-256 `AD695471BE94012C423510346634BF8CF17842C78C80DDB390802B989647F716` (**local, no versionado**) |
| Fixture versionado | `tests/fixtures/kofxi/unlocked-spanish.nvmem`, SHA-256 `F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA` |

Hallazgos de identidad (estado al 2026-10-29):

1. **Preset e identidad de build resueltos:** `windows-msvc-lab` combina
   `AW_CARTRIDGE_PROFILE` y `TEST_AUTOMATION` en un binario separado; ver
   [ADR 0002](../../decisions/0002-lab-build-identity.md). El EXE histórico
   `windows-msvc-profile` sigue siendo solo de diagnóstico y no se debe confundir
   con el nuevo build de laboratorio.
2. **El recorder sigue siendo limitado.** El nuevo EXE incorpora ambas macros,
   pero el recorder existente solo registra eventos de botón host con las
   limitaciones de §2; no equivale al recorder P1.
3. **Región inconsistente.** `build-windows-msvc-profile/Release/emu.cfg` tiene
   `Dreamcast.Region = 1` (USA), el catálogo `awbios` en
   `core/hw/naomi/naomi_roms.cpp:38-48` solo declara `bios0.ic23` como **región 0**,
   y los logs previos reportan cabecera de juego región 0. La BIOS lógica efectiva
   es región 0 aunque la config pida 1; hay que fijarlo explícitamente en E1 en vez
   de inferirlo.
4. **El instalador de NVRAM no cubre el perfil.**
   `scripts/install-kofxi-test-save.ps1` solo opera en
   `build-windows-msvc/{Debug,Release}/data` (ver
   [save local](../local-test-save.md)), no en `build-windows-msvc-profile`. La
   NVRAM del perfil es una copia runtime distinta del fixture (hash distinto),
   permitida como artefacto local pero que debe declararse por corrida.
5. **`awbios` sin verificación observada aquí.** No se abrió el emulador: la
   entrada efectiva se deduce del catálogo, no está confirmada en esta sesión.

## 2. Auditoría de `TEST_AUTOMATION`

`option(TEST_AUTOMATION "Enable test automation" OFF)` en `CMakeLists.txt:81`: no
está activo en ninguno de los dos builds. Comportamiento actual, por código:

| Aspecto | Implementación | Límite para el contrato P1 |
|---|---|---|
| Configuración | claves `record:record_input` / `record:replay_input` | opt-in por archivo de config, no por CLI del laboratorio |
| Destino | `scripts/<nombre-rom>input` derivado del content path (`gamepad_device.cpp:776-787`) | ruta implícita; sin manifiesto ni hashes |
| Qué graba | `fprintf("%ld button %x %04x")`: ciclo `sh4_sched_now64()`, puerto, `kcode` (`gamepad_device.cpp:93-96`) | **eventos host** de botón; sin ejes analógicos ni estado por poll; solo `key <= DC_BTN_BITMAPPED_LAST` |
| Cuándo graba | en `handleButtonInput` (transición de botón) | no registra el valor que consume el guest |
| Cuándo inyecta | `replay_input()` en el vblank (`spg.cpp:182-184`) | granularidad vblank, no poll |
| Aplicación | escribe `kcode[next_port] = next_kcode` mientras `next_event <= now` (`gamepad_device.cpp:940-954`) | sin verificación de lo entregado |
| Fin de archivo | `NOTICE_LOG` + `do_screenshot = true` | no distingue sesión completa de replay truncado |
| Timeout | `die("Automation time-out after 5 s\n")` | mata el proceso; no es recuperación de prefijo |
| Tiempo | offset fijo de "swirl" `UseReios` (`gamepad_device.cpp:926-933`) | depende de BIOS HLE; el perfil usa BIOS real |
| Integridad | ninguna | sin versión, checksum, chunks ni anclas de estado |

Conclusión: `TEST_AUTOMATION` es una **pista útil y no un recorder**, exactamente lo
que el plan anticipa en §P0 y §5. No se activa sin resolver sus límites.

### Hallazgo de compilación (2026-10-29)

Al habilitar `TEST_AUTOMATION` en el preset `windows-msvc-lab` (MSVC 19.44 x64), el
código del recorder **no compilaba**: `error C2672` en `gamepad_device.cpp:930` y
`:932` porque `std::max((int64_t)now - 2152626532L, 0L)` mezcla `long` (32 bits en
Windows) con `int64_t`; además `warning C4477` en `:95` y `:946` por `%ld` con un
`u64` (truncaría el tick guardado a 32 bits en Windows). Corrección mínima aplicada
solo en la ruta bajo la macro: `std::max<int64_t>(..., 0)` y `%lld` con
`(long long)` en escritura y lectura. No cambia el comportamiento sin macro ni el
núcleo instrumentado; con eso el preset `windows-msvc-lab` compila
(`build-windows-msvc-lab/Release/flycast.exe`, ver
[ADR 0002](../../decisions/0002-lab-build-identity.md)).

## 3. Frontera temporal e hipótesis de "consumo guest"

**Corrección posterior:** `MapleConfigMap::GetInput()` es fuente intermedia, no
consumo guest. `MDCF_GetCondition` construye una respuesta Maple que luego cruza
por DMA; las monedas AW tienen camino separado por lectura SH-4 de A0 `0x280`.
Vblank puede disparar Maple, pero MDST también inicia DMA directamente. Por tanto
no se declara vblank como tick ni `GetInput()` como el valor consumido. La propuesta
detallada de puntos candidatos y preguntas a T00/T08 está en
[propuesta de contrato temporal P0](state-replay-p0-temporal-contract-proposal.md).

- `sh4_sched_now64()` sigue siendo candidato de reloj de orden en cada acceso guest.
- Vblank puede registrarse como contexto (`vblank_index`), no reemplaza el ciclo.
- `step`, el punto exacto de observación Maple/coin y la frontera de estado están
  pendientes de revisión T00/T08. No implementar replay basándose en esta hipótesis.

## 4. Mecanismos existentes reutilizables y sus huecos

| Mecanismo | Ubicación | Qué cubre | Hueco relevante |
|---|---|---|---|
| Savestate nativo | `core/serialize.cpp:21` `dc_serialize` | cadena por subsistema, versión global `V59`, flag `rollback` | NO es canónico/interoperable; `S3` prohíbe prometer compatibilidad entre builds |
| Dirty pages | `core/hw/mem/mem_watch.h` (`PAGE_SIZE`, `Watcher`) | RAM/VRAM/ARAM/ELAN a 4 KiB (verificar 16 KiB en alguna plataforma) | solo memoria; gated por `config::GGPOEnable`; sin CPU/FPU/scheduler/IRQ/MMR/dispositivos/TA |
| Delta por frame | `core/network/ggpo.cpp:134-148` (`MemPages`) | reúne páginas sucias de las cuatro memorias | es el control **parcial** de P3/P5, no base de rollback completa |
| Replay de inputs | `TEST_AUTOMATION` (§2) | eventos host de botón | sin consumo guest, sin anclas, sin integridad |

Coincide con la advertencia del plan: un dirty tracker que ignora DMA/registros no
puede ofrecerse como base de rollback.

## 5. Inventario de estado serializado y coste del control completo

`dc_serialize` recorre, en orden: AICA (registros+DSP+ARAM), `sb` (Holly/DMA),
`nvmem`, GD-ROM, dispositivos Maple, PVR (+ contexto TA), SH-4 (regs/FPU/MMR),
BBA (si `EmulateBBA`), módem, `serialize2` (SH-4), `libGDR`, Naomi, config
(`Broadcast/Cable/Region`), `naomi_cart`, `reios` (HLE), logros.

Tamaños de plataforma Atomiswave (`core/emulator.cpp:485-491`): **RAM 16 MiB,
VRAM 8 MiB, ARAM 2 MiB**, BIOS 128 KiB, flash/NVRAM 128 KiB, **sin ELAN**.

Cota inferior de un snapshot completo: ~26 MiB de memoria más registros, contexto
TA, colas y dispositivos. El tamaño real de un `.state` local está **pendiente de
medición** (no se abrió emulador; no se escribe `0`). Este número es la línea base
del control A de P5: cualquier "ahorro" del candidato incremental se compara contra
él, y su p99 de capture/restore decide el gate.

## 6. Estrategia de build para el laboratorio (implementada; contrato replay pendiente)

Requisito del plan: comparar A/B/C/D con **el mismo EXE/config**. Eso exige un único
binario que reúna:

1. inyección/registro de inputs (hoy dependiente de `TEST_AUTOMATION`);
2. telemetría de recursos (`AW_CARTRIDGE_PROFILE`, ver
   [backends experimentales](experimental-rom-backends.md));
3. de forma opcional, los backends B/C/D para P7;
4. todo lo demás idéntico al control stock.

El preset `windows-msvc-lab` implementa la combinación de macros en un binario
separado y no reemplaza el stock `8B2ACB...`; ver [ADR 0002](../../decisions/0002-lab-build-identity.md).
El identificador impreso en logs y la medición del overhead siguen pendientes. Esto
resuelve la identidad del ejecutable, no aprueba el contrato de inputs ni autoriza
implementar el recorder P1.

## 7. Estado, bloqueos y siguiente paso

- **E1: no completado.** Falta decisión explícita del contrato
  `capture/restore/step/input-consumed` (propiedad de buffers/hilos) con T00/T08.
- **P2 sigue bloqueado** por [resolución del actor AW](../runtime/player-memory-resolution.md).
- **Siguiente entregable:** el diseño del laboratorio P1 en
  [state-replay-p1-lab-design.md](state-replay-p1-lab-design.md), pendiente de la
  frontera temporal y del contrato de replay; la identidad de build de §6 ya está
  resuelta por ADR 0002.
- Los hashes de ROM/BIOS/NVRAM son de archivos **locales** de este equipo; no se
  versionan ni se distribuyen. Ningún número aquí es un resultado.

## Límites de esta evidencia

Auditoría estática: sin ejecución, sin capturas de RAM, sin anclas, sin A/B. La BIOS
efectiva y la NVRAM por corrida se infieren, no se observan. `TEST_AUTOMATION` no se
compiló. Un hash de archivo no certifica procedencia ni legalidad del contenido.

# ADR 0002 — Build identificable del laboratorio (replay + telemetría)

Estado: **aceptado e implementado, 2026-10-29**, por decisión explícita del proyecto
(usuario): preset registrado y compilación Release verificada. Queda pendiente solo
el *identificador de perfil impreso* en logs. Los roles T00/T01/T05/T08 actúan como
revisores de la implementación, no como bloqueantes de la decisión.

## Contexto

El [plan de estado, replay y memoria](../kofxi/studies/state-replay-memory-plan.md)
§P7 exige reproducir el mismo paquete A/B/C/D con **el mismo EXE/config**. Hoy no
existe ese binario:

- `build-windows-msvc-profile` (perfil de diagnóstico) define `AW_CARTRIDGE_PROFILE`
  vía `CMAKE_CXX_FLAGS` y tiene `TEST_AUTOMATION=OFF`.
- El stock preservado no define ninguna de las dos macro.
- El recorder/reproductor de inputs vive bajo `#ifdef TEST_AUTOMATION`
  (`core/input/gamepad_device.cpp:53`, `core/hw/pvr/spg.cpp:182`).
- `CMakePresets.json` solo declara `windows-msvc` (`build-windows-msvc`); el
  directorio del perfil no tiene preset ni receta reproducible.

Consecuencia: quien mida backends no puede reproducir inputs, y quien use el
recorder no mide recursos. La [auditoría P0](../kofxi/studies/state-replay-p0-baseline.md)
documenta este conflicto de identidad.

## Decisión propuesta

Declarar un preset `windows-msvc-lab` (binario aparte, p. ej.
`build-windows-msvc-lab`) que:

1. defina simultáneamente `AW_CARTRIDGE_PROFILE` y `TEST_AUTOMATION`;
2. incorpore un **identificador de perfil** legible en build/versión, para que la
   evidencia pueda distinguir el binario del stock;
3. mantenga todo lo demás igual al control (mismo `/O2 /DNDEBUG`, mismo renderer);
4. **no** sustituya ni enlace sobre el stock preservado
   `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B`;
5. mida y publicite el overhead de la instrumentación, que se resta del A/B.

Se documenta en `CMakePresets.json` para que otro equipo reconstruya el binario.
Un prefijo/etiqueta de identidad en los logs (`run.json` y manifiesto P1) registra
build, macros activas y hashes de EXE/ROM/BIOS/NVRAM por corrida.
El preset no autoriza por sí solo comparaciones A/B futuras ni acepta G1/G3.

## Alternativas consideradas

- **Seguir separando builds** (perfil para recursos, otro para replay): barato hoy,
  pero rompe "mismo EXE" y arriesga atribuir diferencias al backend; ya ocurrió en
  el piloto B/C/D.
- **Activar `TEST_AUTOMATION` en el preset `windows-msvc` existente**: contamina el
  build de desarrollo y no aísla la identidad del laboratorio.
- **Un solo macro que englobe ambas capacidades**: acopla permanente el recorder al
  perfil de recursos; difícil de retirar y de medir overhead por separado.
- **Reemplazar el stock por el instrumentado**: prohibido por la decisión de
  conservar el Release funcional (ver [backends experimentales](../kofxi/studies/experimental-rom-backends.md)).

## Consecuencias

- Positivas: identidad única y reproducible para P1/P7; overhead medible; el stock
  queda intacto; el paquete P1 puede declarar la build exacta.
- Costes/riesgos: un binario más que construir y versionar fuera de Git; el macro
  `TEST_AUTOMATION` arrastra limitaciones propias del recorder actual
  (`scripts/<rom>input`, timeout 5 s) que P1 reemplazará; hay que evitar que
  telemetría y replay alteren timing guest (medir el overhead, no asumirlo cero).
- Nota: ampliar `AW_CARTRIDGE_PROFILE` a otro backend obliga a actualizar los
  marcadores que verifica `scripts/run-kofxi-rom-benchmark.ps1`.

## Pendientes / criterio de aceptación

- Aprobación explícita del proyecto (usuario) — hecha el 2026-10-29.
- Registrar el preset, la receta y el identificador de perfil en la evidencia:
  **preset registrado** en `CMakePresets.json` (`windows-msvc-lab` +
  `windows-msvc-lab-release`) y documentado en
  [entorno de desarrollo](../development-setup.md). Configure verificado el
  2026-10-29 en `build-windows-msvc-lab/`: `TEST_AUTOMATION:BOOL=ON` y
  `CMAKE_CXX_FLAGS` con `/DAW_CARTRIDGE_PROFILE`, generador VS 17 2022 x64.
- **Compilación Release verificada** el 2026-10-29 (MSVC 19.44.35229 x64):
  `build-windows-msvc-lab/Release/flycast.exe`, 14.261.248 bytes, SHA-256
  `ED9A55C0AAEC951BE319D3A820A4CAC149BFF09BB92735C2E42E065CA50EFDAD`. El binario
  contiene los marcadores de backend (`all_chip_words_and_PIO_inverse`,
  `all_extracted_bytes_and_G1_records`, `AW host:`) y de replay
  (`Input replay terminated`, `Automation time-out`).
- Hizo falta un **arreglo mínimo de compilación** en la ruta bajo `TEST_AUTOMATION`
  (`core/input/gamepad_device.cpp`): `std::max<int64_t>(..., 0)` en `:930`/`:932`
  y `%lld` con `(long long)` en `:95`/`:946`. No afecta al código sin macro. El
  recorder sigue siendo el parcial descrito en
  [P0](../kofxi/studies/state-replay-p0-baseline.md#2-auditoría-de-test_automation).
- `scripts/run-kofxi-rom-benchmark.ps1` usa ahora por defecto
  `build-windows-msvc-lab/Release/flycast.exe`, valida marcadores de telemetría y
  replay, y registra directorio/perfil en `run.json`. `-BuildDirectory` permite
  seleccionar otra carpeta de build explícitamente, pero también exige ambas
  capacidades; el EXE de `build-windows-msvc-profile` anterior (replay OFF) se
  rechaza correctamente.
- Pendiente: marcador de perfil **impreso** en logs (hoy la identidad se deriva de
  los marcadores del binario + hashes de `run.json`).
- El launcher se validó con `-DryRun` para `original` y `extracted`; el perfil
  histórico sin `TEST_AUTOMATION` es rechazado. Esto verifica selección de EXE y
  argumentos, no arranque de Flycast, comportamiento de backend ni benchmark.
- Retirar o revisar el offset fijo de `UseReios` del recorder si se mantiene la
  frontera temporal de P0.
- Verificar el SHA-256 del stock antes y después de cualquier cambio de presets:
  sigue siendo `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B`
  tras registrar el preset (no se recompiló ni relinkó esa carpeta).
- La identidad de cada corrida se registra por hashes (EXE/ROM/BIOS/NVRAM) en
  `run.json`/manifiesto P1; un banner impreso adicional queda pendiente.

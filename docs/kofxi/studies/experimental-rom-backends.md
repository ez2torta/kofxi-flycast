# Backends ROM experimentales de KOF XI (B/C)

Fecha: 2026-10-28. Responsable: agente de esta sesión, T05, rama
`experiment/kofxi-rom-resource-benchmark`. Prioridad/aceptación: [roadmap](../../roadmap.md).
Rutas: `core/hw/naomi/awcartridge.{h,cpp}`, `naomi_cart.cpp`, lanzador en `scripts/`.
Dependencias: T01 para baseline/replay, T00/T08 antes de integración de contratos;
no se editan scheduler, serialización, GGPO ni dynarec. Prototipos locales autorizados
por el usuario, no drivers de producción ni aceptación G3.

## Implementación y selección

Solo disponibles en builds diagnósticas con `AW_CARTRIDGE_PROFILE` (MSVC Release
local ya compilado). Por defecto siempre se usa `AWCartridge` original.

| `RomBackend` | Fuente | Almacenamiento payload host |
|---|---|---|
| `original` | `roms/kofxi.zip`, AWCartridge | 320 MiB heap lógico con huecos |
| `segmented` | Mismo ZIP cifrado, ocho segmentos | 232 MiB + metadatos; huecos virtuales `0xff` |
| `decrypted` | Mismo ZIP como referencia + ocho chips `.dec` | 232 MiB + metadatos tras validar; sin copia cifrada retenida |

No se requieren archivos nuevos del usuario: en este equipo existe
`E:/github/kofxi_aw_modding/decrypted`. Se necesitan los ocho nombres originales
del catálogo (`ax3201p01.fmem1` y los siete `ax320*m01.mrom*`) con sufijo `.dec`.
El formato es el chip completo descifrado, no un asset descomprimido ni un ZIP.
Si cambia la ubicación se indica `-DecryptedPath`.

Desde PowerShell en la raíz del repo, **una corrida por vez**:

```powershell
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend original
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend segmented
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend decrypted
# Ruta alternativa:
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend decrypted -DecryptedPath 'D:\mis chips\decrypted'
# Inspeccionar sin arrancar:
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend decrypted -DryRun
```

El script comprueba que no haya Flycast abierto y que EXE contenga los marcadores
diagnósticos, fuerza logging, GGPO desactivado, BIOS real y auto-savestates desactivados
mediante valores **transitorios**. Cada ejecución imprime su ruta de log/metadata:
`build-windows-msvc-profile/Release/rom-benchmark-runs/<fecha>-<backend>/flycast.log`.
`run.json` conserva argumentos y SHA-256 del EXE y ZIP; no copia ROM ni saves.
La configuración, BIOS y NVRAM siguen junto al EXE, no se restauran automáticamente.
Todos estos artefactos de build permanecen fuera del versionado.

Selección manual equivalente (directorio de trabajo determina la ubicación del log):

```powershell
& .\build-windows-msvc-profile\Release\flycast.exe `
  -config 'kofxi:RomBackend=decrypted' `
  -config 'kofxi:DecryptedPath=E:\github\kofxi_aw_modding\decrypted' `
  -config 'network:GGPO=no' -config 'config:UseReios=no' `
  -config 'log:LogToFile=yes' .\roms\kofxi.zip
```

Las variantes rechazan otros sets AW, nombres desconocidos, GGPO y HLE BIOS. El
lanzador solo permite `kofxi.zip`. No hay selección silenciosa si falla un `.dec`:
se rechaza la carga, no se continúa con datos parciales. Para volver al control,
usar `original` o abrir el EXE sin esos argumentos (salvo configuración manual persistida).

## Conservación de datos e identidad

Se heredan registros AW, resolución de bancos/MPR, `GetDmaPtr`/`AdvancePtr`, PIO y
serialización de AWCartridge. El loader almacena únicamente blobs `Normal` del
catálogo y rechaza chips truncados. No se materializan los 88 MiB de huecos.

En `decrypted`, cada chip completo se compara en arranque contra el descifrado del
chip cifrado cargado del ZIP, usando su **dirección física** y clave `0xd3`. También
se compara el cifrado inverso de cada palabra para conservar el PIO crudo. Se
reemplaza un chip por vez, liberando su copia cifrada; el temporal máximo adicional
de validación es un chip de 32 MiB, no otro cartucho completo. Las lecturas en huecos
siguen descifrando `0xffff` con la dirección solicitada, no ceros ni plaintext constante.

Esto valida contra el contenido del ZIP local, no certifica una edición comercial
independiente ni legalidad/procedencia del archivo. No modifica RAM guest ni ROM.
`GetPtr` es interfaz interna de carga de blobs, no API pública de plaintext guest.
El acceso HLE que usa punteros directos no está soportado por estos prototipos.

## Evidencia local y límites

Host: i7-9750H, Windows 11 Pro 26200, RTX 2070 Max-Q, driver 32.0.16.1047;
MSVC 19.44 Release `/O2 /DNDEBUG`, renderer DX11 480p. Pruebas sin input, sin GDB ni
dumps; no equivalen al stage jugado por el usuario ni a un A/B de rendimiento.
EXE diagnóstico SHA-256:
`67ABD30E164F1301F3106FCF722018EB7706D014ED21A3C474220966AA52C084`.

- `original`, `segmented`, `decrypted`: logs de arranque `THE KING OF FIGHTERS XI`,
  muestras CPU/memoria periódicas y cierre normal mediante `CloseMainWindow`.
- `decrypted`: ocho verificaciones completas aprobadas, 243.269.632 bytes;
  ~20 s hasta cart-init con validación en cada arranque. No confundir con carga guest.
- Directorio sin chips `.dec`: carga rechazada con `Cannot open decrypted chip`,
  sin evento runtime de juego ni fallback silencioso. `-DryRun` verificado sin
  arrancar Flycast. Checks de tamaño y discordancias implementados; no se inyectaron
  archivos corruptos en esta prueba.
- Compilación `ClCompile` de Release normal sin macro aprobada, sin enlazar ni
  reemplazar el EXE stock (conserva SHA-256
  `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B`).
- Muestras cart-init de un smoke: private bytes original 546.988.032;
  segmentado 453.939.200; descifrado 454.590.464. Son muestras exploratorias, no ahorro
  sostenido acreditado; la reducción de **almacenamiento de payload** sí es 88 MiB.
- `AW runtime` distingue `rom_logical_bytes` (320 MiB) de `rom_storage_bytes`
  (320/232 MiB); CPU y working set se miden por proceso con el logger existente.

Pendiente: combate manual comparable en las tres variantes, anclas/hashes guest,
corpus de bancos/límites/PIO, save/restore, overhead A/B, matrices ARM64/otros SO.
Las consultas y la búsqueda de segmentos tienen overhead; no se promete mejora CPU.
El control instrumentado usa despacho virtual para comparar proveedores; repetir
contra el Release stock preservado para cuantificar el overhead del laboratorio.
El tiempo de validación entra en CPU total y picos desde inicio del proceso: separar
arranque/validación de carga guest y combate, sin ocultar el coste inicial.

`fs_extracted` **no está implementado**: faltan EPR/tablas/padding y política de
fallback por rango para un proveedor D completo. Tampoco se elimina la descompresión
guest `DECORD_TASK`. No se distribuyen ROM, chips `.dec` ni assets extraídos.

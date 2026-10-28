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

## Pareja manual B/C del usuario (2026-10-28, 19:56–20:01)

Fuentes locales bajo `build-windows-msvc-profile/Release/rom-benchmark-runs/`:
`20261028-195647-749-segmented/{run.json,flycast.log}` y
`20261028-195907-019-decrypted/{run.json,flycast.log}`. Mismo EXE SHA-256 indicado
arriba y ZIP SHA-256 `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`.
Sin proceso Flycast vivo al analizar; ambos logs terminan con cierre del renderer.
C aprobó los ocho chips; ninguno de los logs tiene entradas de nivel error.
Esto no sustituye hashes/anclas guest ni confirma por sí solo ausencia de fallo visual.

| Métrica | B cifrado segmentado | C `.dec` segmentado |
|---|---:|---:|
| Tiempo proceso hasta cart-init | 1,417 s | 19,770 s |
| CPU proceso hasta cart-init | 1,547 s | 20,578 s |
| Intervalos posteriores a cart-init (hasta pausa) | 103,023 s | 114,720 s |
| CPU posterior a cart-init / VBlank | 11,897 ms | 11,125 ms |
| CPU posterior equivalente a un core | 70,75 % | 66,28 % |
| Bytes DMA totales | 23.577.200 | 23.557.936 |
| Estimación acumulada bucle DMA | 798,914 ms | 203,350 ms |
| Private commit final | 540,61 MiB | 546,13 MiB |
| Pico commit proceso | 552,43 MiB | 551,27 MiB |
| Pico working set proceso | 522,13 MiB | 521,16 MiB |
| Almacenamiento de chips | 232 MiB | 232 MiB |

Método: emparejar cada `AW runtime` con el siguiente `AW host`. Sumar `interval_ms`,
`cpu_ms`, `vblanks` de muestras con `cpu_valid=1`; normalizar CPU por la suma de
VBlanks (no promediar porcentajes sin ponderar). Excluir validación inicial de la
comparación sostenida, pero mantener su coste explícito y los picos de proceso.
Los totales DMA usan el último resumen. Las fases no están anotadas por operador.

Control temporal adicional: tomar solo muestras cuyo timestamp está entre 35 y 85 s
**después del primer Resume**. Cada caso tiene 25 intervalos / 3.000 VBlanks (~50,05 s):
B 12,115 ms CPU/VBlank, C 11,271; 59,943 y 59,938 VBlanks/s. El elapsed estimado DMA
fue solo 0,01873 y 0,00420 ms/VBlank respectivamente. No asumir que estas ventanas
representan exactamente las mismas fases/acciones: hubo 1,584 MiB y 1,363 MiB de DMA.

Interpretación exploratoria:

- C reduce ~74,5 % el elapsed estimado del bucle DMA con volumen total casi igual
  (difiere ~0,08 %). No equivale a reducir 74,5 % la CPU del juego. Todavía copia a
  buffer de 32 bytes y resuelve segmentos; no es DMA directo ni elimina decoder guest.
- La diferencia observada de CPU total (~6–7 % por VBlank) es mucho mayor que la
  diferencia de bucle DMA en la ventana temporal. No atribuirla al backend: una sola
  pareja manual, distinta duración/tráfico y sin energía/térmica/escenario fijados.
- B y C conservan el mismo ahorro de payload de 88 MiB respecto al control lógico.
  Sus picos de commit difieren solo ~1,17 MiB. El control A histórico con stage dio
  641,42 MiB de pico, compatible con ~89 MiB menos aquí, pero fue otra corrida/build
  diagnóstica; no es un A/B de memoria aceptado.
- El chequeo de C añade ~18,35 s wall y ~19,03 s CPU hasta cart-init en esta pareja.
  Hoy es una opción de ensayo, no una mejora de arranque. No ocultar el coste ni
  quitar verificaciones antes de definir una identidad de caché segura.

Decisión: B es candidato simple para investigar RAM; C confirma que puede evitarse
trabajo host en cargas, pero el beneficio de CPU sostenida sigue sin demostrar.
Próximo control: A/B/C con este mismo EXE, mismos seis personajes/stage, tiempos de
fase anotados y orden alternado; después anclas/estado y pares repetidos del plan.
No iniciar backend `fs_extracted` ni parche guest basándose solo en estos promedios.

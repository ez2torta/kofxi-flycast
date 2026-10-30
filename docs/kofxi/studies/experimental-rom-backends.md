# Backends ROM experimentales de KOF XI (B/C/D)

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
| `extracted` | ZIP de referencia + manifest/payloads `fs_extracted` | Archivos mapeados read-only + fallback cifrado compacto; Windows |

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
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend extracted
# Ruta alternativa:
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend decrypted -DecryptedPath 'D:\mis chips\decrypted'
# Inspeccionar sin arrancar:
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend decrypted -DryRun
```

El script usa por defecto `build-windows-msvc-lab/Release/flycast.exe` (preset
`windows-msvc-lab`), comprueba que no haya Flycast abierto y que el EXE contenga los
marcadores de `AW_CARTRIDGE_PROFILE` **y** `TEST_AUTOMATION`, fuerza logging, GGPO
desactivado, BIOS real y auto-savestates desactivados
mediante valores **transitorios**. Cada ejecución imprime su ruta de log/metadata:
`build-windows-msvc-lab/Release/rom-benchmark-runs/<fecha>-<backend>/flycast.log`
(o bajo la carpeta indicada por `-BuildDirectory`).
`run.json` conserva argumentos y SHA-256 del EXE y ZIP; no copia ROM ni saves.
Para otra carpeta de build, pasar `-BuildDirectory`; el ejecutable debe igualmente
contener ambas capacidades. `-DryRun` inspecciona la receta sin arrancar Flycast.
Para D, `-ExtractedPath 'D:\mis archivos\fs_extracted'` sustituye la ruta por defecto
`E:\github\kofxi_aw_modding\fs_extracted`; no requiere los `.dec`.
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

La implementación D posterior se describe abajo; B/C por sí solos no leen
`fs_extracted`. Ninguna variante elimina la descompresión guest `DECORD_TASK`.
No se distribuyen ROM, chips `.dec` ni assets extraídos.

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

## Variante D posterior: `fs_extracted` mapeado (autorización explícita del usuario)

El usuario pidió probar D aunque la pareja B/C no acreditara beneficio de CPU
sostenida. No se cambia esa conclusión ni se introduce un parche guest.

```powershell
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend extracted
# Opcional, otra ruta:
.\scripts\run-kofxi-rom-benchmark.ps1 -Backend extracted -ExtractedPath 'D:\fs_extracted'
```

Selector manual: `-config 'kofxi:RomBackend=extracted'` y
`-config 'kofxi:ExtractedPath=E:\github\kofxi_aw_modding\fs_extracted'`, además de
las restricciones offline/BIOS real anteriores. Solo disponible en Windows por
ahora; otras plataformas rechazan esta variante explícitamente.

### Contrato y datos conservados

1. Carga temporalmente los chips cifrados segmentados desde el ZIP como referencia.
2. Valida magic y registros G1ROM contra la referencia, no solo contra el manifest.
   Exige índices únicos, nombre original/offset/tamaño correctos y todas las entradas
   de archivo reconocidas. Rechaza nombres con directorios/`..`, números/rangos
   inválidos, archivos ausentes, tamaños incorrectos, bytes distintos y solapamientos.
3. Abre los payloads con `CreateFileW` sin compartir escritura/borrado, y mapea vistas
   read-only. Compara **cada byte** con el descifrado AW a su dirección física. El
   offset lógico MPR se resuelve por el catálogo de siete chips, no sumando un único
   offset físico: conserva los huecos/bancos. No utiliza chips `.dec` como referencia.
4. Desmapea/remapea cada archivo después de validarlo para no retener deliberadamente
   todas sus páginas verificadas en el working set. Mantiene el handle abierto para
   impedir modificaciones concurrentes mientras se juega. Los diez archivos vacíos
   se verifican sin crear una vista de tamaño cero.
5. Conserva en heap cifrado solo los intervalos sin payload extraído (EPR, tablas,
   padding y otros bytes no cubiertos). Los huecos entre chips siguen virtuales.
   Para payloads impares, el word de borde queda en fallback: nunca mezcla media
   palabra cifrada con media descifrada. PIO reconstruye los words crudos mediante
   el cifrado inverso; no cambia registros, avance ni temporización DMA.
6. Compara el proveedor final con snapshots independientes de la ROM cifrada en
   ambos lados de **todos** los bordes de archivos/chips/huecos: plaintext, PIO y
   lecturas impares de tres bytes. Reinicia esos contadores de prueba antes del guest.

Datos locales: 1.057 filas validadas, 1.047 archivos no vacíos mapeados,
230.533.911 bytes de archivos, 1.051 spans físicos y 973 rangos de fallback cifrado
con 12.735.980 bytes (~12,15 MiB). El tamaño lógico almacenado/representado es
243.269.891 bytes: incluye 259 bytes de bordes duplicados entre mapping y fallback.
**No es un cartucho completo adicional** ni se escriben recursos procesados.

### Instrumentación, receta y límites

`KOF XI extracted profile` registra `mapped_files`, `mapped_file_bytes`,
`fallback_heap_bytes`, `mapped_word_reads` y `fallback_word_reads`. Estos últimos
incluyen consultas de metadatos/boot y DMA por palabra, no archivos únicos ni PIO.
El resumen genérico sigue registrando CPU, memoria y modos DMA. Un nombre histórico
`AW decrypt profile` mide el bucle proveedor+copy, incluso cuando los bytes provienen
del mapping y no se descifran.

`mapped_file_bytes` es extensión lógica de las vistas, **no RAM residente actual**.
Leer esos archivos puede cargar páginas y consumir memoria compartida/caché OS.
El working set y los picos del proceso son imprescindibles; bajar private commit
no demuestra por sí solo una reducción equivalente de memoria física del sistema.
La validación inicial todavía carga los chips originales y afecta los picos desde
inicio del proceso. No ocultar su coste ni usarlo como tiempo de carga guest.

Receta: mismo launcher/EXE para A/B/C/D, misma configuración, selección/equipo/stage,
anotar tiempos de fase, jugar un stage y cerrar normalmente. No ejecutar tooling
que intente modificar `fs_extracted` durante la corrida (handles read-only bloquean
escritura). Registrar primera ejecución y caliente por separado; no se ha medido
overhead ni estabilidad a largo plazo de la búsqueda binaria de spans/archivos.

### Evidencia local del 2026-10-28

Build MSVC Release diagnóstica SHA-256
`259D122CD917A54360225510443203F7D99CEBBEFED4602E26B9C7060D8D5696`, mismo host
i7-9750H/Windows 11/DX11 descrito arriba, energía/térmica sin fijar. Smoke sin input
de 40 s desde inicio de proceso y cierre normal; log local
`rom-benchmark-runs/20261028-201213-847-extracted/flycast.log`:

- 1.057 entradas/230.533.911 bytes verificados y **10.584 checks de bordes aprobados**.
- KOF XI arrancó, hubo muestras periódicas y lecturas reales tanto mapeadas como
  fallback: al cerrar, 3.112.994 y 631.278 words respectivamente. Sin errores logueados.
- Cart-init a 10,243 s: private commit 224.595.968 bytes (~214,19 MiB), working set
  239.652.864; picos de proceso WS 482.955.264 y commit 468.422.656 por validación.
  Es evidencia del logger/backend, **no comparación de combate ni ahorro aceptado**.
- B y C regresionados con ese mismo EXE: arranque, muestras periódicas, cierre normal;
  C verificó nuevamente sus ocho chips.
- Negativos con manifest/archivo **sintéticos**, sin editar los assets del usuario:
  manifest ausente, índice no numérico, tamaño incorrecto y contenido discordante
  rechazados antes de runtime. No hubo fallback silencioso a un driver distinto.
- Rebuild final (include explícito de `<memory>`) y smoke adicional de 25 s:
  `rom-benchmark-runs/20261028-201652-177-extracted/flycast.log`, mismas verificaciones
  de archivos/bordes aprobadas y runtime periódico sin errores. SHA-256 del EXE
  **actual**: `DD605A839C5C0C6240F3EC048D26A515D4152613DCF63FCE0EEC66328ADCA55A`.
  `ClCompile` sin el macro también aprobado; no se enlazó ni reemplazó el stock,
  cuyo SHA-256 sigue siendo el registrado en B/C.

Pendientes: stage humano con D, anclas de estado y trazas guest/PIO en ejecución,
restauración/rollback, pares A/B repetidos y otros SO/arquitecturas. Verificar bordes
y archivos en arranque no demuestra fidelidad hardware ni equivalencia universal.

## Corrida humana D del 2026-10-28, 23:08

Fuente: `rom-benchmark-runs/20261028-230808-699-extracted/{run.json,flycast.log}`.
EXE actual `DD605A...ADCA55A`, mismo ZIP identificado arriba. Usuario reporta reducción
visible de RAM; no hay marcadores precisos de fase/personajes/stage en el log.
1.057 entradas y 10.584 checks aprobados; 1.047 vistas de archivos, 12.735.980 bytes
fallback. Cierre normal del renderer a 125,998 s. Sin errores logueados; warnings
conocidos: autogen mipmaps B4G4R4A4 y BIOS regional 1 ausente en `awbios`.

Método de agregación idéntico a la pareja B/C (CPU válida posterior a cart-init).
Primer Resume a 9,663 s, pausa a 124,274 s. 114,615 s de intervalos, 75,391 s CPU
proceso: 65,78 % de un core equivalente y 11,046 ms CPU/VBlank. Último resumen DMA:
752.814 bloques / 24.090.032 bytes, bucle elapsed estimado 438,010 ms. Proveedor
extraído atendió 11.400.649 words mapeados y 649.409 fallback (incluye consultas
de metadatos, por eso la suma no es idéntica al total DMA).

| Métrica observada | B humano previo | C humano previo | D humano |
|---|---:|---:|---:|
| Private commit final | 540,61 MiB | 546,13 MiB | 331,60 MiB |
| Working set final | 516,28 MiB | 520,18 MiB | 328,51 MiB |
| Pico commit, desde inicio | 552,43 MiB | 551,27 MiB | 445,16 MiB |
| Pico working set, desde inicio | 522,13 MiB | 521,16 MiB | 459,41 MiB |

Ventana temporal 35–85 s tras primer Resume, 25 intervalos/3.000 VBlanks:
D 50,049 s, 59,942 VBlanks/s, 11,458 ms CPU/VBlank; private commit 298,29–320,18 MiB
y working set 297,26–318,66 MiB. B/C dieron 12,115/11,271 ms CPU/VBlank en sus
ventanas correspondientes, pero no se verificó igualdad de fase/acciones ni energía.

Conclusión exploratoria: la reducción no aparece solo en private commit; también
disminuyó el working set del proceso (~192 MiB al final frente a C). Los picos
siguen elevados por la carga/verificación inicial de chips. La CPU de D resulta
cercana a C, no hay mejora sostenida acreditada ni garantía de ausencia de stutter:
la cadencia media de VBlank no es un p99 de frame ni FPS presentado. D usa otro EXE
respecto a las corridas B/C históricas, con cambios en búsqueda de segmentos; estos
porcentajes no sustituyen un A/B con mismo build/replay. No se midió memoria global
OS/page cache ni prueba larga de fugas. La evidencia justifica continuar el piloto
de D centrado en memoria, sin aceptación G3/rollback/fidelidad hardware.

## Smoke aislado A/B/C/D con build común (2026-10-29)

Objetivo limitado: comprobar que las cuatro selecciones alcanzan `cart-init` con
un mismo binario y recoger una única muestra exploratoria de memoria durante un
arranque **sin input**. No se jugó un stage, no se registraron personajes/escenas
y no hay replay T01 con anclas; por ello no es un A/B formal de combate, CPU ni
equivalencia guest.

### Receta e identidad

- EXE del preset `windows-msvc-lab`, copiado byte por byte a cuatro directorios
  aislados junto a su propia configuración y `data/`: SHA-256
  `ED9A55C0AAEC951BE319D3A820A4CAC149BFF09BB92735C2E42E065CA50EFDAD`.
- ROM `kofxi.zip`: SHA-256
  `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`.
  BIOS `awbios.zip`: SHA-256
  `DC44E2FA5F3184F6502E2544B1590223437C366CDFC9E3983CCB32D83C350772`.
- Config `emu.cfg` idéntica en los cuatro perfiles, hash SHA-256
  `13E79E1A1D7E9A9A1E6F5FB0FD3079D385EB9D76C8A25DEBA713808539DE0B17`:
  ContentPath/BiosPath al directorio local `roms`, `Region=0`, `Language=1`,
  `UseReios=no`, auto-load/save state desactivados. El launcher añadió logging,
  GGPO off y el backend seleccionado; solo los perfiles C/D tienen su ruta de
  assets específica.
- Región 0 se eligió porque el catálogo AW (`core/hw/naomi/naomi_roms.cpp:38-48`)
  y la cabecera de juego registrados en el [baseline P0](state-replay-p0-baseline.md)
  identifican `bios0.ic23` como región 0. Un primer pase con `Region=1` dio el
  mismo warning “Region 1 bios not found in awbios” en los cuatro casos y se
  excluye de esta tabla. Con región 0 no salió ese warning; sí se repitió en las
  cuatro corridas el warning DX11 conocido de autogeneración de mipmaps B4G4R4A4.
- Runs: `20261029-103845-471-original`, `20261029-103932-756-segmented`,
  `20261029-104019-746-decrypted`, `20261029-104106-665-extracted`. Metadatos y
  logs están solo bajo el temp local aprobado
  `$env:LOCALAPPDATA\Temp\opencode\kofxi-t05-region0-isolated-20261029-1530\`;
  ROM/BIOS no se copiaron. Cada EXE tuvo `data/` propia, no se reutilizó ni se
  sobrescribió la NVRAM personal. Los cuatro Flycast se cerraron normalmente tras
  unos 45 s; no quedó proceso activo.
- Comando por perfil: `.\scripts\run-kofxi-rom-benchmark.ps1 -Backend <backend>
  -BuildDirectory <temp-root>\<backend>`. Se dejó cada proceso correr unos 45 s
  sin input intencional y se cerró con `CloseMainWindow`; el runner registra
  argumentos, hashes de EXE/ROM y ruta de log en `run.json`.

### Observaciones (una pasada, orden A→B→C→D)

El tiempo de cart-init es el timestamp del log desde que inició Flycast, no una
medición aislada de lectura de ROM o tiempo hasta menú. La memoria es la última
muestra del proceso al cerrar aproximadamente a los 45 s; picos desde el arranque.

| Backend | Storage lógico del cartucho | Cart-init (log) | Private bytes al cierre | Working set al cierre | Pico private commit | Pico working set |
|---|---:|---:|---:|---:|---:|---:|
| A original | 320 MiB | 1,742 s | 568,73 MiB | 564,08 MiB | 572,90 MiB | 565,93 MiB |
| B segmented | 232 MiB | 1,782 s | 481,34 MiB | 476,16 MiB | 485,43 MiB | 477,94 MiB |
| C decrypted | 232 MiB | 20,282 s | 481,56 MiB | 476,48 MiB | 485,25 MiB | 482,83 MiB |
| D extracted | 232,00025 MiB | 10,329 s | 262,86 MiB | 266,71 MiB | 443,51 MiB | 470,89 MiB |

En esta muestra, respecto de A, B/C terminan con ~87 MiB menos de private bytes y
~88 MiB menos de working set; D, ~306/~297 MiB menos respectivamente. D todavía
alcanza picos de ~444 MiB commit y ~471 MiB working set durante inicialización.
La lectura `rom_storage_bytes` valida la representación que reporta el driver,
no memoria física global: D usa archivos mapeados, y el page cache del OS no está
medido. La validación inicial de C/D y sus cart-init más tardíos quedan incluidos;
no se extrapola ese coste a carga de stage ni al combate.

Validación de selección por log: B reporta `backend=segmented` y
`validation=catalogued_segments`; C verificó los ocho chips y
`PIO_inverse=passed`; D reportó 1.057 archivos, 10.584 checks de bordes aprobados,
230.533.911 bytes mapeados y 12.735.980 bytes de fallback. A reportó
`backend=original`. Región 0 y `UseReios=no` se solicitaron en la config, y ya no
apareció el warning de BIOS región 1; aun así no se capturó una identificación
positiva del BIOS realmente cargado, así que esa identidad no se declara verificada.

CPU por VBlank en las últimas muestras no se interpreta: se dejó correr sin
interacción, las fases y el número de VBlanks al pausar difieren, y los valores
son de baja actividad. No demuestra mejora de CPU, juego fluido, igualdad de
estado, comportamiento visual ni reducción de stutter. Una única pasada ordenada
tampoco separa efectos de caché/orden ni cuantifica variación. El host/energía y
la temperatura no se registraron en esta corrida.

### Decisión

El smoke confirma que el EXE común acepta los cuatro backends y alcanza el hook
`cart-init` con región 0, sin error `E[` en los logs. La muestra apoya continuar
investigando el ahorro de working set de B/C/D, especialmente D, pero no cierra
la decisión de optimización: falta una receta T01 con estado/replay verificable,
escenario de stage común, mediciones repetidas AB/BA, fases marcadas, CPU/GPU/OS
page cache y control de temperatura. No es aceptación G3 ni rollback/fidelidad
hardware.

### Repetición exploratoria B/D sin input (2026-10-29)

Para comprobar si la diferencia de memoria del smoke anterior sobrevivía a una
repetición, corrí cuatro procesos aislados en orden **D–B–B–D** (`extracted`,
`segmented`, `segmented`, `extracted`), 45 s por proceso desde el inicio y cierre
normal. No se introdujo input ni se seleccionaron personajes/stage; no es un replay
ni una prueba de combate. Cada perfil usó una carpeta `Release` temporal nueva con
el mismo EXE, el mismo `emu.cfg` de región 0 (SHA-256
`13E79E1A1D7E9A9A1E6F5FB0FD3079D385EB9D76C8A25DEBA713808539DE0B17`) y `data/`
vacía al arrancar; no se copiaron ROM, BIOS ni NVRAM. La ROM se leyó desde su ruta
local; su SHA-256 fue `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`;
BIOS `awbios.zip`: `DC44E2FA5F3184F6502E2544D1590223437C366CDFC9E3983CCB32D83C350772`.
La config solicita BIOS real (`UseReios=no`), pero el log no confirma positivamente
qué BIOS fue cargada.
En los cuatro `run.json`, el EXE coincide byte por byte (SHA-256
`ED9A55C0AAEC951BE319D3A820A4CAC149BFF09BB92735C2E42E065CA50EFDAD`). Los datos
runtime creados quedaron bajo perfiles temporales no versionados.

Memoria del proceso según `AW host`, muestras al cierre cerca de 45 s y máximos
desde el inicio. La primera columna de memoria por proceso es la muestra más cercana
a 15 s tras `cart-init` (el logger muestrea por intervalos, así que no cae exactamente
en 15 s):

| Backend/run | `cart-init` desde inicio | Muestra tras init | Private / WS en esa muestra | Private / WS al cierre (~45 s) | Pico commit / pico WS |
|---|---:|---:|---:|---:|---:|
| D `135320-055` | 24,785 s | +13,362 s | 261,56 / 265,72 MiB | 261,55 / 265,75 MiB | 443,08 / 470,93 MiB |
| B `135407-595` | 1,874 s | +13,417 s | 480,57 / 476,55 MiB | 480,16 / 476,40 MiB | 484,26 / 478,19 MiB |
| B `135454-578` | 1,843 s | +16,851 s | 481,51 / 476,86 MiB | 480,37 / 476,22 MiB | 484,78 / 478,25 MiB |
| D `135541-515` | 10,575 s | +16,663 s | 262,90 / 265,59 MiB | 263,93 / 266,77 MiB | 444,54 / 470,93 MiB |

En estos dos pares exploratorios, D mostró aproximadamente **219 MiB menos de
private bytes** y **211 MiB menos de working set** que B en las muestras tardías.
El patrón también aparece en las muestras finales. Sin embargo, el pico de working
set solo bajó ~7 MiB y el pico de commit ~40–41 MiB: los picos de validación siguen
siendo relevantes. En D se verificaron en ambas corridas 1.047 archivos mapeados,
230.533.911 bytes y 12.735.980 bytes de fallback; los logs no contienen `E[` y
terminan con cierre normal.

La variación de `cart-init` de D (10,575–24,785 s) es grande frente a B (1,843–1,874 s).
No se controló ni vació la caché del sistema de archivos, así que no se atribuye la
variación a caché fría/caliente ni se toma como un coste estable. Tampoco se midió
memoria global/page cache, energía/temperatura, p99 de frame o fases jugables. La
disminución tardía del consumo **del proceso** apoya continuar el piloto de RAM para
D, pero no demuestra ahorro equivalente de RAM física del sistema, mejor rendimiento,
equivalencia de combate ni aceptación G3. Es evidencia host dirigida, con dos muestras
por backend y sin replay; no sustituye los pares repetidos del plan de recursos.

Los cuatro `run.json` y logs se conservan localmente, no en Git, bajo
`%LOCALAPPDATA%\Temp\opencode\kofxi-t05-repeat-20261029-135237-0ab07373\`.

### Sesión manual 1P vs 2P: D/B (2026-10-29)

Tras el smoke sin input hice una prueba exploratoria con una pelea local 1P vs 2P
por backend, según el reporte del usuario. No se grabaron inputs ni vídeo; el logger
no identifica jugadores, personajes, stage ni comienzo/fin de la pelea. Por ello las
duraciones de abajo son del proceso/log completo —incluyen arranque, menús y cierre—,
no el tiempo cronometrado de combate ni sesiones pareadas.

Se usaron los perfiles temporales aislados `extracted` y `segmented`, preparados con
`data/` vacío y sin copiar ROM, BIOS ni NVRAM personal. Ambas `run.json` identifican el mismo EXE
(SHA-256 `ED9A55C0AAEC951BE319D3A820A4CAC149BFF09BB92735C2E42E065CA50EFDAD`) y ROM
(`8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`). Los archivos
`emu.cfg` finales de ambos perfiles tienen el mismo SHA-256
`670F0898135E41CF1F568986671F882CB2BA2AAD15E09747B752D984DDEFF2BB`. Cada perfil
generó sus propios archivos runtime de datos; no se restauró un estado ni una NVRAM.
La selección del backend y la ruta de assets variaron según lo esperado, pero los
perfiles de entrada no quedaron idénticos: `extracted/mappings/` contiene
`SDL_Keyboard_arcade.cfg` y `SDL_Default Mouse_arcade.cfg`; `segmented/mappings/`
solo contiene `SDL_Default Mouse_arcade.cfg`. No se registró qué mandos físicos
participaron ni se verificó igualdad de mapeos; esto limita aún más la comparación.
El BIOS se solicitó desde la misma ruta local y con `UseReios=no`, pero el log no
certifica qué BIOS se cargó positivamente.

Resumen calculado de las muestras `AW host` con `memory_valid=1`; p50/p95 usan
nearest-rank. Private bytes y working set son **del proceso**; máximos muestreados no
son memoria global del sistema:

| Backend | Proceso/log | `cart-init` | Muestras | CPU p50 (ms/VBlank) | Private p50/p95/máx/final | WS p50/p95/máx/final | Pico commit / pico WS |
|---|---:|---:|---:|---:|---:|---:|---:|
| D `extracted` | 524,683 s | 11,130 s | 132 | 0,521 | 312,61 / 330,10 / 334,49 / 330,68 MiB | 331,66 / 357,96 / 362,15 / 361,25 MiB | 443,52 / 470,80 MiB |
| B `segmented` | 423,199 s | 1,983 s | 120 | 0,781 | 530,94 / 545,43 / 551,88 / 547,19 MiB | 518,34 / 529,51 / 533,79 / 533,77 MiB | 554,38 / 535,62 MiB |

Los dos logs terminaron con cierre normal del renderer, sin `E[` (0 errores de ese
nivel). En esta pareja manual se observa menos memoria del proceso para D en las
muestras y picos registrados: frente a B, D tuvo ~218,33 MiB menos de Private p50,
~186,68 MiB menos de WS p50 y ~110,86/~64,82 MiB menos de pico commit/WS. Pero los
mapeos, duración, selección/escena y secuencia no se controlaron ni anotaron. `cart-init`
y CPU/VBlank tampoco son una comparación de rendimiento bajo carga equivalente. La
señal justifica continuar la investigación de memoria; no demuestra que toda la diferencia
se traduzca en RAM física libre, ni que D sea más rápido, visualmente equivalente o mejor
para rollback. No es A/B reproducible, no satisface el gate de replay T01/T00/T08, y no
es aceptación G1/G3.

`run.json` y logs completos quedan localmente, sin versionar ROM/BIOS/NVRAM, bajo
`%LOCALAPPDATA%\Temp\opencode\kofxi-t05-2p-20261029-142300\` (`extracted/Release/rom-benchmark-runs/20261029-142317-352-extracted/`
y `segmented/Release/rom-benchmark-runs/20261029-143300-660-segmented/`). El logger
muestrea cada ~3,7 s e incluye menús/idle: no se dispone de fases de combate marcadas,
inputs consumidos, anclas guest, temperaturas ni medición de page cache.

### Siguiente gate T05 y bloqueo vigente

El worktree revisado en esta iteración no contiene un harness de replay: `benchmarks/`
solo tiene su README de laboratorio pendiente, no hay player/recorder P1 en `scripts/`
ni paquete con estado inicial y anclas en los artefactos versionados. El replay
Fightcade estudiado tampoco aportó su cuerpo binario. Por tanto, no hay un input común
reproducible con el que iniciar el ensayo de stage; este smoke no llena ese vacío.

El paso verificable antes de una comparación de stage es completar la revisión T01/T00/T08
del contrato P0/E1 y del diseño P1: el dueño ya decidió que `input-consumed` exige
lectura SH-4 real (incluida dynarec), no solo respuesta Maple entregada; eligió coin desde
fuentes por slot y validar cada retorno A0 `0x280`, y fijó `sh4_sched_now64()`+ordinal
monótono, y step hasta el siguiente VBlank con drain de DMA/callbacks iniciados. Debe
validarse el predicado de drain y la revisión del checkpoint quiescente; el dueño decide
incluir `mapleInputState[4]` completo+cursor, pendiente de validar en serializer/restore
y propiedad de buffers/hilo.
T01 debe producir entonces un paquete de replay identificado, con estado inicial,
inputs consumidos, anclas y cierre íntegro, y probar que se reproduce desde proceso
nuevo antes de usarlo igual en A/B/C/D. Las preguntas del contrato siguen abiertas en
[`state-replay-p0-temporal-contract-proposal.md`](state-replay-p0-temporal-contract-proposal.md)
y los gates/controles están en [`state-replay-p1-lab-design.md`](state-replay-p1-lab-design.md).
El [paquete de revisión T05](state-replay-p0-t05-review-package.md) convierte esas
preguntas en recomendaciones concretas para respuesta de Maple, monedas y checkpoint;
permanece explícitamente pendiente de revisión, no aprobado.
El backlog asigna a esta sesión la subtarea recorder/player T01; el dueño asume las
revisiones T00/T08, aún pendientes de evidencia/validación. Se autoriza solo el hook
temporal Debug/laboratorio post-load de `shop_readm`, coordinado con T08. Esto no autoriza
recorder/player integrado, cambios de scheduler/serialización, ni GGPO. Hasta completar
la traza y cerrar gates, T05
queda limitado a evidencia host exploratoria y no se ejecuta stage A/B.

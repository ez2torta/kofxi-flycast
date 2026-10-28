# Estudio: imagen Atomiswave procesada para KOF XI

- **Estado:** BLOCKED para medición dinámica; solo trazado estático completado.
- **Pregunta:** ¿una imagen local del ROM ya procesada ahorra CPU frente a
  `AWCartridge`, sin divergencia guest ni coste de memoria que anule el ahorro?
- **Confianza:** `observed` para el comportamiento visible en el código de Flycast;
  `unknown` para el perfil real de KOF XI y el beneficio de rendimiento.
- **Estabilidad:** comportamiento de la implementación fijada abajo; no se
  afirma fidelidad hardware ni contrato universal del formato Atomiswave.
- **Juego/revisión/hash/región/idioma:** KOF XI mencionado por el usuario; revisión,
  región, idioma y hashes de su imagen no aportados ni inspeccionados. El catálogo
  de juegos de este checkout tiene un único descriptor `kofxi` (sin parent), pero
  eso no identifica el archivo local del usuario ni acredita revisión/región real.
- **Flycast/build:** repo en `02b60b91715b7f9e184c5048ebe250efaa813a47`;
  Release local `build-windows-msvc/Release/flycast.exe`, SHA-256
  `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B`, tamaño
  14,211,584 bytes. Hay un ejecutable Release local, pero no se verificó que sea
  exactamente el que el usuario ejecutó después de reiniciar.
- **Equipo observado en el entorno de trabajo:** Intel Core i7-9750H, 6 núcleos /
  12 hilos (consulta del sistema), 17,047,597,056 bytes de RAM instalada y
  NVIDIA GeForce RTX 2070 Max-Q. No se observó un proceso Flycast activo al tomar
  la muestra; estos datos no son una medición de ejecución.

## Hallazgo estático

1. `core/hw/naomi/naomi_cart.cpp` selecciona `AWCartridge` para cartucho `AW`,
   aplica `game->key` con `SetKey()` y conserva el descriptor del juego.
2. `AWCartridge::decrypt16()` en `core/hw/naomi/awcartridge.cpp` obtiene una
   palabra de la imagen fuente y transforma usando la dirección de palabra y
   `rombd_key`. Por tanto, el resultado no es simplemente una permutación
   lineal independiente de dirección.
3. La preparación EPR/MPR calcula offsets desde registros de programa, índice de
   registros/archivos y banco. Las direcciones usadas por el descifrado cambian
   con esos offsets y el banco. El comentario de la ruta indica además que el
   programa EPR participa en un checksum descifrado usado para el acceso MPR.
4. `GetDmaPtr()` devuelve como máximo 32 bytes por llamada y descifra cada palabra
   de 16 bits en el rango solicitado. `core/hw/naomi/naomi.cpp` solicita bloques
   de DMA y copia el resultado al destino guest antes de avanzar el cursor.
   Por este call graph, el coste aparece en las transferencias de cartucho a RAM
   guest; no es la traducción SH-4 ni descifrado por instrucción en cada frame.
   Un bloque completo de 32 bytes implica 16 llamadas a `decrypt16`; la capa DMA
   externa procesa hasta 1024 bytes por turno (hasta 32 llamadas a `GetDmaPtr`).
   La frecuencia y bytes totales transferidos por KOF XI siguen sin medirse.
5. Las lecturas PIO tienen un camino separado: el comentario de `ReadMem()`/
   `WriteMem()` documenta acceso sin descifrado. No se debe asumir que toda lectura
   de la ROM equivale a un DMA descifrado.
6. La descompresión de recursos del juego (por ejemplo LZSS de assets) es una
   transformación posterior/diferente; no la hace `AWCartridge::GetDmaPtr()`.
7. El descriptor `kofxi` en `core/hw/naomi/naomi_roms.cpp` lo clasifica como AW,
   fija clave `0xd3`, BIOS `awbios` y tamaño lógico de cartucho `0x14000000` (320
   MiB). Enumera una imagen de 8 MiB de programa y siete segmentos MROM de 32 MiB
   (232 MiB de payload declarado en total) situados en offsets dispersos. Estos
   nombres/CRC32 son los esperados por la base de datos de Flycast, no una
   verificación de los archivos del usuario.
8. La clave se fija desde el descriptor (`SetKey(game->key)`); en este checkout
   el código solo llama a `SetKey` al cargar el cartucho y KOF XI declara `0xd3`.
   `Init()` descifra con esa clave los campos EPR que localizan MPR, y la lectura
   MPR descifra también los offsets de registros de archivo. Una tabla preprocesada
   tendría que respetar la dirección absoluta de cada palabra y no solo descifrar
   un bloque de datos independiente.
9. `Cartridge::Cartridge(size)` hace `malloc(RomSize)` y escribe `0xff` sobre todo
    el rango (`core/hw/naomi/naomi_cart.cpp`). Para el descriptor actual eso implica
    una asignación heap de 320 MiB y tocar también las páginas de los huecos entre
    segmentos. Esto predice presión de memoria, pero no cuantifica el Working Set:
    Windows puede recortar páginas del working set, aunque el commit privado siga
    existiendo y las páginas escritas deban respaldarse; se requiere medición real.
   Tampoco se sabe si un backend procesado reemplazaría o duplicaría esa asignación.
    El descriptor solo declara 232 MiB de payload en ROM; explorar almacenamiento
    segmentado podría afectar RAM incluso si descifrar no fuera un hotspot, pero
    sería una hipótesis separada del ahorro de CPU por imagen descifrada.
10. DMA ve datos descifrados, pero PIO lee bytes fuente sin descifrar. Reemplazar
    una sola `RomPtr` cruda por una imagen descifrada podría cambiar PIO. Un backend
    específico tendría que preservar ambas semánticas (p. ej. fuente cruda para PIO
    y vista transformada para DMA, sin duplicarlas necesariamente en RAM). Esa
    segunda vista también podría anular cualquier ahorro si se materializa completa.
11. Flycast ya tiene una ruta `.bin`/`.lst` (`loadDecryptedRom`) que crea un
    `DecryptedCartridge` derivado de `NaomiCartridge` y sirve bytes directamente,
    sin `AWCartridge::decrypt16`. Es una ruta genérica de cartucho Naomi, no una
    implementación de registros EPR/MPR ni bancos Atomiswave. Por ello no se debe
    asumir que un dump AW procesado puede cargarse ahí: primero habría que validar
    el mapa de registros/boot y el acceso a MPR en una prueba privada. Investigar
    esta ruta evita duplicar un mecanismo existente si resultara compatible.
    En esta versión, `getGamePlatform()` clasifica `.bin`/`.dat`/`.lst` directamente
    como `DC_PLATFORM_NAOMI`, y `loadDecryptedRom()` busca `naomi.zip`; por tanto,
    esa ruta no selecciona el BIOS ni plataforma Atomiswave para KOF XI. Reutilizar
    el parser de archivos podría ser posible, pero no el dispatch/camino de cartucho
    sin una adaptación explícita y validación de las semánticas AW.

## Aclaración: `decrypted/` frente a `fs_extracted/` del laboratorio KOF XI

Se inspeccionó solo inventario/README/manifiesto y código de pipeline en
`E:\github\kofxi_aw_modding` (sin leer ni copiar payloads):

- `decrypted/` contiene ocho ROMs de chip terminadas en `.dec`: EPR de 8 MiB y
  siete MROMs de 32 MiB (232 MiB = 243,269,632 bytes en el inventario local).
  El README describe estos como salida de descifrado Atomiswave. Esta es la
  representación relevante para probar *evitar el decrypt por palabra de DMA*,
  siempre que el mapeo byte/dirección coincida exactamente con AWCartridge.
- `fs_extracted/` contiene 1,058 archivos del filesystem G1ROM, inventario de
  230,591,080 bytes, más `_manifest.txt`. `src/unpack.py` confirma el pipeline:
  primero descifra chips a `decrypted/`, luego ensambla MPR, parsea G1ROM y
  extrae sus archivos. No es una imagen de cartucho ni reemplazo directo para el
  DMA; pierde el layout de MPR/offsets/espacios y el EPR no está en ese filesystem.
- Los nombres del inventario incluyen assets `.lz`, `.UNI`, audio y otros formatos.
  Extraerlos del filesystem no equivale a descomprimirlos: por ejemplo, la propia
  documentación del proyecto identifica `.lz` como LZSS de recursos. Esa
  descompresión ocurre en el juego/código de recursos y es distinta del cifrado
  Atomiswave en `AWCartridge::GetDmaPtr()`.

Por tanto, la hipótesis correcta para el ahorro que pregunta el usuario es probar
si alimentar DMA desde los chips `.dec` (reconstruidos en su layout de MPR y con
PIO crudo preservado) evita suficiente CPU de `decrypt16` como para compensar
carga/acceso de archivos y memoria. **No** es probar `fs_extracted/` como ROM ni
precargar assets descomprimidos. Además, si AWCartridge solo descifra una fracción
de esos 232 MiB durante una sesión, precalcular todo puede hacer más trabajo que
la ruta actual; si el juego hace DMA repetido de los mismos rangos, precálculo o
caché sí podría ahorrar. El número de bytes/rangos DMA realmente usados es la
medición discriminante todavía pendiente.

Procedencia: `kofxi_aw_modding/README.md` (pipeline y tamaños), `src/unpack.py`
(`unpack`: pasos decrypt → build MPR → parse → extract),
`fs_extracted/_manifest.txt` (offsets/nombres, solo primeras filas consultadas) y
metadatos de filesystem del usuario. No se copiaron hashes ni contenido de ROM.
La distinción describe el pipeline; no certifica por sí sola que los `.dec` locales
sean bit a bit los que Flycast carga en la corrida medida.

## ¿Puede `fs_extracted/` aliviar CPU/RAM durante el juego?

El criterio de valor para esta línea es dar margen de CPU y RAM sostenidos al
emulador (incluido el futuro netcode), no solo acortar la pantalla de carga. Con
la evidencia disponible, precargar en el host los archivos de `fs_extracted/` no
los hace llegar automáticamente antes a RAM guest: Flycast actualmente carga la
imagen AW y el SH-4/BIOS solicita transferencias DMA; el loader de recursos del
juego decide qué archivo, destino y momento usar.

Además, `core/hw/naomi/naomi.cpp` agenda DMA por bloques de hasta 1024 bytes y
`Naomi_setDmaDelay()` configura el caso Atomiswave a 27 ciclos por unidad, con el
comentario de ~7 MB/s. La ruta copia bytes a la RAM guest y agenda el siguiente
bloque. Precargar datos en RAM host podría reducir acceso/descrypt de la fuente,
pero **no acorta el tiempo guest modelado** sin cambiar ese contrato de timing; no
se debe acelerar el DMA como atajo de netcode. Y si los recursos se usan una vez
al arrancar o en una transición, eliminar ese coste no implica ahorro sostenido
durante pelea.

Hay evidencia estática de que el juego tiene una tarea `DECORD_TASK` para recursos
y de que 301 archivos `.lz` del corpus extraído se decodificaron al tamaño que
declara su cabecera en la investigación de origen. También se documentó un área
temporal de descompresión y copias de UNI en RAM. Pero el propio informe de esa
investigación deja pendiente capturar una transición que solicite un `.lz` y
seguirlo hasta el buffer transformado; por ello no demuestra cuántas veces se
descomprime durante una pelea ni qué CPU consume en un Release de Flycast.

### Hipótesis y propuesta (sin implementación)

1. **Precarga de archivos comprimidos al host:** baja prioridad. Flycast ya tiene
   el cartucho en memoria; la precarga de `fs_extracted/` no evita DMA, decode ni
   asignaciones guest salvo que se cambie explícitamente el camino de consumo.
2. **Cache de salida descomprimida:** posible solo si una traza muestra que los
   mismos recursos se vuelven a descomprimir durante juego estable. El cache debe
   tener límite y contabilizar tamaño/evicción; cachear todas las salidas podría
   empeorar RAM y netplay.
3. **Pre-descompresión local e inyección en el loader guest:** podría ahorrar CPU
   de SH-4, pero exigiría resolver cada loader, formato, tamaño y destino dinámico,
   además de respetar la tarea/completion, allocator y estado observable. Inyectar
   buffers adelantados cambiaría timing y puede cambiar hashes/rollback; `fs_extracted/`
   por sí solo no aporta las direcciones guest ni el orden de carga. No es una
   optimización segura para iniciar sin prueba de hotspot.
4. **Mejor primer experimento:** medir por separado arranque/transiciones y pelea
   estable: CPU host total, tiempo de `decrypt16`, tiempo SH-4/tarea de decode,
   DMA bytes/ciclos, RAM Private Bytes/Working Set, y recursos/bytes realmente
   solicitados. Capturar una escena natural repetible y comparar al menos tres
    corridas instrumentadas con la misma escena; luego repetir cualquier mejora en
    Release sin instrumentación. Si el decode solo aparece en carga y su coste no
    es material en el CPU sostenido de pelea, descartar precarga como ayuda al
    netcode, aunque pueda ser una mejora de UX independiente.

La instrumentación `AW_CARTRIDGE_PROFILE` preparada en este repo solo mide el
descifrado del DMA AW; **no** mide la tarea `DECORD_TASK` del SH-4 ni otros loaders.
Un resultado bajo de `decrypt16` no basta para descartar optimizaciones de recursos,
y un resultado alto no demuestra que el decode `.lz` importe. Son hipótesis y
contadores separados; perfilar decode requiere una traza guest/ETW con símbolos o
instrumentación dedicada coordinada, sin alterar el contrato del scheduler.

Fuentes adicionales: `core/hw/naomi/naomi.cpp` (`naomiDmaSched`,
`Naomi_setDmaDelay`); `kofxi_aw_modding/docs/plans/plan_experimentos_lzss_recursos.md`
(`DECORD_TASK`, evidencia estática y límite de falta de captura runtime); y
`kofxi_aw_modding/docs/architecture/runtime/uni-ram.md` (observaciones históricas
de UNI y región temporal). La identidad de build/región y cobertura de esas capturas
no se verificaron en este checkout; se tratan como hallazgos reportados, no como
medición de CPU/RAM del Release funcional actual.

**Límite:** esto demuestra qué hace el código para cualquier imagen AW que pase
por esa clase. No demuestra qué bytes ni cuántas veces pide KOF XI, qué versión
de ROM usó el usuario, ni qué parte del coste total representa el descifrado.
El descriptor del catálogo tampoco determina automáticamente identidad regional
de la copia local. Su recorrido debe fijarse antes de probar un formato procesado.

## Muestra exploratoria: Release de KOF XI

El 2026-10-28 se capturó con `Get-Process` una instancia llamada `flycast`:
239 muestras cada ~1 s durante 240.3 s, de 17:16:15 a 17:20:16 (-03:00). El usuario
confirmó que era el Release con KOF XI y que jugó dos stages arcade. El muestreador
no registró la ruta ni hash del ejecutable, escenario/configuración o inputs; el
identificador del build y el recorrido se conservan como reportados por el usuario,
no verificados independientemente. Es un primer baseline exploratorio, no una
aceptación G1/G3 ni una prueba A/B.

- CPU acumulada del proceso: 0.266 s en la primera muestra y 171.656 s en la
  última; delta observado 171.390 s durante 240.3 s. Media aproximada: 71.3 % de
  un hilo lógico o 5.94 % del host de 12 hilos. No es tiempo de `decrypt16`.
- Working Set: mediana de muestras 604.2 MiB; máximo instantáneo muestreado
  614.57 MiB; high-water `PeakWorkingSet64` observado 616.58 MiB.
- Private Bytes: mediana 628.4 MiB; máximo muestreado 641.7 MiB (no high-water).
- La captura empezó después del arranque del proceso: primera muestra 29.77 MiB,
  segunda 229.54 MiB y tercera 528.89 MiB. La serie no cubre con precisión los
  picos anteriores a la primera muestra.
- CSV local, no versionado ni copiado al repo:
  `%TEMP%\opencode\kofxi-process-metrics.csv`. No contiene ruta de ejecutable,
  build, datos de juego ni stacks.

Las primeras muestras suben de 29.77 a 229.54 y 528.89 MiB de Working Set durante
el inicio/carga; esto es compatible con materializar el cartucho de 320 MiB, pero
también incluye gráficos y recursos. El pico superior a 600 MiB indica una
oportunidad para investigar RAM; no cuantifica qué parte se recuperaría con otro
backend. La CPU total promedia ~0.71 de un hilo lógico durante la captura; no
identifica el hotspot. Como `decrypt16()` solo corre al proporcionar bloques DMA,
no es la traducción SH-4 de cada frame y todavía no hay evidencia de que merezca
pre-descifrar el ROM. El coste podría concentrarse en carga/transferencias. No
iniciar prototipo hasta perfilar esa ruta.

## Medición específica requerida (pendiente)

No se registran cifras CPU atribuibles a `decrypt16` ni mediciones segmentadas de
arranque frente a juego. Falta confirmar ruta/hash del ejecutable, identidad/región
de la copia, configuración y detalle temporal del recorrido. No se abrieron ni
copiaron ROM, BIOS, NVRAM ni saves.

Para completar baseline en el Release identificado arriba, fijar antes de medir:

- versión/región/idioma del juego (hash local opcional; guardar solo el hash);
- BIOS/configuración relevante, backend gráfico, resolución, VSync y estado
  inicial; equipo, modo de energía, drivers y temperatura aproximada;
- recorrido natural exacto, duración y puntos/anclas: arranque, selección y una
  sesión de juego estable; repetir al menos tres veces en orden fijo;
- RAM: Working Set y Private Bytes periódicos desde arranque hasta salida, además
  de pico; distinguir RAM del proceso y memoria GPU/unificada;
- CPU total del proceso y muestreo de stacks en Release durante las transferencias.
  Para atribuir coste a `decrypt16`, usar perfil ETW con símbolos adecuados o una
  build diagnóstica coordinada que cuente llamadas/palabras y mida tiempo exclusivo.
  No introducir instrumentación en scheduler/serialización/GGPO sin T00.
- salida completa, comandos/configuración, métricas por corrida y hashes de anclas;
  nunca adjuntar ROM/BIOS ni imagen procesada.

El protocolo de producto en el plan rector requiere 10 pares A/B para hipótesis de
rendimiento; las tres corridas de esta fase solo son exploratorias para determinar
si merece la pena diseñar un experimento formal. No son aceptación G3.

### Instrumentación diagnóstica autorizada (build validada; corrida pendiente)

El usuario autorizó una instrumentación local/diagnóstica en `AWCartridge`; no es
un driver ni se activa en builds normales. Con `AW_CARTRIDGE_PROFILE` definido,
el código cuenta bloques/words DMA y cronometra con `steady_clock` uno de cada 64
bucles de `GetDmaPtr()`. Al terminar el subsistema Naomi registra muestra, palabras
y una extrapolación ponderada por palabras (el destructor es respaldo). El tiempo es **elapsed wall time del bucle**
(incluye el coste de llamada/loop y puede incluir preempción), no ciclos CPU
exclusivos; el muestreo evita llamar al reloj por cada palabra. La build diagnóstica
se compila en `build-windows-msvc-profile/`, separada del Release funcional.

Estado a 2026-10-28: Release diagnóstico compilado exitosamente en
`build-windows-msvc-profile/Release/flycast.exe` con `/DAW_CARTRIDGE_PROFILE`,
`/O2 /Ob2 /DNDEBUG` (MSVC 19.44.35229.0). SHA-256 del diagnóstico:
`F012314CEB9131F0C4E6937CF8518A7060B58F58AD12E919709B9CF80A0BCAEE`. SHA-256 del
Release funcional preservado: `8B2ACBD44D09A9A060AD618B4DCAFBDEE62675302608635FDCE3F2C190276B6B`.
La primera corrida abrió KOF XI con la build diagnóstica anterior (log confirma
`THE KING OF FIGHTERS XI`, región 0), pero no produjo el resumen: Flycast no destruye
`CurrentCartridge` al cerrar normalmente. Se corrigió el punto de volcado a la
terminación Naomi y se recompiló; falta una corrida corta con esta revisión. No hay
resultados que permitan decidir si el descifrado merece preprocesarse. No se tocó
serialización, estado guest, scheduler, GGPO, dynarec ni código habilitado por defecto.

## Puerta de decisión

- **Descartar la línea** si el perfil muestra que el descifrado es insignificante
  dentro del coste total y no hay una hipótesis concreta de carga repetida.
- **Prototipar localmente** solo si el perfil atribuye coste material a esta ruta.
  Generador y salida permanecerían fuera del repositorio; conservar `AWCartridge`
  como referencia/fallback y comparar los bytes DMA visibles al guest, recorrido,
  hashes, CPU, RAM total (incluido el archivo procesado) y tiempo de generación.
- No proponer `KOFXIDecryptedRom` como feature de producción sin equivalencia
  demostrada y mejora reproducible. No reemplazar el cartucho AW genérico.

## Fuentes

- `core/hw/naomi/awcartridge.cpp`: comentarios de formato, `ReadMem`, `decrypt16`,
  `recalc_dma_offset`, `GetDmaPtr`, `GetBootId`, serialización.
- `core/hw/naomi/awcartridge.h`: estado del cartucho serializado y buffer temporal.
- `core/hw/naomi/naomi_cart.cpp`: elección del cartucho y aplicación de `game->key`.
- `core/hw/naomi/naomi_cart.cpp`: `loadDecryptedRom()` y dispatch `.bin`/`.lst` a
  `DecryptedCartridge`; ruta genérica que requiere validación separada para AW.
- `core/emulator.cpp`, `getGamePlatform()`: extensiones `.bin`/`.dat`/`.lst`
  seleccionan Naomi genérico; no Atomiswave.
- `core/hw/naomi/naomi_roms.cpp`, descriptor `kofxi`: metadatos esperados del
  catálogo y CRC32 de los segmentos (no verificados contra la instalación local).
- `core/hw/naomi/naomi.cpp`: caller DMA y copia hacia memoria guest.
- Plan rector: `docs/plans/plan-flycast-kofxi-online.md`, G0/G1/G3 y presupuesto
  de recursos; prioridad del trabajo: `docs/roadmap.md`.

## Herramientas disponibles en este Windows

`wpr.exe` está disponible y anuncia el perfil integrado `CPU`; `wpr -profiledetails
CPU` confirma que la variante verbose incluye eventos `SampledProfile` y stacks.
`tracerpt.exe` está disponible, pero `xperf.exe` y `wpa.exe` no están en PATH.
La captura de la muestra anterior fue solo por proceso; no se inició ETW. Para una
corrida coordinada, WPR puede recoger muestreo ETW del sistema:

```powershell
wpr -start CPU -filemode
# Ejecutar manualmente el recorrido ya fijado, sin otros benchmarks concurrentes.
wpr -stop "$env:TEMP\kofxi-aw-cpu.etl"
```

El ETL puede contener actividad ajena y se queda local; no incluirlo en Git ni
compartirlo sin revisar. Aunque haya stacks, el Release local no tiene PDB al lado;
sin símbolos compatibles, no se puede confiar en la atribución a
`AWCartridge::decrypt16`. Haría falta WPA/símbolos compatibles o una build
instrumentada aprobada por T00. El Working Set y Private Bytes se registran por PID
durante arranque y sesión; el high-water debe leerse antes de cerrar y no equivale
a RAM total del sistema.

**Siguiente:** completar baseline Release con el responsable de T01/T05 y receta
reproducible; el prototipo permanece bloqueado hasta conocer el perfil.

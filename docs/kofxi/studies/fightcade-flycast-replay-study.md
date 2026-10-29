# Estudio del replay Flycast Dojo/Fightcade de KOF XI

Fecha: 2026-10-29. Estado: **el replay se abrió y se observó en ejecución mediante
Fightcade; el cuerpo binario concreto no se obtuvo ni se parseó**. Es evidencia de
que el enlace inicia la reproducción, no validación de nuestra build ni de
determinismo. Enlace proporcionado:
[`1793248201552-3726`](https://replay.fightcade.com/flycast/flycast_kofxi/1793248201552-3726).

## Resumen ejecutivo

**Sí sirve como material de estudio si se consigue el stream/archivo**: podría aportar
un recorrido real de inputs por frame, metadatos y ejemplos de la infraestructura de
replay de Dojo. **No satisface por sí solo P1/E2**: el formato descrito por el código
Dojo no garantiza identidad reproducible (ROM/BIOS/NVRAM/build), estado inicial
autónomo, inputs realmente consumidos por el guest en cada poll, checksums/anclas,
chunk checksums ni una semántica portable de ciclo guest. Lo razonable es usarlo
como corpus exploratorio para inspeccionar mapping, frecuencia/cobertura de acciones
y compatibilidad con el runner, no como baseline determinista aceptado.

## Acceso al objeto concreto

Una petición HTTP(S) al enlace devuelve **301** con:

```text
Location: fcade://stream/flycast/flycast_kofxi/1793248201552-3726.2,7007
```

Es una URI de esquema privado `fcade://`, no un enlace HTTP al cuerpo. El cliente
Fightcade instalado sí abrió la URI, pero eso no expuso el cuerpo para descarga ni
inspección offline. El sufijo `.2,7007` queda como dato literal de la redirección;
no lo interpreto como versión del formato sin especificación. El timestamp decimal
del ID corresponde a `2026-10-29T04:30:01.552Z`; no prueba cuándo se jugó la partida,
solo decodifica el componente temporal aparente del nombre.

Por tanto, no se conocen del replay concreto: tamaño/hash, versión Dojo, duración,
equipos/stage, cabecera real, registros/maple frames, cantidad de entradas ni si el
cuerpo es el formato v1/v2 descrito abajo. Aunque el cliente inicia el stream, no
deja visible una copia del cuerpo para inspección offline. Para estudiar bytes y
parser hace falta una exportación/captura por una vía soportada por Fightcade o una
especificación pública del endpoint. No guardar el cuerpo del replay en Git; anotar
hash y metadatos locales.

### Apertura runtime observada

El protocolo privado estaba registrado para
`C:\Users\Tortita\Documents\Fightcade\emulator\fcade.exe "%1"`. Abrí la URI
`fcade://stream/...` recibida del servidor. El cliente lanzó:

```text
flycast.exe -config dojo:Receiving=yes -config dojo:ActAsServer=no
-config dojo:SpectatorIP=ggpo.fightcade.com -config dojo:SpectatorPort=7007
-config dojo:Quark=1793248201552-3726 -config dojo:GameEntry=kofxi
-config dojo:Enable=yes -config dojo:EnableMatchCode=no -config dojo:RxFrameBuffer=180
```

Identidad observada: Fightcade `2.1.45`; Flycast `dojo-6.46`, exe SHA-256
`23A164CF2096E82AD49BA748D3C90D06F0AAD6DD6A9E5F34C4682B6592A6AA64`. Flycast
Dojo abrió KOF XI y mostró un combate con HUD/personajes. En capturas separadas el
timer visible avanzó de 81 a 67 y después a 31; esto confirma que la emulación estaba
avanzando mientras el cliente recibía/consumía el stream. No confirma la duración
total ni que se observara el inicio/fin completos del replay. La ventana seguía
activa al terminar esta inspección.

No apareció un archivo replay nuevo ni un log de texto utilizable; `fcade.log` quedó
vacío. El proceso padre `fcade.exe` terminó después de lanzar Flycast. El replay
queda, por ahora, accesible solo al cliente/emulador; la URI no permitió recuperar
el contenedor binario. Capturas visuales de esta sesión están solo en la carpeta
temporal aprobada y no se versionan.

## Base documental de Dojo (no equivale al objeto concreto)

Fuente inspeccionada: `blueminder/flycast-dojo`, rama `master` observada en la API
de GitHub como commit `d0e47e572b1e7b355e88bda8308c89d0c5156cbf` (committer
2025-06-30). El HEAD del repo de investigación no identifica la versión del cliente
que creó este replay; el timestamp del ID es posterior al commit consultado. No
atribuir automáticamente estos detalles al archivo enlazado sin abrirlo.

Fuentes de código:

- [`DojoSession.cpp`](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/dojo/DojoSession.cpp):
  crea `.flyr`, escribe cabecera SPECTATE_START, acumula frame records y parsea
  GAME_BUFFER/MAPLE_BUFFER/PLAYER_INFO/PLAYER_WIN.
- [`MessageWriter.hpp`](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/dojo/MessageWriter.hpp)
  y [`MessageReader.hpp`](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/dojo/MessageReader.hpp):
  cabecera binaria, longitudes/campos y lectura de strings/bytes.
- [`README.md`, sección Replays](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/README.md#replays):
  describe grabación de sesiones y reproducción en el juego correspondiente; no
  especifica el formato completo.

SHA-256 de los archivos fuente descargados para esta revisión (no del replay):

| Archivo | SHA-256 |
|---|---|
| `DojoSession.cpp` | `18C9BE610862ABA34B1E3F038043BF91A69F65E7047AE2DEDF04E435371F80EE` |
| `MessageWriter.hpp` | `3B9DE0880CB7B88EE29944F17642CE5E19AF12CC783A3162B9273DAB638C8AB2` |
| `MessageReader.hpp` | `F96E4E42CD88FBC6EDDD12BBBDF0CFD8FFF18129654B7E2225996D7EAAEDF489` |

## Esquema observado en esa revisión de Dojo

El contenedor `.flyr` versión 1 escribe mensajes concatenados. Cada mensaje empieza
con cabecera de 12 bytes: `payload_size:u32`, `sequence:u32`, `command:u32`; el
`MessageWriter` codifica esos enteros little-endian. Campos de string/datos llevan
`length:u32`; luego el payload. `GAME_BUFFER` y `MAPLE_BUFFER` llevan además un
`frame_size:u32` y records contiguos.

Comandos vistos en el writer:

| Código | Nombre | Observación |
|---:|---|---|
| 1 | `GAME_START` | declarado; no se observó en las rutinas de cabecera replay consultadas |
| 2 | `GAME_BUFFER` | frames Dojo de 12 bytes |
| 3 | `SPECTATE_START` | metadatos/cabecera de sesión |
| 5 | `PLAYER_INFO` | nombres/metadata de jugadores |
| 6 | `MAPLE_BUFFER` | frames Maple de 24 bytes |
| 7 | `PLAYER_WIN` | índice de ganador |

`SPECTATE_START` incluye versión, nombre del juego, nombres, `Quark`, match/relay
string y máscara de ejes analógicos. En versión >=3 añade MD5 y commit del savestate
de netplay. Eso es útil para identidad parcial, pero no equivale a identidad completa
de ROM/BIOS/config/build, y depender de un estado de servidor/época externa no
garantiza que el archivo sea autónomo.

### Dos clases distintas de frame

- **GAME_BUFFER (12 bytes)**: `player:u8`, `delay:u8`, `frame_number:u32` en offset
  2 y `INPUT_SIZE=6` bytes en offset 6 (hasta offset 11). La fuente crea/consume el
  contador con `memcpy` del `u32` nativo; no es un formato canónico documentado como
  endian-neutral. Los seis bytes de input y los bytes analógicos que contengan varían
  según plataforma/mapping. Se almacena una entrada por frame/actor Dojo, no cada
  poll guest observado.
- **MAPLE_BUFFER (24 bytes)**: `maple_frame_number:u32` seguido de 20 bytes de
  estado Maple. En la rama inspeccionada se usa desde replay version >=2; el parser
  extrae las 20 bytes y las coloca en `maple_inputs[frame_num]`. Representa un
  snapshot de entradas Maple por frame según Dojo, no una traza de cada acceso DMA ni
  de lectura coin A0 `0x280`.

Al crear el header de archivo en la fuente revisada, Dojo usa version 3 cuando
`GGPOEnable`, si no 1. El camino MAPLE_BUFFER se activa para versión >=2. No inferir
que `.2,7007` del redirect Fightcade significa esta versión; podría pertenecer al
enrutamiento del cliente/servidor.

Los records se acumulan en lotes de 120; el writer vuelve a vaciar el lote al ver el
sentinel de fin de 12/24 bytes de ceros ASCII (`"000..."`). No encontré en ese
writer checksum por lote, hash de sesión ni marcador global de cierre/EOF equivalente
a un manifest firmado. Falta contrastarlo con cualquier encapsulado o postproceso
que aplique el servicio Fightcade.

## Utilidad concreta para nuestro P0/P1

| Uso potencial | Valor | Reserva |
|---|---|---|
| Analizar formato/mapear por frame los 2 jugadores | Bueno si se obtiene el cuerpo y se identifica versión | No muestra necesariamente poll count ni el valor de cada acceso guest |
| Generar secuencias para explorar acciones naturales | Útil como corpus externo orientativo | Requiere ROM/BIOS/región, NVRAM, savestate, mapping y build compatibles; acciones/stage no inferibles del input solo |
| Probar lector de contenedor/lotes/corrupción | Útil para diseñar un parser y casos negativos | No reutilizar parser Dojo como formato propio ni asumir integridad del stream |
| Servir de replay determinista P1/E2 | **No basta** | No se ha establecido estado inicial completo, compatibilidad exacta, inputs consumidos por poll ni anclas completas |
| Comparar nuestros backends ROM A/B/C/D | No hasta recuperar y verificar el objeto | Podría ser valioso después como estímulo común; necesita mismo EXE/config y anclas |

La propuesta P0 de este repo observa respuesta Maple entregada/lectura coin a nivel
guest. Dojo MAPLE_BUFFER es un input/frame empaquetado, por lo que puede ayudar a
crear una adaptación de estímulos, pero no debe confundirse con telemetría de acceso
guest. Además, valores coin AW consultados por A0 podrían no quedar representados en
un Maple frame.

## Siguiente protocolo si el usuario consigue el cuerpo

1. Registrar nombre/tamaño/hash del archivo y versión del cliente/Dojo que lo abre;
   no versionar el cuerpo.
2. Parsear primero cabeceras y longitudes bajo límites estrictos; rechazar tamaños
   no alineados, comandos desconocidos y lote truncado. No ejecutar el replay para
   identificar estructura si el parser estático basta.
3. Extraer solo resumen no sensible: versión, nombres si el usuario autoriza,
   número de messages/frames, frame sizes, duración derivada, player distribution,
   sentinels y cobertura; guardar el hash y CSV agregado local/permitido.
4. Con consentimiento/recursos de juego del usuario, probar en un entorno aislado
   con Dojo compatible y comparar first/last frames, inputs de ambos jugadores y
   resultado visual. Eso sigue siendo exploratorio, no prueba de equivalencia con
   nuestro core.
5. Solo promoverlo a corpus T01 si se consigue identidad de estado inicial y
   replays repetidos con anclas. Si no, catalogarlo como replay de inputs Dojo
   externo y mantener su límite explícito.

## Límite de evidencia

Observado: redirect HTTP 301 hacia esquema privado; el cliente instalado abrió
Flycast Dojo 6.46 en modo receiving para `kofxi`, y un combate avanzó visiblemente;
además, estructura/comportamiento de la rama fuente `master` de Flycast Dojo
indicada arriba. Desconocido: cuerpo binario, versión exacta del contenedor, campos
del header, endpoint posterior de Fightcade, cobertura completa/resultado e
identidad del estado inicial. La observación demuestra que Fightcade puede
reproducir el enlace en esta instalación, no reproducibilidad fuera de Dojo ni
utilidad como baseline aceptado.

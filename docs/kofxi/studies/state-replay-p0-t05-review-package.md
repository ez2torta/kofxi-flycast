# Paquete de revisión T05 para P0/E1 — frontera de input y checkpoint

Fecha: 2026-10-29. Estado: **propuesta de revisión**, no contrato aprobado,
implementación ni resultado. Propietario documental: T05. Revisión requerida:
T00 (semántica temporal/API), T08 (estado/restauración) y T01 (recorder/corpus).
El backlog los muestra sin asignar; este documento no asigna responsables.

Autoridad: [roadmap](../../roadmap.md), [plan estado/replay](state-replay-memory-plan.md)
§P0–P1, [baseline P0](state-replay-p0-baseline.md), [propuesta temporal P0](state-replay-p0-temporal-contract-proposal.md)
y [diseño P1](state-replay-p1-lab-design.md). Se ofrece para resolver preguntas,
no para sustituir esas fuentes ni declarar E1/E2 completados.

## 1. Evidencia de código que informa la propuesta

- El controlador crea la respuesta de `MDCF_GetCondition` dentro de `RawDma`, usando
  `GetInput`, mapeo de botones y ejes (`core/hw/maple/maple_devs.cpp`). En
  `maple_DoDma`, Flycast pone la salida en `mapleDmaOut`; más tarde `maple_schd`
  la copia a RAM guest y eleva la interrupción Maple (`core/hw/maple/maple_if.cpp`).
  La ruta incluye latencia simulada. Por ello, **respuesta creada**, **respuesta
  comprometida en RAM** y **lectura SH-4 de esos bytes** son observaciones distintas.
- La moneda AW se consulta por una ruta diferente: lectura de `0x00600280` entra en
  `atomiswave::readMem_A0_006`. La primera lectura toma la rama especial de RAM test;
  las siguientes calculan el valor de cuatro slots y actualizan `coin_chute_time`
  según ciclo SH-4 (`core/hw/naomi/atomiswave.cpp`). `maple_atomiswave_coin_chute`
  obtiene el estado fuente desde el input mapeado (`core/hw/maple/maple_cfg.cpp`).
- El savestate nativo recorre subsistemas en `dc_serialize` (`core/serialize.cpp`);
  Maple serializa dispositivos, `mapleDmaOut` y estado de transferencia
  (`core/hw/maple/maple_cfg.cpp`), y el scheduler serializa su reloj y el evento
  Maple (`core/hw/sh4/sh4_sched.cpp`). Atomiswave serializa `coin_chute_time` y
  `aw_ram_test_skipped` (`core/hw/naomi/atomiswave.cpp`). Esto demuestra cobertura
  de esos campos en la ruta nativa actual, **no** que toda captura sea atómica,
  canónica, portable o segura en cualquier momento/hilo.

## 2. Recomendación sometida a decisión

### 2.1 Separar “respuesta entregada” de “guest la leyó”

Para el primer laboratorio se recomienda que el paquete registre las **respuestas de
input entregadas al guest**, y que no las denomine “lecturas guest” ni afirme que el
SH-4 las consumió salvo que esa lectura se observe explícitamente.

En Maple, registrar la respuesta efectiva completa (incluidos status, botones y ejes)
con identificador de sesión, secuencia monótona, ciclo SH-4, VBlank opcional, bus/puerto,
comando, destino/longitud y bytes. La observación candidata para entrega es la copia
de `mapleDmaOut` a RAM en el callback; conservar además el ciclo y los bytes generados
en `RawDma`, para detectar cambios entre creación y entrega. No colapsar dos respuestas
del mismo ciclo. Para replay, comparar la solicitud/control de DMA y reproducir los
bytes de respuesta acordados por T00/T08 a través del camino temporizado normal; no
inyectar `kcode` host como si fuera respuesta guest. Una lectura real posterior por el
SH-4 no se presupone.

**Decisión T00/T08:** aceptar “response-delivered” como unidad P1 reproducible, dejando
explícito que no prueba lectura guest; o exigir prueba de lectura efectiva y especificar
qué capa SH-4 la observa, incluyendo dynarec y accesos de memoria. La segunda opción
amplía significativamente el alcance y requiere coordinación; no implementarla por
inferencia desde VBlank ni desde `GetInput()`.

### 2.2 Monedas AW

Registrar cada lectura efectiva de A0 `0x280`: secuencia, ciclo SH-4, ancho/tamaño,
si fue la rama inicial de RAM test, estado fuente por slot y byte retornado. Para replay,
la recomendación es repetir el **estado fuente de los coin switches** por lectura y
ejecutar la transformación/debounce original, comprobando que el byte retornado coincide;
esto conserva la actualización de `coin_chute_time`. Registrar solo el byte de salida y
saltarse la lógica podría perder esos efectos temporales.

**Decisión T00/T08:** confirmar si el estado fuente por slot basta para reproducir la
interfaz, qué ocurre con lecturas de otros anchos/alias y cómo inyectar la fuente sin
consultar input host durante replay. El valor especial inicial debe estar en el stream o
derivarse de estado restaurado de forma demostrable, no asumirse ausente.

### 2.3 Orden, reloj y respuesta a fallos

- Usar `sh4_sched_now64()` como reloj guest candidato en cada observación; agregar un
  contador de secuencia por sesión para ordenar eventos que compartan ciclo. VBlank es
  metadato contextual, nunca unidad equivalente a cada poll.
- Validar en replay secuencia, ciclo, dominio, dispositivo/puerto, operación y bytes.
  En la primera diferencia, detener y reportar evento esperado/observado; no alinear
  ciclos ni omitir eventos. EOF sin marcador final, corrupción, identidad equivocada,
  input físico de juego o discontinuidad deben fallar explícitamente.
- Separar telemetría de input host intermedio de eventos reproducibles. Si se captura
  también la fuente host, etiquetarla como tal; no llamarla consumo guest.

**Decisión T00/T01:** fijar reloj, overflow/anchura del contador, orden entre eventos
del mismo ciclo, versionado/endianess y si se permiten eventos externos a Maple/coin.

### 2.4 Captura/restauración del estado

Proponer captura solo desde el propietario único del emulador, entre operaciones SH-4,
fuera de una pila activa de callback/DMA y sin escritura concurrente. Como punto inicial
conservador para probar: después de completar un DMA Maple (copia a RAM, estado/IRQ
actualizados), antes de la siguiente operación guest; no fijar “después de VBlank”. Si
se quiere capturar un DMA pendiente, T08 debe demostrar que payload, destino, estado de
controlador, scheduler/evento y cursor de replay se restauran juntos.

El paquete debe asociar el estado inicial a identidad estricta del mismo EXE/build,
ROM, BIOS, configuración, región/idioma, NVRAM inicial y backend; mantener el savestate
nativo como artefacto local/no portable. Guardar junto a él el cursor de secuencia de
input que se consumirá a continuación. `restore` no avanza el tiempo; `step` y su límite
temporal se fijan con T00/T08. No inferir atomicidad solo porque `dc_serialize` incluya
los subsistemas citados: validar callbacks, colas, I/O/hilos y toda fuente externa.

**Decisiones T00/T08:** propietario/hilo y exclusión mutua; checkpoint antes/después del
próximo evento scheduler; estado de todos los callbacks/colas; estado/cursor en una sola
operación; vida de buffers; política de reset, pausa, loadstate y branching. Cualquier
solución que toque scheduler, serialización, dynarec, GGPO o memwatch espera coordinación
y aprobación de sus responsables.

## 3. Gate propuesto para T01 y T05

1. T00/T08 revisan y responden las decisiones de §2; se registra una decisión aprobada
   y quién mantiene cada interfaz. Hasta entonces esto es propuesta, no permiso para
   instrumentar el core.
2. T01 define manifest/chunks y construye recorder/player mínimo en rutas asignadas,
   con entrada física bloqueada durante replay, cancelación host separada, checksum,
   marcador de cierre y reporte de primera divergencia.
3. Capturar una secuencia natural corta sin modificar RAM/estado para fabricar casos.
   Validar identidad y cobertura; reproducirla tres veces desde procesos nuevos con
   mismo EXE/config/backend: estado/anclas por subsistema iguales, secuencia/ciclos y
   respuestas iguales, EOF completo, controles negativos rechazados.
4. Solo tras ese gate, crear el paquete stage común para A/B/C/D. Separar carga y
   combate; fijar orden AB/BA y métricas/umbrales previamente según el plan de recursos.

Esto no reduce los criterios P1 (piloto de cinco minutos, tres reproducciones) ni
T08/G2; la secuencia corta sería solo smoke del recorder para detectar defectos antes
del corpus. No demuestra determinismo entre arquitecturas ni acepta G3.

## 4. Respuestas que faltan para dar por revisada la propuesta

| Revisor | Decisión requerida |
|---|---|
| T00 | Definir tick/ciclo/orden, semántica de respuesta Maple entregada frente a lectura SH-4, formato/versionado del evento y límites de `step`. |
| T08 | Aprobar estado completo, checkpoint quiescente, propiedad de hilo/buffers, scheduler/DMA pendiente y restauración atómica de cursor. |
| T01 | Confirmar responsable y rutas del recorder/player, manifest, controles, corpus corto/P1 y evidencia exigida. |
| T05 | Confirmar impacto de medición y asegurar A/B usa exactamente el paquete, build/config y límites de equivalencia aprobados. |

**Estado al redactar:** T01/T00/T08 figuran sin asignar en el backlog. Aún no se ha
recibido ni registrado su aprobación; nadie debe interpretar esta tabla como revisión recibida.
No se grabó replay, no se ejecutó combate y no se cambiaron archivos de código.

## 5. Solicitud de revisión publicada

Se publicó el [issue #1 — Revisar propuesta P0/E1 para recorder/replay de KOF XI](https://github.com/ez2torta/kofxi-flycast/issues/1)
en `ez2torta/kofxi-flycast`. Verificado por `gh issue view`: estado OPEN, sin asignados.
El cuerpo contiene el resumen y las preguntas de revisión de forma autocontenida. No se
asignaron reviewers; el issue no demuestra que los responsables T00/T08/T01 hayan
respondido ni que la propuesta esté aprobada. El paquete documental local aún no está
publicado en el remoto.

Reconsulta del API público de GitHub (2026-10-29): issue #1 sigue OPEN, con cero
comentarios, cero assignees y `updated_at` igual a su creación. Su cuerpo todavía dice
que T00/T01/T08 “siguen sin asignar”, lo que quedó obsoleto frente al backlog/roadmap
local que registra responsable y decisiones asumidas durante esta sesión. La consulta
confirma que no existe revisión remota; no se modificó el issue porque actualizarlo o
publicar la evidencia local requiere autorización explícita.

Se solicita a quien tome T00, T08 y T01 revisar este paquete y contestar en la tabla
siguiente. Para cada punto, usar `ACCEPT` (con alcance exacto), `REJECT` (con motivo y
alternativa) o `NEEDS_EVIDENCE` (con traza/prueba que falta). Toda respuesta debe incluir
nombre/rol del revisor, fecha, rutas/commits examinados y cualquier límite de la
aprobación. El silencio, una build exitosa o una respuesta parcial no equivale a PASS.

| Revisión | Resultado | Evidencia/razón y alcance aprobado | Revisor/fecha |
|---|---|---|---|
| T00: semántica `input-consumed`; ciclo y orden de eventos; formato canónico local y límite de `step` | IN_PROGRESS — dueño asume revisión; NEEDS_EVIDENCE | Decisiones del dueño para el piloto: requiere lectura SH-4 real (incluida dynarec), coin desde fuentes por slot verificando retorno A0, ciclo `sh4_sched_now64()`+ordinal y step desde quiescencia hasta siguiente VBlank más drain de DMA/callbacks iniciados. La captura P2 de §4.11 corrobora lecturas dynarec sobre los ocho destinos Maple, con ambos loggers capados. Falta moneda activa/retorno, validar drain y completar especificación formal. | Dueño del proyecto (respuesta en sesión), 2026-10-29 |
| T08: checkpoint quiescente; estado de DMA/callbacks/colas; propietario/hilo; captura/restauración atómica de estado + cursor | IN_PROGRESS — dueño asume responsabilidad/revisión técnica | El dueño decide que el checkpoint incluya `mapleInputState[4]` completo + cursor; mantiene frontera quiescente/propietario único/no cross-load sin prueba. Aún debe inspeccionar serializer/restore y validar atomicidad/compatibilidad runtime. | Dueño del proyecto (respuesta en sesión), 2026-10-29 |
| T01: responsable/rutas del recorder-player; manifest/chunks; controles, corpus y criterio de repetición | IN_PROGRESS — scope asignado; implementación bloqueada por T00 | El dueño asigna esta sesión a la subtarea recorder/player una vez fijado el contrato. Rutas de trabajo reservadas: `benchmarks/replay/`, `docs/evidence/` y nuevo launcher acotado bajo `scripts/`; hooks/core solo tras aprobación T00 y coordinación T08. | Dueño del proyecto (respuesta en sesión), 2026-10-29 |
| T05: impacto diagnóstico y prerregistro de escenarios/métricas para comparar backends | PENDING | — | Agente de esta sesión; pendiente de revisión |

### 4.1 Ruta de revisión si el proyecto tiene un solo responsable

No hace falta inventar revisores ni abrir trabajo paralelo para poder decidir el
protocolo. El responsable del proyecto puede registrar decisiones explícitas para
T00/T08/T01 (incluso asumir más de un sombrero), pero debe indicar qué evidencia
revisó y qué límites acepta. Esto **no** convierte una propuesta en aprobación por
silencio ni habilita automáticamente cambios en código compartido.

Para facilitar esa revisión, T05 recomienda como punto de partida —no son decisiones
tomadas ni aprobaciones—:

| Rol | Recomendación para aceptar o corregir | Consecuencia verificable |
|---|---|---|
| T00 | La propuesta inicial de response-delivered queda superada para este piloto: se requiere lectura SH-4/dynarec. El dueño fija coin desde fuentes por slot verificando retorno A0; `sh4_sched_now64()`+ordinal y step VBlank+drain. | El hook autorizado debe unir respuesta Maple, DMA/copia, dirección/valor y lectura SH-4; aún falta evidencia runtime y validar drain. |
| T08 | Capturar solo en un punto quiescente, con propietario único del emulador, sin DMA/callback parcial. Estado completo y cursor forman una operación lógica; `restore` no avanza tiempo. No cargar entre backends por presunción. Para A/B, opción inicial más conservadora: producir un checkpoint en cada backend desde la misma secuencia de preparación y exigir igualdad de anclas/cursor antes de medir el tramo; cross-load es una prueba aparte que primero debe pasar compatibilidad y anclas. | El paquete registra frontera y cursor; prueba smoke compara cada checkpoint contra el otro antes del A/B. Fallo de una ancla bloquea medición, no se “corrige” adelantando inputs. |
| T01 | Asumir explícitamente el diseño y rutas antes de programar. El `TEST_AUTOMATION` existente no es base de aceptación. Empezar por formato/validador y pruebas negativas; después captura/player con input físico bloqueado, EOF íntegro, identidad estricta y primera divergencia. | Tres reproducciones desde procesos nuevos con mismo paquete por backend, anclas iguales y EOF correcto; los casos negativos requeridos fallan explícitamente. |

Las respuestas anteriores son decisiones del dueño sobre dirección/alcance, no
evidencia de ejecución. T00 permanece `NEEDS_EVIDENCE`. Se autoriza únicamente el hook
temporal Debug/laboratorio post-load de `shop_readm`, coordinado con T08, para obtener
la traza requerida; esto no autoriza captura/replay integrado, hooks de producción,
scheduler ni serialización. La observación Frida de §4.3 fue externa y exploratoria.
La asignación T01
autoriza el trabajo en las rutas reservadas una vez fijado el contrato, pero no
levanta la dependencia de T00/T08 ni permite tocar sus subsistemas ahora.

### 4.2 Evidencia mínima pendiente para T00

La revisión estática del código ubica los puntos que una traza debe correlacionar,
pero no los sustituye: `maple_DoDma` consulta el input y llama `RawDma`
(`core/hw/maple/maple_if.cpp`), acumula la respuesta en `mapleDmaOut` y programa
el callback; `maple_schd` copia los bytes al destino guest y eleva la IRQ. El
controlador AW crea la respuesta `MDCF_GetCondition` en
`core/hw/maple/maple_devs.cpp`. No hay en esos puntos prueba de que el SH-4 leyó
después esos bytes.

Con la decisión del dueño, la frontera requerida para este piloto es la lectura SH-4.
La evidencia debe incluir en una sola secuencia correlacionada: ciclo guest y orden; solicitud
Maple (bus/puerto/comando/destino/longitud); respuesta completa generada; finalización
del DMA y bytes efectivamente copiados a RAM; IRQ; y, si se propone como contrato,
el acceso SH-4 al destino (dirección/ancho/valor), incluyendo cómo se observa en
dynarec. Para coin, registrar cada acceso a A0 `0x280` con ancho, ciclo, rama
RAM-test inicial, fuentes de cuatro slots, `coin_chute_time` y valor retornado.
La repetición de esa traza debe ejecutar la transformación de coin y comparar el
valor/ciclo observados, no saltársela inyectando únicamente un byte final.

Las funciones/ubicaciones anteriores son puntos candidatos obtenidos por lectura
estática. §4.3 y §§4.11–4.12 añaden trazas dinámicas parciales; ninguna aporta por sí
sola la correlación completa ni prueba una coin activa transformada.
No se acepta como sustituto el contrato más acotado de respuesta entregada. Hasta
observar la lectura SH-4 (incluida la ruta dynarec usada), el evento no se etiqueta
como `input-consumed` para este piloto.

Comprobación del entorno local (2026-10-29): los dos logs disponibles del piloto
manual no contienen respuesta Maple, bytes DMA ni lecturas de coin correlacionadas.
No había proceso Flycast activo. El GDB encontrado es 11.2 configurado para
`i686-w64-mingw32`, y el Release x64 no tenía PDB. Para habilitar una observación
externa, compilé la configuración Debug ya generada del preset de laboratorio con:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-windows-msvc-lab --config Debug --target flycast -- /m
```

En una revisión posterior (mismo preset, tras autorizar el hook Debug acotado), el
build recompiló `core/rec-x64/rec_x64.cpp` y enlazó con éxito. EXE resultante:
`build-windows-msvc-lab/Debug/flycast.exe`, SHA-256
`1833CC03274DBB0D3862BDFA9C25EC101DE106E72D23D7437694DAF37A719CD7`. Esto solo prueba
compilación/enlace del diagnóstico, no cobertura de loads ni lectura SH-4 dinámica.

El build terminó con MSVC 19.44 x64 y generó `build-windows-msvc-lab/Debug/flycast.exe`
y `flycast.pdb` (127,610,880 bytes), ambos locales/ignorados. Identidades SHA-256:
EXE `EEEE7B1C683A37D68E6057400902C64A0AB3CE6D96A248035244641B40D833C1`, PDB
`D427DEDF07AA6E3BE28CF622DAD96353112C31BBBE1B863528B1610B09534514`. La copia de
EXE usada para la observación coincide byte a byte (mismo hash). Probé el
`OpenDebugAD7.exe` instalado de C/C++ Tools: aborta con `FileNotFoundException` porque
falta `cppvsdbg.ad7Engine.json`; en esa carpeta solo existe `cppdbg.ad7Engine.json`
(GDB). Sin embargo, MSVC DIA SDK sí está instalado, así que pude resolver RVA desde el
PDB y usar Frida como observador externo, sin modificar fuentes ni el ejecutable.

### 4.3 Evidencia dinámica exploratoria (2026-10-29)

Se inició esa copia del mismo Debug x64 con `roms/kofxi.zip`, lectura del BIOS desde
`roms/`, y directorios nuevos bajo
`%LOCALAPPDATA%\Temp\opencode\kofxi-t00-frida-20261029\` para EXE/config/saves/VMU/
savestates. No se copiaron ni escribieron ROM/BIOS/NVRAM personales; tras terminar el
proceso, `roms/` conservaba los mismos tres archivos y timestamps. La instrumentación
Frida solo leía argumentos, retornos y buffers; no modificó entradas ni memoria del
emulador. Evidencia REA de sesión: `ev_87d7a091d17f807f885c5aac898804c4499ea20b61000b9703860e0e32e70859`.

| Observación | Ciclo SH-4 muestreado al salir | Resultado |
|---|---:|---|
| `atomiswave::readMem_A0_006(0x280, 4)` | 43008 | `0x00`: primera rama `!aw_ram_test_skipped`, retorno de RAM-test antes de calcular monedas. |
| Slots 0–3 vía `maple_atomiswave_coin_chute` | 50176 | Los cuatro retornaron `false` (ningún coin switch activo). |
| `atomiswave::readMem_A0_006(0x280, 4)` | 50176 | `0x0f`, consistente con los cuatro slots inactivos en esa lectura normal. |

El ciclo se muestreó externamente en la salida de la función, no en la instrucción exacta
de lectura. Es una observación dirigida de una sola ejecución, no demuestra el debounce
con monedas activas ni una serie de lecturas temporales. Los hooks `maple_DoDma` y
`maple_base::RawDma` se instalaron antes de reanudar el proceso, pero no produjeron
eventos en la captura; por tanto **no hay correlación Maple respuesta→DMA→lectura guest**.
Un sondeo adicional posterior no observó llamadas a `maple_schd` ni a
`jvs_io_board::handle_jvs_message` en su ventana limitada; no permite afirmar ausencia
global de esos caminos. La captura no satisface T00 y no autoriza integrar hooks/core.
El proceso aislado se terminó; Frida fue cerrado.

Repetí desde un segundo proceso scratch con todos los hooks cargados antes de reanudar
el primer ciclo (script SHA-256
`6c40f5ce6bdede03dfd8dbb856d8f7c82c21b41d8d0339d9e8ccdc71fd032b47`; evidencia REA
`ev_2a6362b4d0db1ccbe5fb17abff7980cf13f2e541d853e9c1c5b3f437c355aa5e`). En la ventana
observada, `maple_Init` ocurrió una vez; los contadores de `maple_DoDma`,
`maple_base::RawDma`, `maple_schd` y `jvs_io_board::handle_jvs_message` permanecieron en
cero. Se repitieron solo las dos lecturas de A0 y cuatro consultas de coin de la tabla.
Los cinco errores registrados por Frida fueron llamadas periódicas a
`sh4_sched_now64` desde el hilo timer de JavaScript (access violation a `0x800ff68`),
no desde los hooks de guest; no se usan esas muestras del timer como evidencia de ciclo.
El perfil scratch y la ROM/BIOS conservaron la separación descrita; se terminó el
proceso y cerró Frida.

Esto acota las dos primeras ventanas de arranque observadas: la ruta de coin A0 sí se
ejecutó, pero en ellas no se vio consumo Maple ni mensajes JVS. No prueba que esos
caminos no sean usados en otra escena ni identifica por sí solo el endpoint real de
todos los controles; evita asumir que una traza Maple es pertinente a este tramo.

Una tercera ejecución aislada añadió lectura de datos globales resueltos por DIA desde
el mismo PDB: `mapleInputState` (RVA `0x03483AD0`),
`atomiswave::aw_ram_test_skipped` (RVA `0x03486EF0`) y
`atomiswave::coin_chute_time` (RVA `0x03486EF8`). Evidencia REA:
`ev_21a85a3a764d3e6224ce5a475a5dce0f5cce354279c7bb25ae9f02966e013725`. En las dos
lecturas A0, slot 0 tenía `kcode=0xffffffff` (sin botones activos) y los cuatro
`coin_chute_time` estaban a cero; la rama cambió `aw_ram_test_skipped` a 1 tras
devolver el primer `0x00`. Se posteó Q down/up (`DC_BTN_D`, mapping default del
teclado) a la ventana aislada después del arranque, pero no hubo otra lectura A0 ni
llamada Maple/JVS observada; por tanto la acción host **no demuestra input consumido**
y no se reporta como coin activo. No se modificó RAM del emulador desde Frida.

La vía de observación externa queda técnicamente habilitada con DIA+Frida, pero T00
sigue `NEEDS_EVIDENCE`: falta una moneda activa realmente consumida y su transformación
temporal, además de una decisión respaldada por evidencia sobre la ruta de input
aplicable (o una sesión que genere tráfico Maple y lo correlacione con DMA/lectura
guest). No se midió overhead del observador.

### Observación posterior: aviso de backup y progreso del SH-4

En la instancia interactiva scratch del 2026-10-29, el usuario informó que seguía
viendo `backup data was cleared` y que interpreta correctamente el aviso como parte de
la inicialización aún incompleta/fallida, no como confirmación de que ya se pueda
insertar una ficha. Sin enviar más teclas, se añadió un muestreador externo al retorno
de `sh4_sched_now64` y se mantuvieron los hooks de A0/coin/Maple. Evidencia REA de la
sesión: `ev_00c3dbfb2eb44ac12bab99ffb0f4c0a6e4eec68853574c7f2d7dc4f7f29c1a8c` (mismo
proceso PID 17768; instrumento con PDB del build Debug x64 MSVC).

La sesión sí mostró actividad adicional: el ciclo SH-4 avanzó durante las muestras,
hubo 2.501 entradas observadas a `maple_DoDma` y respuestas `RawDma` a consultas de
dispositivo/control (los buffers capturados incluyen comandos `0x01` y `0x09`). En la
ventana el slot 0 siguió con `kcode=0xffffffff`, `coin_chute_time[0..3]=0` y las
consultas de coin retornaron `false`; A0 dejó de ser consultada después de las primeras
lecturas. Esto acredita progreso de ejecución y tráfico/respuestas Maple, **no** que la
inicialización del juego haya terminado ni que la imagen haya dejado el aviso. Tampoco
correlaciona aún una respuesta DMA completa con su escritura guest y lectura SH-4; el
buffer fue limitado a 64 bytes por mensaje. No se envió input, no se modificó RAM y no
se midió overhead. Por instrucción del usuario no se intenta coin mientras permanezca
ese aviso; T00 y el gate de T01 continúan pendientes.

La observación posterior del mismo proceso (`ev_aca5658bf6adcacfe16d49573c9572bbee54946dbf765992b84d0e39ff9d800b`)
contabilizó 3.713 entradas Maple DMA en total; el ciclo asociado a las últimas entradas
capturadas alcanzó `14,054,513,984`. El conteo es acumulado del hook, no una medición de
frames, tasa ni duración; siguen sin aparecer coin activos o datos para la correlación
de entrega con lectura guest.

En la consulta más reciente del mismo proceso (`ev_c5a44cf71862540c907f66126bf268da70a75a83d84bf25a42da94a80d4b8e59`), el total acumulado pasó a 6.485 entradas Maple DMA y el ciclo de la última captura fue `23,302,945,792`. Las últimas consultas coin disponibles seguían devolviendo `false` con `kcode=0xffffffff` y timers en cero. No se interpreta el contador acumulado como duración/frames ni como evidencia de inicialización completada.

Para reducir el tráfico del observador, descargué los hooks anteriores que enviaban un
mensaje por cada DMA y cada consulta coin, sin cerrar Flycast. Un monitor Frida de menor
volumen (script SHA-256
`ed055eab063ce861b40ac99ef57b4f97f7bf9418f62f1a840aa811aac44fcfe4`) quedó activo en
el mismo PID 17768: reporta cambios de `kcode`/timers en la frontera de Maple, A0 solo
si devuelve un valor distinto de `0x0f`, coin solo si retorna activo y un hito por cada
1.000 DMA. La evidencia de instalación y primera muestra es
`ev_3456d0421e3b8aa08594ca8b132c7ef2439e094ad5f932c133ad170c0e33e513`: al instalarse,
`kcode=0xffffffff` y los cuatro timers seguían a cero. Este filtro reduce volumen de
mensajes, pero no se presenta como una medición validada de overhead ni reemplaza la
captura completa requerida por T00.

Tras continuar el monitor de baja salida, la evidencia REA
`ev_71d055cf38473cd807d43296330750bf22db257fe198dd569e519c63a85e16a7` registró el
hito local `dma=1000` en ciclo `36,354,844,288`; el proceso PID 17768 seguía activo y
sin errores de instrumentación. No se generaron eventos `aw280_nonidle` ni `coin_active`
en esta ventana. Este monitor solo informa estados/eventos seleccionados y no convierte
su ausencia en prueba global de que no ocurra otro tipo de input o endpoint.

La siguiente consulta (`ev_62bd2116ba1692b9294cc0bdd7b1e204b429445120e1fd35d1fa7f688bcc61ae`)
registró el hito `dma=2000`, ciclo `39,691,219,456`; tampoco reportó coin activo, A0
no-idle ni errores Frida.

La sesión posterior (`ev_bc0e7cad9fed5c194c005f00008cba3d9d2f3951754ef47e1f42c43c0b0cea5f`)
alcanzó `dma=4000`, ciclo `46,363,969,344`, sin cambios de `kcode`/timers, A0 no-idle,
coin activo ni errores dentro del alcance del monitor.

La consulta actual (`ev_f2107fea232e9aff5365050cea9ef2b9d4ce544e12caf4f251f0378ad7a34d02`)
alcanzó `dma=5000`, ciclo `49,700,344,960`, con las mismas limitaciones: sin cambios de
`kcode`/timers ni eventos A0 no-idle/coin activo observados por este monitor; sin errores.

### Coin activo observado tras finalizar el arranque

Una captura de ventana del perfil scratch mostró `INSERT COIN(S)` y `CREDITS 0`
(PNG temporal SHA-256
`51949F72D31065E207EB5B55EC15F1DF77791437D388960192345EC0FE08BB6A`). El usuario
indicó que pulsaría Q una vez; el monitor read-only existente registró posteriormente
el camino guest activo (`ev_f5a28215b8eac750b5739aab690562f979c9e49ce7e3006d65e107a59ff8decb`):
`mapleInputState[0].kcode=0xfffff7ff` (bit 11 de `DC_BTN_D` activo bajo; `gamepad.h`
define `DC_BTN_D=1<<11` y `keyboard_device.h` mapea Q a Coin),
`maple_atomiswave_coin_chute(0)` retornó true, y la lectura A0 `0x280` devolvió
`0x0e` (bit 0 activo bajo para slot 0) en ciclo `84,575,512,448`, con
`coin_chute_time[0]` fijado a ese mismo ciclo. Otras lecturas consecutivas devolvieron
`0x0e` mientras seguía activa la ventana temporal de la lógica original. La fuente
(`core/hw/naomi/atomiswave.cpp`) establece el timer al primer muestreo activo, permite
la moneda por menos de `SH4_MAIN_CLOCK/10`, y limpia el timer cuando el switch deja de
estar activo. Esta es evidencia dinámica de una entrada Maple observada a nivel de
`kcode`, su consulta coin y el byte A0 transformado; no es una intervención sobre RAM.

Una captura posterior de ventana mostró `PRESS START` y `CREDITS 3` (PNG temporal
SHA-256 `DF1CF88F4E3E3453F4D88A76C157D4B8EA4BB26372457A9E3A6AB1CF55EF2A91`). Aunque
el usuario pretendía una sola pulsación Q, el resultado visible fue tres créditos y
el monitor registró varios intervalos activos separados; no se atribuye la diferencia
a auto-repeat, tres key-downs ni a una regla de una pulsación=un crédito sin una traza
de eventos host/guest más fina. Hasta esa captura no se había enviado Start. Esto satisface la evidencia antes
faltante de coin activo/transformación temporal en esa ejecución, pero T00 aún requiere
decisión formal de semántica/orden/límite y falta correlación de respuestas Maple
completas con su entrega a RAM/lectura guest, además de revisión T08. No declarar T00
aprobado ni iniciar recorder/player/B-D.

Después, el usuario autorizó pulsar Start una vez. El monitor vio `kcode=0xfffffff7`
en Maple en ciclo `129,202,824,128` y el retorno a `0xffffffff` en
`129,226,178,816`; esto corresponde al bit 3 (`DC_BTN_START`) según
`core/input/gamepad.h`. La captura posterior mostró la pantalla de selección de
personajes con `CREDITS 3` (PNG temporal SHA-256
`CC05424AFAFDF1CD866A9CECF9204F98EB13F3FFCC714868A12DE168C0242511`). Evidencia REA
posterior: `ev_d747d148e0d085c4e349cb51b8a07038b6bad6b0209ed379b2621a5afb2772be`. En
esa captura aún no había comenzado el combate.

### Selección y tramo natural exploratorio

El usuario autorizó seleccionar y jugar un tramo breve. El hook low-volume de entrada
a `maple_DoDma` observó 245 cambios de `kcode` entre ciclos
`151,773,401,024` y `158,145,877,568` (evidencia REA
`ev_5fb692cf04e4e4562e15a2597df28d3191396a78e0a989a8ef15dc84dcb0ed2b`). Según
`SH4_MAIN_CLOCK=200 MHz`, el delta corresponde a ~31.86 segundos de ciclos SH-4, no a
una medida de tiempo de pared ni a un segmento aislado de combate: el intervalo incluye
selección/entrada a ronda. **Límite de la frontera**: el hook era `onEnter`, y el cuerpo
de `maple_DoDma` actualiza input en su primera instrucción relevante (`ggpo::getInput`);
por ello, esa lectura global puede ser el valor anterior a la muestra de ese mismo DMA y
no se usa como registro exacto de cada poll actual. Se observaron máscaras activas de
Start/A/B/X/Y y direcciones, todas activas-bajo; no se capturaron ejes analógicos,
respuesta RawDma completa ni muestras en cada tick. No hubo coin activo/A0 no-idle
adicional en este intervalo; no se registraron errores Frida.

La captura visual posterior muestra escenario `STAGE 01`, timer `96`, score `14800` y
`CREDITS 2` (PNG temporal SHA-256
`B696045404FB74BE2BEAE26CC68860575DA63D0B60248359C004D4E47EBB2EE1`). Es una sesión
natural exploratoria con input cambiado, no un paquete de replay ni aceptación P1: no
hay checkpoint/cursor, entradas completas por tick ni reproducción desde proceso nuevo.

Para corregir el límite del hook de entrada, añadí un segundo observador de solo lectura
en `onLeave` de `maple_DoDma`, posterior a la consulta de input y a la construcción de
respuestas síncronas (script SHA-256
`6ff49fad8fe2e0bf96b4d6a5bf59b4db31c336aeadc07c022e73888269765c44`). Evidencia REA
inicial `ev_c6e6cb09dcf70a3b294bd087efea14d304fb913e61b328db8d1cbe7d20721b60` observó
cambios de máscara, incluyendo `RIGHT`, `DOWN`, `DOWN+LEFT`, `LEFT+B`, `LEFT+A` y
`LEFT+X`, en ciclos posteriores a la captura anterior. Es una muestra post-call más
apropiada de `mapleInputState`, pero todavía no captura bytes/entrega DMA ni el momento
de lectura del SH-4; no sustituye T00 ni es un recorder/replay aceptado.

La continuación del mismo intervalo de juego elevó a 737 los cambios `kcode` observados
por el hook de entrada desde ciclo `151,773,401,024` hasta `172,392,198,720` (REA
`ev_a14c3a195a6683da9a2a4a9d448df299713d124dd075879becc4b8c8f08a24b9`), delta
`20,618,797,696` ciclos = ~103.09 s SH-4, incluyendo selección/carga y ronda. La muestra
post-call llegó a 170 cambios entre `168,545,358,464` y `172,388,862,464` (REA
`ev_a8c7dba1103f37022acc6f2f7a07d5207bfa0795ccea3099c38a54bc2636d17a`), con máscaras
direccionales y de botones y sin errores Frida. Una captura de ventana en el mismo tramo
mostró `STAGE 01`, timer `47`, score `200100` y `CREDITS 2` (PNG temporal SHA-256
`2D8BB2CD0451712355219ADDA93968C49A0457E5C24AC62EC17647B1738526AE`). Los intervalos
en ciclos no miden tiempo de pared ni aíslan por sí solos el combate. Tras recopilar
esta muestra, descargué el hook de entrada/A0/coin de alta frecuencia; queda activo solo
el observador post-DMA de cambios de `kcode`. Ninguna de estas capturas incluye checkpoint,
cursor, respuesta DMA completa o replay desde proceso nuevo.

La observación post-DMA posterior (`ev_7bfa6fdafc8eba65fc4297a323e7d7bf277cdcf5d24a5aaacbcba4ee2c1627b1`)
acumuló 383 cambios de máscara en 3.388 Maple DMA, hasta ciclo `179,845,660,736`;
desde la primera muestra post-call en `168,545,358,464` son ~56.50 s de ciclos SH-4,
no tiempo de pared. La imagen del mismo proceso mostró `WINNER P1`, `STAGE 01`, timer
`27`, score `292000` y `CREDITS 2` (PNG temporal SHA-256
`188A2EE7DD337D09FEA61DC7F3452FF08C5CFFBAF354AC7D89E77DEC0298AF4A`). Es evidencia de
un tramo natural que alcanzó resultado; sigue siendo solo un log parcial de máscaras
cambiantes, no una secuencia input-tick completa ni un replay verificable.

### Respuesta RawDma y límite de entrega a RAM

La sonda de `RawDma`/`maple_schd` (`ev_cabe8fbf7e800771ebf3508e54d65e4225a8f51a0bfb25885582a8cec4e9bebb`)
capturó dos respuestas de controller de 16 bytes en el ciclo
`193,344,633,664`: puerto 0 `[08 00 20 03 00 00 00 01 FF FF 80 80 80 80 80 80]` y
puerto 1 `[08 40 60 03 00 00 00 01 FF FF 80 80 80 80 80 80]`. Se repite el par en
ciclos siguientes. El callback `maple_schd` se observó por primera vez en
`193,344,733,568`, 99,904 ciclos después. El código fuente (`maple_if.cpp:315-339`)
confirma que, si DMA está habilitado y el par de destino no es cero, el callback obtiene
el puntero con `GetMemPtr`, copia el vector de respuesta mediante `memcpy` y eleva
`holly_MAPLE_DMA`; un destino cero causa overrun. Sin embargo, la sonda runtime no
capturó la dirección/destino de ese `memcpy`, su retorno ni una lectura SH-4 posterior.
Así que la evidencia dinámica correlaciona generación RawDma y callback por tiempo;
la afirmación de escritura a RAM se basa solo en el flujo estático condicional, y no
prueba qué rama se tomó ni que el guest leyó el buffer. No usar esto como confirmación
de `input-consumed` ni como cierre de T00.

La consulta actual de la página pública de GitHub para el issue #1 confirma que sigue
abierto y muestra “No one assigned”, sin actividad/comentarios ni revisiones. Su cuerpo
dice que T00/T01/T08 siguen sin asignar; para T01 esto está desactualizado respecto del
backlog local, que sí asigna a esta sesión la subtarea recorder/player, condicionada a
T00/T08. El issue no contiene aprobación ni reemplaza el backlog como autoridad de
estado. `gh` no está instalado en este entorno, por lo que no pude consultar metadatos
de revisión mediante CLI; esta comprobación se limita a la vista pública de la página.

Al retomar esta iteración, la sesión Frida anterior figura `detached` y el PID 17768 ya
no está presente en `Get-Process`; por ello no es posible ampliar la traza del mismo
proceso ni confirmar su estado final. No se infiere por qué terminó. Una futura prueba
de copia/lectura requiere arrancar un proceso nuevo y obtener autorización/estado de
ejecución, además de instrumentar el destino guest de `maple_schd`.

### Respuesta que habilita el siguiente paso

Solo cuando T00 y T08 respondan explícitamente, cualquier rechazo quede resuelto y
T01 confirme responsable/rutas bajo el backlog, se puede redactar/implementar un
contrato P0/E1 conforme a la decisión registrada. La edición de especificaciones,
hooks del core o código compartido requiere coordinación en sus rutas y revisión de sus
responsables; esta solicitud no constituye esa aprobación. Después se diseña e
implementa el recorder/player conforme al contrato autorizado. Hasta entonces siguen
bloqueados la captura como replay aceptado, el ensayo de stage A/B y cualquier
afirmación de determinismo/aceptación G3.

## 6. Plan T01 y pruebas de aceptación (borrador condicionado)

Este plan aterriza P1/G1 en entregables comprobables. El dueño asignó a esta sesión
la subtarea recorder/player y reservó las rutas indicadas en el backlog; la asignación
no aprueba un contrato ni autoriza hooks compartidos. La implementación de captura,
restore y replay queda condicionada a resolver T00 y coordinar T08. Si T00/T08 eligen
otra semántica, se actualiza antes de codificar y se registra en su ruta autoritativa.

### Orden de trabajo y gates

| Fase | Trabajo de T01 | Salida/gate para avanzar |
|---|---|---|
| 0 — tomar la tarea | Responsable/subtarea/rutas ya registradas en backlog; revisar issue #1; falta fijar con T00/T08 el contrato y con T05 el build/identidad del laboratorio. | Hook temporal Debug/lab de lectura SH-4 y logging de destino DMA están implementados y compilan; la primera ejecución scratch no emitió eventos DMA. T00 sigue NEEDS_EVIDENCE y T08 requiere validación concreta: recorder/player **BLOCKED**. |
| 1 — especificar paquete | Congelar versión del manifest, orden/anchura/endianess de eventos, ciclo/contador, cursor, vida de buffers, checksum/chunk, marcador final y comportamiento de error. Definir qué puede omitirse y cómo se compara cada ancla. | Esquema revisado contra S2/S3/S5; identidad y exclusiones explícitas; ninguna ambigüedad del contrato abierta. |
| 2 — validador puro | Implementar `check`/`info` y pruebas sintéticas del manifest/chunks, antes del acceso al core. Probar longitudes, overflow, versión, checksum, orden y finalización. | Casos válidos aceptados; negativos rechazados con código/mensaje estable y sin cargar estados. |
| 3 — captura/replay mínimo | Solo por interfaces aprobadas: captura atómica de estado completo + cursor; registrar eventos pactados de Maple/coin; chunks con flush y cierre íntegro. Player restaura en proceso nuevo, bloquea input físico de juego, conserva cancelación host separada y detiene en la primera divergencia. | Pruebas de integración reproducen los mismos eventos/estado en smoke corto; overhead medido por separado. |
| 4 — smoke del recorder | Recorrido natural de 30–60 s, sin writes para fabricar casos; reproducir 3 veces desde procesos nuevos con el mismo EXE/config/backend y paquete. | Cero diferencias de evento/anclas dentro de la cobertura; identidad, duración y EOF completos. Es solo smoke; no satisface por sí solo P1/G1. |
| 5 — piloto P1/G1 | Corpus acordado y piloto de 5 min, 3 replays desde procesos nuevos; cubrir inputs pertinentes y fases seleccionadas; comparar anclas por subsistema y medir ruido/overhead. Ejecutar controles de input y estado mutados fuera de corridas normales. | Replays repetibles, anclas verificadas, negativos detectados y ruido cuantificado. Primera divergencia sin explicar = **FAIL**, corregir baseline; no reducir estado para esconderla. |
| 6 — handoff | Entregar informe, comandos, identidad, hashes, métricas, cobertura y límites en plantilla de evidencia; dejar estados/inputs/anclas completos en almacenamiento local ignorado. | T01 puede proponer PASS de su piloto; no es PASS de G2/G3. G2 requiere por separado 10.000 restauraciones y x86-64/AArch64; stage A/B T05 comienza solo con replay compartido verificable. |

### Matriz mínima de aceptación de T01

| ID | Prueba | PASS | FAIL / BLOCKED |
|---|---|---|---|
| T01-I01 | Identidad del paquete | Hashes/identidad de EXE y commit, ROM/BIOS por hash, configuración/región/idioma/backend, estado inicial/NVRAM por hash y entorno quedan fijados; player valida todo antes de restaurar. | Campo obligatorio ausente o mismatch no detiene la reproducción: **FAIL**. Falta de un artefacto legítimo/local: **BLOCKED**, no se sustituye silenciosamente. |
| T01-I02 | Estado inicial + cursor | Un checkpoint aprobado restaura estado completo y cursor juntos; `restore` no avanza el tiempo; misma build/plataforma salvo decisión distinta explícita. | Captura solo RAM, cursor separado/incierto, callback o fuente externa sin cubrir: **FAIL/BLOCKED** según evidencia. |
| T01-I03 | Eventos Maple y coin | Se registran únicamente eventos y fronteras aprobadas por T00/T08; ciclo guest + orden, bus/puerto/operación/bytes y estados especiales aplicables se conservan; no se confunde `GetInput` con consumo ni respuesta entregada con lectura SH-4. | Eventos colapsados/reordenados, coin RAM-test omitido o semántica no aprobada: **FAIL/BLOCKED**. |
| T01-I04 | Integridad incremental | Chunks versionados, tamaños acotados, contador, checksum por chunk/global, flush y marcador de final; un corte recupera solo el prefijo válido. | Truncado/corrupto presentado como sesión completa, overflow o checksum ignorado: **FAIL**. |
| T01-I05 | Replay fiel | Desde proceso nuevo se restauran estado/cursor, se bloquea el input físico de juego, se reproducen valores/ciclos acordados y se paran los eventos/anclas en la primera diferencia. EOF completo y control de cancelación independiente. | Entrada host se filtra, se alinea el ciclo para ocultar diferencia, el replay continúa tras reset/loadstate/EOF ambiguo: **FAIL**. |
| T01-I06 | Anclas y diagnóstico | Anclas definidas por subsistema en frontera y bloques; primer fallo informa secuencia/ciclo/tick o poll, subsistema y bytes/campos; punteros/padding excluidos solo con justificación. | Solo hash de actor/RAM, exclusión sin justificación o falta de primer fallo: **FAIL**. |
| T01-I07 | Repetición positiva | Smoke natural 30–60 s ×3 desde procesos nuevos y, después, P1 5 min ×3; mismo paquete/build/backend, identidad/inputs/EOF/anclas completos y cero divergencias inexplicadas. | Un mismatch, run incompleto o escena/input no cubierto se informa; no se promedian divergencias ni se marca PASS por mayoría. |
| T01-I08 | Negativos | Cada mutación aislada de input omitido/alterado, checksum/chunk, truncado sin marcador final, identidad, cursor/continuidad y ancla/estado incorrectos falla explícitamente. | Cualquier negativo aceptado o no detectado = **FAIL**; mutaciones no ejecutadas = **BLOCKED**, nunca cero fallos. |
| T01-I09 | Overhead y evidencia | Capture/restore/hash/resim y CPU/memoria medidos con/sin harness; comandos, commits, hashes, cobertura, resultados negativos y límites según `docs/evidence/TEMPLATE.md`. | Sin control de overhead/ruido o protocolo reproducible: no promover a baseline G1 ni usar para afirmar ahorro G3. |

### Artefactos, seguridad y límite de aprobación

Versionar solo código/esquema, pruebas sintéticas y resúmenes agregados revisados.
`initial.state`, inputs con estado, anclas completas, ROM/BIOS, NVRAM runtime y trazas
grandes permanecen locales e ignorados por Git; nunca ampliar la excepción del fixture
NVRAM aprobado. No incluir credenciales ni secretos en manifests/logs. No incluir datos
de prueba en el issue sin revisar licencia y privacidad.

Umbral de overhead propuesto por el plan de recursos: diagnóstico ≤2 % CPU/frame; medirlo
y no darlo por alcanzado. No hay presupuesto absoluto G1 aprobado todavía. La puerta T01
solo acepta que el laboratorio reproduce el paquete en el dominio probado; no sustituye
el contrato T00/T08, las 10.000 restauraciones G2 ni los A/B G3 con escenario común.
Hasta resolver T00, coordinar T08 y congelar el contrato, esta sección continúa siendo
un checklist de diseño; la asignación T01 no cambia ese gate.

### 4.4 Ejecución scratch para descubrir destino Maple (2026-10-29)

- Autorización del dueño: usar la ROM local KOF XI en scratch aislado y añadir logging
  Debug mínimo del destino/longitud en la copia Maple; no importar saves ni tocar
  scheduler/serialización. Scratch: `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-trace`.
- Build: preset `windows-msvc-lab`, Debug, MSVC; `rec_x64.cpp` y `maple_if.cpp`
  recompilaron, enlace exitoso. EXE scratch coincide con build por SHA-256
  `508D90E72A791F9BDFE75BB2008CDE0E0581100001D4CEAAFBBBAE3921BFE4A1`. ROM usada:
  `roms/kofxi.zip`, SHA-256 `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`.
- Comando efectivo: EXE Debug copiado a scratch; argumentos `-config "log:LogToFile=yes"`
  `-config "network:GGPO=no"` `-config
  "config:UseReios=no,Dreamcast.AutoLoadState=no,Dreamcast.AutoSaveState=no"` y ROM
  absoluta. Working directory = scratch; no se configuró rango `KOFXI_SH4_READ_TRACE_*`.
- Proceso PID 22804 se ejecutó 25 s, se cerró mediante `CloseMainWindow` y terminó exit
  code 0. El log `flycast.log` registra apertura de ROM/BIOS, 120 VBlanks en los
  intervalos observados y creación de NVRAM nueva dentro de `scratch/data/`
  (`kofxi.zip.nvmem`, 131072 bytes; `kofxi.zip.nvmem2`, 65536 bytes); no se copió ni
  cargó una NVRAM personal. El entorno scratch permanece local, no versionado.
- Resultado: cero líneas `KOFXI_MAPLE_DMA`, cero `KOFXI_SH4_READ`; no se identificó
  destino Maple ni se observó callback con payload. La falta de líneas no prueba que
  el juego no leyera inputs: solo indica que el callback instrumentado no pasó por la
  copia observada durante ese tramo. `now64` tampoco se probó como ciclo exacto por
  instrucción. T00 sigue NEEDS_EVIDENCE; no hay replay/checkpoint ni comparación B/D.
- Siguiente paso concreto: requiere que el dueño avance el juego hasta generar DMA
  Maple/lecturas de controller (o autorice otra técnica observacional apropiada), luego
  ejecutar con `KOFXI_SH4_READ_TRACE_START/END` ajustado al destino observado. No se
  reutilizará el scratch NVRAM en B/D ni se iniciará otro proceso sin las mismas
  salvaguardas.

### 4.5 Launcher scratch para la siguiente captura

Se añadió `scripts/run-kofxi-sh4-trace.ps1` para no repetir a mano la configuración de
aislamiento. Requiere `-RomPath`, copia el EXE Debug validado a un subdirectorio nuevo
bajo `%LOCALAPPDATA%\Temp\opencode\kofxi-sh4-traces`, deja que Flycast cree un `data/`
limpio junto al EXE, bloquea una segunda instancia, desactiva GGPO y autoload/autosave
de savestate, fija el hash de EXE/ROM en `run.json` y cierra con `CloseMainWindow` (no
fuerza terminación). Duración por defecto: 30 s; `-ReadStart/-ReadEnd` opcionales
configuran el filtro post-load, con validación `uint32` y rango creciente. Si el proceso
no cierra normalmente, informa PID y no lo mata.

Se probó `-DryRun` y la validación negativa de rango incompleto, ambos sin crear scratch
ni iniciar proceso; también se probó `-DryRun` con un rango hexadecimal válido. Una
corrida real y sus límites se registran a continuación; el `-DryRun` no constituye
cobertura funcional del hook.

### 4.6 Corrida scratch con fixture NVRAM individual (2026-10-29)

- El launcher se amplió con `-NvmemFixture`, aceptando únicamente el fixture aprobado
  `tests/fixtures/kofxi/unlocked-spanish.nvmem` por tamaño y SHA-256
  `F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA`. El fixture se
  copió solo a `scratch/data/kofxi.zip.nvmem`; el `kofxi.zip.nvmem2` generado por
  Flycast durante la corrida (65.536 bytes) es un artefacto scratch nuevo, no el segundo
  archivo correspondiente del save del usuario. No se usó ni se buscó otro save.
- Corrida real: PID 25668, 180 s, salida normal exit 0; scratch
  `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-traces\20261029-183222-221`.
  `run.json` registra EXE SHA-256
  `508D90E72A791F9BDFE75BB2008CDE0E0581100001D4CEAAFBBBAE3921BFE4A1`, ROM
  `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`, fixture hash y
  argumento combinado `log:LogToFile=yes,Verbosity=4`. El log alcanzó 182.694.032 bytes
  y conservó mensajes `D[HOLLY]`. La inspección de `parseConfigOption()` en
  `core/cfg/cl.cpp` mostró que la coma reinicia la sección y que cada opción debe
  volver a incluir `log:`; por tanto el argumento combinado no establecía
  `log:Verbosity=4`. Se corrigió el launcher para pasar `-config "log:LogToFile=yes"`
   y `-config "log:Verbosity=4"` por separado. En ese momento aún faltaba verificar el
   efecto; la verificación corta posterior consta en §4.7.
- El log contiene cuatro hitos `AW runtime: reason=interval`, tres `Erase Sector`, y
  cero coincidencias de `KOFXI_MAPLE_DMA` y `KOFXI_SH4_READ`. Esto prueba actividad
  limitada del runtime, no que el guest llegara a gameplay o leyera controles. El
  usuario reportó que persistía el aviso “backup data was cleared”; no se interpreta
  como juego listo ni como verificación de un save completo. La corrida no incluyó el
  `.nvmem2` emparejado y no satisface T00.
- El fixture individual, por tanto, no resolvió el arranque observado. El siguiente
  intento que requiera el par depende de que el dueño proporcione y autorice
  explícitamente el segundo archivo para uso exclusivo en scratch. No copiar el
  `.nvmem2` generado por esta corrida como si fuera el original ni llevar ninguna
  NVRAM runtime al repositorio.

### 4.7 Verificación corta de Verbosity (2026-10-29)

- Tras corregir la sintaxis, se ejecutó de nuevo el launcher por 25 s con el mismo
  fixture autorizado individual, en scratch nuevo
  `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-traces\20261029-184117-225`.
  PID 756 terminó normalmente con exit code 0. `run.json` contiene las opciones separadas
  `-config "log:LogToFile=yes" -config "log:Verbosity=4"`.
- El log mide 10.498 bytes y contiene cero líneas `D[HOLLY]`, verificando que el cambio
  sí filtra el spam de debug observado antes. Registra dos hitos `AW runtime`, pero cero
  `KOFXI_MAPLE_DMA` y cero `KOFXI_SH4_READ`; la ventana breve no fue un recorrido
  interactivo y no añade evidencia T00 ni de llegada a gameplay.
- Flycast creó de nuevo un `kofxi.zip.nvmem2` de 65.536 bytes en ese scratch; no se usó
  como parte del save. El único archivo importado fue el fixture aprobado de 131.072
  bytes, hash especificado en §4.6. Esta corrida solo verifica logging/launcher.

### 4.8 Corrida interactiva: destinos DMA Maple (2026-10-29)

- El dueño autorizó repetir la corrida con la pareja de scratch compuesta por el fixture
  aprobado como `kofxi.zip.nvmem` y el `kofxi.zip.nvmem2` que Flycast crea
  automáticamente, sin sobrescribir ni cruzar los dos archivos. Esto no se considera
  el par original del usuario. El launcher cerró normalmente: PID 3056, 180 s, exit 0;
  scratch `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-traces\20261029-190636-147`.
- `run.json` fija EXE SHA-256
  `508D90E72A791F9BDFE75BB2008CDE0E0581100001D4CEAAFBBBAE3921BFE4A1`, ROM SHA-256
  `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44` y fixture
  SHA-256 `F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA`. El log
  tiene 1.191.030 bytes, cero `D[HOLLY]`, 36 hitos `AW runtime`, 5 `Erase Sector` y
  ningún texto `backup data was cleared` durante esta ventana.
- El hook registró 10.000 `KOFXI_MAPLE_DMA` (ordinal 0–9999, alcanzó su límite; por lo
  tanto el total real puede ser mayor), desde ciclo `1.653.731.520` hasta
  `10.007.591.104`. Ocho destinos observados: `0x0c374200` (una respuesta de 116 B y
  1.249 de 16 B), `0x0c375a00` (igual), `0x0c377200` (1.250 × 4 B), `0x0c378a00`
  (1.250 × 4 B), `0x0c37a200` (1.250 × 16 B), `0x0c37ba00` (1.250 × 16 B),
  `0x0c37d200` (1.250 × 4 B) y `0x0c37ea00` (1.250 × 4 B). Esto establece destinos
  de copia Maple observados en este recorrido; no demuestra todavía qué rangos leyó el
  SH-4 ni que la escena fuera gameplay.
- Esta corrida no configuró `KOFXI_SH4_READ_TRACE_START/END`; por eso cero
  `KOFXI_SH4_READ` es **no observado**, no una lectura negativa del guest. Las capturas
  filtradas posteriores se registran en §§4.9–4.11. El EXE
  registró el fixture permitido antes del run, pero Flycast modificó el `.nvmem` en
  runtime: hash final `18F6F266C8AC3863FF3F570A9BEFD6F27CEDD9DC0EEFE829D61FCDE1F43DE5F8`;
  el `.nvmem2` autogenerado termina en
  `404E49D7CA24501E0D786C0DC9CC99B5DE6149D03CB387617D560153CD3BAD5C`. Ambos siguen en
  scratch y no se copiarán a Git. T00 continúa abierto hasta completar coin/step/drain
  y revisión formal.

### 4.9 Primer filtro SH-4 sobre dirección física (2026-10-29)

- Se ejecutó en scratch el mismo launcher con filtro exclusivo
  `[0x0c374200, 0x0c37ea04)`, correspondiente a las direcciones que registra el hook
  después de la copia DMA. PID 28436 terminó exit 0 tras 180 s; el log midió 1.191.019
  bytes, `D[HOLLY]=0`, `KOFXI_MAPLE_DMA=10.000` (límite), `KOFXI_SH4_READ=0`, 36 hitos
  AW runtime y cinco `Erase Sector`.
- El log confirma `Using Recompiler`, pero el filtro del hook compara la EA SH-4 antes
  de la traducción MMU. La corrida no demuestra que el guest no leyera los buffers: pudo
  acceder por un alias P1/P2 (`0x8c...`/`0xac...`) distinto de la dirección física
  (`0x0c...`) que se filtró. La causa del cero aún no está determinada. La prueba P1 se
  documenta en §4.10; al no producir lecturas, se evaluará el alias P2 antes de concluir
  falta de consumo.

### 4.10 Filtro SH-4 sobre alias P1 (2026-10-29)

- Corrida scratch PID 26448, 180 s, exit 0, filtro `[0x8c374200, 0x8c37ea04)`. El log
  midió 1.190.775 bytes, cero `D[HOLLY]`, 10.000 `KOFXI_MAPLE_DMA` (cap), cero
  `KOFXI_SH4_READ`, 36 hitos AW runtime y cinco `Erase Sector`.
- Este cero descarta únicamente eventos que el hook implementado habría emitido dentro
  del intervalo P1; no demuestra que el guest no consultara los datos, ni que haya
  gameplay. El alias P2 se analiza en §4.11.

### 4.11 Captura SH-4 por alias P2 y correlación de rangos (2026-10-29)

- Corrida scratch PID 28036, 180 s, exit 0, intervalo `[0xac374200, 0xac37ea04)`;
  log 2.599.910 bytes, cero `D[HOLLY]`, 10.000 eventos Maple (límite), 10.000 lecturas
  SH-4 (también límite), 36 hitos AW runtime y cinco `Erase Sector`. El log confirma
  `Using Recompiler`. No se registró `backup data was cleared` en la ventana. El filtro
  capó ambos streams, así que las frecuencias/cobertura completas no se conocen.
- Correlación offline de los registros disponibles, canonicalizando cada EA con
  `addr & 0x1fffffff` y comparando intervalos de acceso `[EA, EA+width)` con los spans
  Maple observados, encontró 6.274 lecturas SH-4 con solapamiento de dirección. Hay
  lecturas en los ocho destinos: 1.419 en cada uno de `0x0c374200` y `0x0c375a00`,
  214 en cada uno de `0x0c377200` y `0x0c378a00`, 1.292 en cada uno de `0x0c37a200` y
  `0x0c37ba00`, y 212 en cada uno de `0x0c37d200` y `0x0c37ea00`. Son conteos del
  prefijo capturado, no proporciones del tráfico completo. Coincidencia temporal directa:
  primer DMA destino `0x0c374200`, ciclo `1653731520`, ordinal 0, coincide con lectura
  dynarec PC `8c051406`, EA `ac374200`, width 4, valor `1c200005`, mismo ciclo y ordinal
  de lectura 0. Luego PC `8c0516fc` relee esa dirección en ciclo `1653731968`; loads de
  bytes desde `ac374204` en adelante decodifican campos de esa respuesta (incluye ASCII
  `Dreamcast`). Esto establece consumo SH-4 real en esta ruta y la correspondencia de
  alias P2 para los buffers observados, no un recorrido completo ni que todos los eventos
  sean input de gameplay. 104 lecturas solapadas comparten exactamente un ciclo presente
  entre los eventos DMA del mismo destino; el resto requiere correlación causal más fina
  por ciclos/orden porque el DMA logger y el SH-4 logger tienen contadores ordinales
  separados y ambos alcanzaron cap.
- El resultado satisface la evidencia solicitada de lectura SH-4/dynarec + destino
  Maple para este recorrido diagnóstico. No demuestra coin A0, un paso completo VBlank +
  drain, checkpoint, ni repetibilidad B/D. T00 sigue `NEEDS_EVIDENCE`/pendiente de revisión
  por esas condiciones. No se cambió core ni se inició recorder/player.

### 4.12 Captura acotada del registro A0 (2026-10-29)

- La inspección de `core/hw/holly/sb_mem.cpp` y `core/hw/naomi/atomiswave.cpp`
  confirma que el registro A0 `0x00600280` se atiende en el área 0 cuando `addr <=
  0x006007ff`; el alias SH-4 P1 esperado es `0xa0060280`. La prueba previa con
  `[0xac006280,0xac006284)` usó un alias P2 incorrecto para esta dirección y dio cero;
  ese resultado se descarta como evidencia del registro.
- Corrida scratch PID 17436, 180 s, exit 0, filtro P1 `[0xa0060280,0xa0060284)`, log
  1.191.813 bytes, `Using Recompiler`, 10.000 eventos Maple (tope), cero lecturas
  `KOFXI_SH4_READ`, cero `D[HOLLY]`, 37 hitos AW runtime y cinco `Erase Sector`. El cero
  no determina si el registro fue leído en esta sesión: no se verificó que el hook
  `shop_readm` cubra la ruta MMIO ejecutada ni se capturaron llamadas de `sb_mem`.
- La instrumentación Frida previa §4.3 observó en otro proceso dos retornos A0: el
  especial inicial `0x00` de RAM-test y un retorno normal `0x0f` con los cuatro slots
  inactivos (`maple_atomiswave_coin_chute` devolvió `false` para todos). Es evidencia
  exploratoria dirigida, no de coin activa ni del mismo build/runtime que la captura
  SH-4. En el proceso más reciente no se introdujo coin, conforme a la instrucción de
  no probarla mientras persistiera el aviso de inicialización. Por tanto no está
  verificado el requisito T00 de registrar fuentes activas por slot y comparar la
  transformación temporal/valor retornado en A0. En esa corrida no se había añadido hook
  en `atomiswave.cpp`; la instrumentación posterior autorizada se registra en §4.13.

### 4.13 Hook diagnóstico directo A0 y corrida idle (2026-10-29)

- Tras autorización explícita del dueño en esta sesión, se añadió a
  `atomiswave::readMem_A0_006` un logger acotado a `_DEBUG && _WIN32`, sin volver a
  consultar ninguna fuente ni cambiar la transformación/retorno. Registra ciclo guest,
  ordinal, anchura, rama (`ram-test`/`coin`), máscara de slots cuyo
  `maple_atomiswave_coin_chute(slot)` retornó true, los cuatro `coin_chute_time` después
  del cálculo y byte retornado. Límite: 10.000 eventos por proceso. No modifica
  serialización ni estado del core más allá de los contadores diagnósticos/logger.
- Build `windows-msvc-lab` Debug recompiló `atomiswave.cpp` y enlazó con éxito. EXE
  fuente SHA-256 `4A267B1D605C82376F87F35FE4B37345170526F78CCD6D3B94F6CF1D911E391B`.
  Corrida nueva scratch PID 27004 terminó exit 0 tras 180 s en
  `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-traces\20261029-194152-003`.
  Log SHA-256 `81DC7B837CAA28632BFBD18BC0943282072FB4E1FD2BCE57BBB7B0C1D239957B`,
  1.767.919 bytes; ROM SHA-256 `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`,
  fixture SHA-256 `F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA`.
  El run confirma `Using Recompiler`, 10.000 eventos Maple (cap), cero `D[HOLLY]`, 36
  hitos AW runtime, cinco `Erase Sector` y cero textos `backup data was cleared` en esta
  corrida. El fixture `.nvmem` y el `.nvmem2` generado permanecen aislados en scratch;
  hash final `.nvmem` `FF18C7A7626992E72511C7DE86E2544C662107E86BF38F041B45D5CEF13CBB9B`,
  `.nvmem2` `404E49D7CA24501E0D786C0DC9CC99B5DE6149D03CB387617D560153CD3BAD5C`.
- El hook registró 3.896 lecturas A0 (`ordinal` 0–3895, sin alcanzar su cap): una rama
  `ram-test`, ciclo 43.008, size 4, slots 0, timers `0,0,0,0`, retorno `0x00`; después
  3.895 ramas normales, todas con slots `0`, timers `0,0,0,0` y retorno `0x0f`. Dos
  lecturas normales fueron de 4 bytes y 3.893 de 1 byte. El primer retorno normal fue
  ciclo 50.176; la última muestra llegó al ciclo `14.658.409.920`. Esto confirma
  observabilidad directa de la función A0, rama inicial y retorno idle en el build de
  laboratorio. No demuestra ninguna coin activa ni toda la secuencia temporal para coin.
- Las dos primeras muestras coinciden exactamente con la observación Frida independiente
  anterior §4.3: ciclos 43.008/50.176 y retornos `0x00`/`0x0f`. Esa coincidencia aporta
  corroboración del valor/ciclo inicial en esta identidad de ROM y ruta de arranque; no
  es comparación byte a byte del resto de la ejecución ni medición de overhead.
- No se introdujo coin. Aunque en este log no apareció el texto de backup-cleared, el
  historial de sesión reporta el aviso de inicialización persistente y las cinco
  operaciones `Erase Sector` no prueban que el juego esté listo. Esperar confirmación
  explícita del dueño/estado de la escena antes de coin activa. A0 cubre ahora una parte
  del criterio T00; faltan fuentes activas/transformación bajo coin y el predicado de
  step VBlank+drain. No se inició recorder/player ni B/D.

### 4.14 Muestreo post-timeslice y candidato de drain Maple (2026-10-29)

- Con autorización explícita solo observacional, se añadió una sonda Debug/Windows en
  `UpdateSystem_INTC()`, después del tick del scheduler y de actualizar la interrupción
  pendiente. Incrementa un contador diagnóstico en la rama SPG de VBlank y consulta
  `SB_MDST`, `mapleDmaOut.size()` y `sh4_sched_is_scheduled(maple_schid)` al final de
  timeslice. No pausa SH-4, cambia ciclos, altera callbacks ni modifica el resultado de
  `UpdateSystem_INTC()`. Tras el primer VBlank registra solo la primera muestra y los
  cambios de los tres indicadores, con tope de 10.000 líneas. La etiqueta
  `maple_drain_candidate` es deliberada: no afirma drain global.
- Build `windows-msvc-lab` Debug recompiló `sh4_interpreter.cpp` y enlazó con éxito.
  EXE SHA-256 `92923A668E4D5A845FD3E6810AA8A58090E7AD75CD1141DA12EC604A6F7A81A8`.
  Corrida scratch PID 15700, 180 s, exit 0, directorio
  `C:\Users\Tortita\AppData\Local\Temp\opencode\kofxi-sh4-traces\20261029-201816-116`.
  ROM SHA-256 `8FBAFB8615986981E17E49E49A80AC4806B3E758A36A1880F7B528D1BFF19D44`, fixture
  individual SHA-256 `F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA`; el EXE
  de scratch coincide por hash. Log SHA-256
  `42995A66CE1B05424F2F18C4DDCEA12F57615CF5E0E7D8DF20EE8F0913F439C3`, 3.805.347 bytes.
- Se analizaron exactamente 10.000 muestras de frontera y 10.000 entregas DMA Maple
  (ambos streams alcanzaron el cap). En las muestras: 6.833 estados tenían
  `mdst=0,pending=0,scheduled=0,candidate=1`; 3.167 tenían
  `mdst=1,pending=4,scheduled=1,candidate=0`. Es evidencia runtime de que el estado
  Maple pendiente aparece en fronteras post-tick y, en fronteras posteriores observadas,
  vuelve al predicado candidato. No se registró cada timeslice, solo VBlank y cambios de
  estado; los 10.000 eventos DMA tampoco son necesariamente el total. La captura previa
  §4.8–4.13 cubre destinos/lecturas SH-4; esta corrida no configuró filtro SH-4 y su
  `KOFXI_SH4_READ=0` no es evidencia negativa. Tampoco demuestra correlación exhaustiva
  entre cada DMA y transición, callbacks de otros subsistemas drenados, suficiencia del
  predicado para quiescencia global ni que la escena haya alcanzado gameplay.
- Correlación por ciclo de los prefijos: hubo 3.167 transiciones observadas hacia estado
  Maple pendiente y 3.166 transiciones de vuelta al candidato. De estas últimas, 2.500
  comparten ciclo exacto con una línea `KOFXI_MAPLE_DMA`; para 2.500 transiciones a
  pendiente, el siguiente evento DMA registrado aparece a no más de 600.000 ciclos. Los
  restantes no se atribuyen a ausencia de DMA: las muestras/probes continúan después de
  que el stream DMA alcanza su cap y, además, la sonda solo registra cambios/VBlank, no
  cada timeslice. Esto corrobora la relación temporal dentro de la intersección observada,
  pero no es una traza 1:1 completa ni valida que todo trabajo iniciado esté drenado.
- Observabilidad VBlank: 4.291 líneas de sonda distintas tras activarse, con incrementos
  del contador de VBlank. El registro A0 produjo 4.018 eventos; hubo 37 hitos de runtime
  AW, cinco `Erase Sector`, cero `D[HOLLY]` y ningún texto `backup data was cleared` en
  esta ventana. Esto no autoriza introducir coin ni demuestra inicialización completa.
  Los saves generados/modificados permanecen en el directorio scratch; no se reutilizan
  como par original ni se versionan.
- Cierre: la sonda establece que el muestreo post-timeslice puede ver pendiente→candidato
  drenado de Maple en esta ejecución. El predicado queda **sin validar como límite de
  step**: falta revisión formal T00/T08, correlación completa/una prueba de falsos
  positivos y contabilizar callbacks no-Maple. T00 permanece abierto; T08 y T01 siguen
  bloqueados. No se inició recorder, checkpoint ni comparación B/D.

# Propuesta T00/T08 — contrato temporal P0 para replay de KOF XI

Fecha: 2026-10-29. Estado: **propuesta para revisión**, no contrato aprobado ni
autorización de implementación. Complementa el [plan de estado/replay](state-replay-memory-plan.md)
§P0/E1 y corrige la hipótesis inicial del [baseline P0](state-replay-p0-baseline.md).
Responsables de revisión: T00 (contrato temporal), T01 (laboratorio), T08
(estado/restore); T05 revisa el impacto de instrumentación. No se modifica el core
en este documento.

## 1. Hallazgo que cambia la hipótesis P0

`MapleConfigMap::GetInput()` **no es el punto donde se sabe qué consumió el guest**.
En `core/hw/maple/maple_devs.cpp:185-200` se lee estado host y se construye la
respuesta Maple (`MDCF_GetCondition`); el resultado cruza al guest más tarde por el
DMA de Maple. Además, AW lee monedas por una ruta distinta: `atomiswave.cpp:49-83`
consulta `maple_atomiswave_coin_chute()` al leer el registro guest A0 `0x280`.
`MapleConfigMap::GetInput()` es una fuente intermedia, no una confirmación de
consumo. El vblank tampoco es el instante de consumo: puede disparar el DMA de Maple
(`maple_if.cpp:48-76`), pero MDST también inicia el DMA directamente (`:86-95`), y
la entrada consumida es la respuesta que el SH-4 recibe/lee.

Por tanto, la propuesta anterior de registrar `(vblank, puerto, valor de GetInput)`
se **retira** como definición de `input-consumed`. Puede mantenerse como telemetría
diagnóstica intermedia, etiquetada como tal, pero no como fuente de replay/anclas.

## 2. Definiciones propuestas

- **Ciclo guest:** `sh4_sched_now64()` en el punto de observación, con el valor
  exacto antes de la operación. Es reloj de orden; no se ajusta para alinear estados.
- **VBlank:** evento de presentación/temporización PVR. Se registra como contexto
  (`vblank_index`) si está disponible, nunca reemplaza el ciclo ni el orden real.
- **Tick de replay:** unidad lógica asignada por el runner a un intervalo de
  simulación/reproducción. Su relación con vblank y el conjunto de polls debe
  resolverse en T00; no se presupone 1:1.
- **Acceso consumido:** acto observable por el guest que expone un valor derivado
  de input externo. Debe capturarse al nivel de la respuesta/lectura guest, después
  de las transformaciones y reglas temporales del emulador, no en SDL ni en
  `mapleInputState`.
- **Orden:** contador monotónico de observaciones en la sesión. Dos accesos en el
  mismo ciclo no se colapsan.

## 3. Dominios iniciales de KOF XI AW

| Dominio | Camino observado | Punto candidato de captura | Replay |
|---|---|---|---|
| Botones del mando | `MDCF_GetCondition` → `MapleConfigMap::GetInput()` → mapeo AW → respuesta Maple → DMA a RAM guest | Bytes efectivos de la respuesta para el mando, asociados a puerto/bus y finalización/entrega DMA a guest | Emular la respuesta en la frontera Maple convenida; nunca inyectar el `kcode` host como si el guest lo hubiera leído |
| Monedas/servicio | Lectura SH-4 de `atomiswave::readMem_A0_006(0x280)` → `maple_atomiswave_coin_chute()` + ventana temporal 100 ms | Valor efectivo retornado al SH-4 (incluida la primera lectura especial de RAM test, que debe quedar diferenciada) | Reproducir el valor leído y preservar su ciclo/efecto temporal; no sintetizar monedas a partir de eventos de teclado |
| Otros dispositivos | `GetKeyboardInput`, `GetMouseInput`, lightgun y otros `GetInput()` | No incluidos hasta confirmar que KOF XI los usa y acordar observación de respuestas/lecturas | Si aparecen en corpus, ampliar explícitamente; no tratarlos como neutral sin evidencia |

Para los botones no se debe asumir que registrar el buffer Maple basta: hay que
confirmar el punto de copia DMA, posibles continuaciones/errores y si el SH-4 puede
leer el destino antes o después del callback. El comentario de `maple_if.cpp:26-32`
advierte que el controlador está simplificado. T00/T08 deben seleccionar el punto
preciso con trazado y revisión de orden/hilos.

## 4. Registro mínimo propuesto

Cada muestra tiene un `session_id` local, `sequence`, `guest_cycle`, `vblank_index`
opcional, `domain`, `device/slot`, `operation`, `value_bytes` (o valor tipado con
anchura/endianness explícitas), `source` y `delivery_status`. En el caso Maple,
guardar la respuesta de juego que ve el guest y su ciclo de DMA; en el caso coin,
guardar la lectura efectiva A0 `0x280`, no solo la máscara de botón. Los bytes se
representan sin padding/struct host.

La repetición valida en la primera discrepancia: secuencia, dominio, ciclo guest,
identidad del dispositivo/puerto y bytes/valor efectivo. Ciclo distinto es
divergencia, aunque el valor coincida. Las diferencias se reportan, no se alinean.

## 5. Borde de estado y operaciones

No se fija aún un punto universal "justo después de vblank". Se propone que T00/T08
definan un checkpoint **entre operaciones guest**, cuando no haya callback Maple/DMA
parcial y con scheduler, SH-4, IRQ y dispositivos en un estado serializable. La
evidencia inicial debe identificar si la captura ocurre antes o después del próximo
evento scheduler, y si el primer input del paquete se consume en ese estado o en el
siguiente `step`.

Contrato a revisar, no API:

```text
capture() -> estado completo local + cursor de secuencia de input
restore(estado) -> estado restaurado y cursor restaurado, sin avanzar tiempo guest
step(entradas) -> avanza hasta una frontera acordada; devuelve eventos consumidos
```

Precondiciones de `capture/restore/step`: un único propietario del estado emulado;
sin mutación concurrente durante snapshot/restore; buffers de resultado válidos
hasta que el consumidor confirme; no guardar punteros host; no cambiar ciclos,
colas ni calendario para facilitar replay. T00/T08 deben precisar pausa y afinidad
de hilo reales antes de implementar.

## 6. Decisiones solicitadas a T00/T08

1. ¿Se acepta acceso guest observable (respuesta Maple entregada y lectura A0 `0x280`)
   como definición, en vez de evento host/`GetInput()`?
2. ¿Qué significa "entregada" para Maple DMA: generación de respuesta, escritura
   completa a RAM o primera lectura SH-4 del buffer? Elegir una sola como contrato y
   justificarla con trazas.
3. ¿El reloj canónico será ciclo SH-4 en cada acceso, más contador de orden; vblank
   solo contexto? ¿Cómo representar múltiples accesos por frame?
4. ¿Cuál es el punto de checkpoint quiescente y el límite exacto de `step`?
5. ¿Qué propietario/hilo impide capturas concurrentes y cómo se invalidan buffers?
6. ¿Qué campos de scheduler/IRQ/DMA/dispositivos deben acompañar al estado completo
   para restaurar el cursor de input sin discontinuidad?

Hasta resolverlas, P0/E1 y P1 siguen bloqueados para implementación del recorder/player.
No se infiere aceptación por haber compilado el build lab.

## 7. Verificación y límites

Verificación estática de las rutas `maple_devs.cpp`, `maple_if.cpp`, `maple_cfg.cpp`,
`atomiswave.cpp` y `spg.cpp`; sin traza instrumentada, ejecución ni revisión T00/T08.
El punto de captura final y el dominio real de inputs consultados por KOF XI siguen
siendo desconocidos. Una respuesta generada/emulada no prueba por sí sola que el
guest la haya leído; la aceptación debe fijar explícitamente ese límite observable.

## 8. Decisión parcial del dueño (2026-10-29)

Para el piloto B/D, el dueño exige observar la **lectura SH-4 real** del valor, no solo
la respuesta Maple generada o copiada a RAM; la observación debe cubrir la ruta dynarec
si esa es la ruta ejecutada. Por tanto, `response-delivered` queda como telemetría
diagnóstica y no satisface por sí sola `input-consumed` para este piloto. Esta decisión
no define todavía los otros campos del contrato: límite de `step`, punto quiescente
exacto y requisitos del serializer siguen pendientes de revisión T00/T08. El dueño
también asumió la responsabilidad/revisión técnica T08; la asignación no equivale a
haber completado esa revisión ni a aprobar cambios de core.

El dueño aclara que la experimentación inmediata será Windows, pero el contrato debe
ser portable después. Se elige para el alcance coin registrar fuentes por slot y
verificar el valor retornado por cada lectura A0 `0x280`, reproduciendo las fuentes y
dejando ejecutarse la transformación original (incluida la rama inicial RAM-test), no
inyectando solo el byte final. El dueño fija el reloj en `sh4_sched_now64()` más ordinal
monotónico por evento. La fuente define un contador de ciclos SH-4 de 64 bits
(`sh4_sched_ffb - Sh4cntx.sh4_sched_next`), cuyo acumulador se serializa; no es un reloj
del host ni una API Windows. `SH4_MAIN_CLOCK` fija la escala guest (200 MHz en esta
configuración). El ordinal preserva orden y accesos simultáneos; su estado/cursor debe
formar parte del checkpoint. El dueño también decide que el checkpoint incluya el
`mapleInputState[4]` completo junto al cursor replay. Para el piloto, el dueño fija
`step` desde una frontera quiescente hasta después del siguiente VBlank y de drenar
callbacks/DMA ya iniciados; cada step puede contener cero o más lecturas SH-4. VBlank no
es por sí mismo la frontera final: `maple_vblank()` puede llamar a `maple_DoDma()` y este
programa `maple_schd`; un write a `SB_MDST` también inicia Maple DMA fuera de VBlank
(`core/hw/maple/maple_if.cpp:48-95`). El punto final es después del drain, sin Maple DMA
en curso ni `mapleDmaOut` pendiente; verificar que ese predicado sea suficiente y
observable forma parte de la revisión T08/T00.

### Predicado Maple para revisar (estático; runtime pendiente)

La lectura de `core/hw/maple/maple_if.cpp` junto con `core/hw/sh4/sh4_sched.cpp`
concreta una condición candidata para muestrear **fuera de cualquier callback** en la
frontera del runner: `SB_MDST == 0`, `mapleDmaOut.empty()` y
`!sh4_sched_is_scheduled(maple_schid)`. `maple_DoDma()` acumula respuestas y programa
`maple_schid`; `maple_schd()` copia el vector, limpia `SB_MDST`, eleva IRQ, vacía el
vector y retorna `0`. El scheduler es de disparo único: `handle_cb()` marca
temporalmente `end=-1` antes de ejecutar el callback y solo rearma si este devuelve un
retardo positivo. Por eso consultar `sh4_sched_is_scheduled()` **dentro** del callback
no prueba que ya terminó; la comprobación debe hacerse después de que el callback
retorne y el scheduler cierre el tick. El camino `SDCKBOccupied` puede invocar
`maple_schd()` síncronamente desde `maple_vblank()`; también existe inicio directo por
`SB_MDST`, no asociado a VBlank. Estos tres campos cubren el trabajo Maple pendiente
visible según la implementación actual, pero no demuestran por sí solos que todos los
callbacks de otros subsistemas estén drenados ni que el punto de muestreo sea una
frontera quiescente global. Es una derivación estática, no resultado runtime; T00/T08
deben acordar un lector en la frontera del runner y probar que el predicado no pasa
antes ni demasiado tarde. No se añadió instrumentación scheduler en esta revisión.

### Límite de `Sh4Executor::Step()` y candidato de frontera (estático)

La interfaz `Sh4Executor` expone `Step()`, pero no equivale al step lógico acordado:
`Emulator::runInternal()` consume `singleStep` llamando una sola vez al executor; en
`Sh4Interpreter::Step()` se ejecuta una instrucción con `ReadNexOp()`/`ExecuteOpcode()`
y no se llama `UpdateSystem_INTC()`. Por tanto, repetir esta API no garantiza que se
ejecuten los callbacks del scheduler y no puede por sí sola alcanzar VBlank+drain.
En el intérprete normal, `UpdateSystem_INTC()` se llama después de agotar
`SH4_TIMESLICE`; el dynarec x64 hace lo mismo en su `slice_loop` (bucle en
`core/rec-x64/rec_x64.cpp`, llamada a `UpdateSystem_INTC()` y vuelta a `run_loop`). Esa
función decrementa `sh4_sched_next`, ejecuta `sh4_sched_tick(SH4_TIMESLICE)` si vence
algún evento y luego actualiza interrupciones.

Además, `Sh4Recompiler` hereda `Step()` de `Sh4Interpreter` y no lo sobrescribe
(`core/hw/sh4/dyna/ngen.h`); por tanto, usar `Step()` con el recompiler seleccionado
ejecuta el opcode individual por el intérprete, no por el bloque dynarec. El `Run()` del
recompiler llama a `sh4Dynarec->mainloop()`, cuyo bucle generado revisa `CpuRunning` al
inicio y vuelve después de `UpdateSystem_INTC()`. Así, ni repetir `Step()` ni parar el
executor genérico es evidencia de que el mismo camino dynarec avanzó hasta el límite
temporal acordado.

Consecuencia: la observación del límite de step candidato debe ocurrir en un lugar que
se ejecute **después** del tick del scheduler de la timeslice, fuera de callbacks, y en
el hilo propietario del SH-4; muestrear al retornar de `Step()` instruction-level o
dentro de `maple_schd()` no satisface eso. Un runner podría continuar dynarec timeslices hasta
haber observado el siguiente VBlank y después comprobar el predicado Maple del apartado
anterior tras terminar el callback/tick. Falta definir cómo pausar/salir del `Run()` en
ese punto de forma portable entre intérprete y dynarec sin alterar orden/ciclos, y cómo
contabilizar los callbacks no-Maple ya iniciados. No se implementó el runner ni se
instrumentó `UpdateSystem_INTC()`: ese cambio toca el ciclo compartido de CPU y requiere
revisión/coord. T00/T08. Es una propuesta de punto de observación, no contrato aprobado
ni evidencia runtime del step.

### Consecuencia para la observación dynarec

Para el build Windows x64 del laboratorio, la inspección estática de
`core/rec-x64/rec_x64.cpp` muestra dos caminos de
`shop_readm`: `GenReadMemImmediate()` puede emitir cargas host directas a un puntero de
RAM (`mov` de byte/word/dword), mientras que el caso no inmediato genera una llamada a
`MemHandlers` tras preparar la traducción. Por eso, enganchar solamente `ReadMem32` o
un handler de memoria no garantiza cubrir todas las lecturas del buffer Maple. Para
cumplir la decisión del dueño, el diseño de instrumentación debe observar los loads
guest SHIL `shop_readm` en el camino dynarec **después de obtener el valor**, anotando
PC guest, dirección guest efectiva, anchura, valor, ciclo SH-4 y orden; filtrar por
rangos de respuesta Maple ya entregados, cuya dirección se correlacione primero. El
acceso coin A0 se observa aparte a través de `readMem_A0_006`, que ejecuta la lectura
y devuelve el valor transformado.

El hook temporal autorizado está implementado en `core/rec-x64/rec_x64.cpp` bajo
`_DEBUG && _WIN32`. Guarda la EA guest antes de MMU; llama al logger después del load,
preservando el resultado SHIL y los XMM caller-saved mediante `GenCall`. Para evitar
volcado masivo, solo emite direcciones dentro del rango exclusivo configurado por
`KOFXI_SH4_READ_TRACE_START` y `KOFXI_SH4_READ_TRACE_END`; el límite es 10 000
registros por proceso. Registra PC guest, EA guest, ancho, valor,
`sh4_sched_now64()` muestreado dentro del hook y ordinal. En corrida P2 scratch este
hook observó 10 000 loads (cap), con 6 274 solapados por dirección con spans DMA Maple
y una coincidencia inicial de destino/EA canonicalizada, ciclo y ordinal; ambos streams
alcanzaron cap, así que esto es evidencia parcial, no correlación completa ni aceptación
de gameplay.

Con autorización adicional explícita del dueño, `core/hw/naomi/atomiswave.cpp` también
tiene hook `_DEBUG && _WIN32` dentro de `readMem_A0_006`: registra rama RAM-test/coin,
ciclo, anchura, slots cuyo `maple_atomiswave_coin_chute` resultó activo, timers y byte
retornado, sin volver a consultar fuentes ni reemplazar la transformación. Build Debug
del preset `windows-msvc-lab` recompiló y enlazó `flycast.exe` con éxito, SHA-256
`4A267B1D605C82376F87F35FE4B37345170526F78CCD6D3B94F6CF1D911E391B`. Corrida scratch
de 180 s observó un RAM-test `0x00` y 3 895 lecturas normales idle `0x0f`, slots
inactivos. No hubo coin activa; T00 no queda cerrado. Detalle en paquete T05 §§4.11–4.13.

La cobertura actual se limita deliberadamente a cargas enteras SHIL `shop_readm` de
1/2/4 bytes en el dynarec Windows x64; cargas de 8 bytes y resultados asignados como
FPU se omiten. No cubre rec-x86, ARM/ARM64 ni interpreter. El hook SH-4 no observa DMA
ni establece por sí solo los rangos Maple. Se añadió logging Debug/lab de destino y
longitud justo después de `memcpy` en `maple_schd`, limitado a 10 000 eventos; en el
primer runtime no apareció ninguna línea de este logger, así que el rango sigue sin
descubrir. El ordinal indica orden de hooks, pero el muestreo `now64` puede coincidir
para varias lecturas dentro del mismo bloque; no se interpreta como ciclo de instrucción
exacto si el scheduler no avanza entre ellas. La observación toca el dynarec compartido
solo en Debug Windows autorizado; T08 debe revisar instrumentación, punto quiescente y
overhead antes de aceptación. Una futura matriz requiere evidenciar cada backend por
separado.

### Inventario estático preliminar para revisión T08

La revisión de fuente (no de un restore real) encontró estos campos relevantes:

- `sh4::serialize` guarda `Sh4cntx.cntx`, interrupciones y llama al serializer del
  scheduler; el scheduler guarda `sh4_sched_ffb` y entradas de callbacks, incluida
  `maple_schid` (`core/hw/sh4/sh4_mmr.cpp`, `sh4_sched.cpp`).
- `mcfg_SerializeDevices` guarda el vector pendiente `mapleDmaOut` (destino y payload)
  y serializa el estado de los dispositivos Maple (`core/hw/maple/maple_cfg.cpp`).
- `atomiswave::serialize` guarda `coin_chute_time[4]`, `aw_ram_test_skipped` y
  `aw_maple_devs` (`core/hw/naomi/atomiswave.cpp`).
- `mapleInputState[4]` se define en `maple_cfg.cpp` y `maple_DoDma` lo actualiza vía
  `ggpo::getInput`; no encontré serialización explícita de ese global. El dueño decide
  que el checkpoint lógico incluya ese array completo más el cursor replay, para cubrir
  lecturas A0 antes del siguiente DMA. Debe verificarse cómo se implementa/serializa
  esto sin añadir dependencias del estado de host.
- `sh4::serialize` omite `mem_b` cuando `ser.rollback()` es true. En la ruta GGPO
  inspeccionada, RAM/VRAM/ARAM/ELAN RAM se capturan aparte
  como deltas de `memwatch` y se restauran antes de `dc_deserialize`; no deben contarse
  dos veces ni omitirse al auditar ese modo. En cambio, `mapleInputState[4]` no figura
  en `dc_serialize`, `mcfg_SerializeDevices` ni en las rutas de deltas listadas, pese a
  ser consultado directamente por dispositivos/lecturas AW entre llamadas
  `maple_DoDma`. La ruta GGPO guarda el frame y serializa con `rollback=true`; al cargar,
  restaura deltas y luego ejecuta `dc_deserialize`. Esto no satisface por sí solo la
  decisión de incluir el array completo y cursor replay en el checkpoint B/D. T08 debe
  decidir y validar una captura local completa independiente de la semántica GGPO.
- `dc_savestate()` en `core/nullDC.cpp` llama a `dc_serialize()` dos veces (primero
  para medir y luego para escribir) y no contiene una aserción local de CPU detenida;
  en contraste, el callback GGPO `save_game_state()` verifica
  `!emu.getSh4Executor()->IsCpuRunning()` antes de serializar. Esto no prueba que la
  ruta nativa sea insegura: los callers deben imponer pausa/afinidad fuera de esa
  función. La ruta de hotkey pasa por `gui_saveState()` (default `stopRestart=true`),
  que llama `emu.stop()` antes de salvar y `emu.start()` después; `gui_loadState()` hace
  stop/load/start. `Emulator::unloadGame()` también detiene primero y luego puede
  auto-salvar; el filtro UWP detiene antes de su autosave. Lua `saveState()` llama
  `gui_open_settings()` cuando el GUI está cerrado, pero esa función solo llama a
  `emu.stop()` si `achievements::canPause()` es true. Si es false, vuelve sin abrir el
  menú y Lua continúa a `dc_savestate()`; `dc_savestateAllowed()` no comprueba esa
  condición ni `IsCpuRunning()`. `Achievements::canPause()` devuelve true cuando
  achievements no está activo, pero con achievements activos devuelve directamente el
  resultado de `rc_client_can_pause`; por tanto existe una condición alcanzable en la
  que ese camino no ejecuta `emu.stop()` y prosigue al serializer si Lua invoca
  `saveState()`. El camino de ejecución verificado en fuente es síncrono: el mainloop
  x64 llama `UpdateSystem_INTC()` al terminar cada slice; esa función hace tick del
  scheduler, cuyo `handle_cb()` marca el evento como no programado y llama directamente
  el callback. La callback SPG alcanza `rend_vblank()` → `emu.vblank()` →
  `EventManager::event(Event::VBlank)` → `lua::emuEventCallback()` → callback Lua
  `vblank`. `Sh4Recompiler` hereda `Sh4Interpreter::Start()` (que activa
  `CpuRunning`) y su `Run()` ejecuta el mainloop dynarec. Si el script de VBlank llama
  `saveState()` cuando `canPause()` es false, el guardado ocurre reentrante dentro del
  callback del scheduler, sin que `dc_savestateAllowed()` compruebe o pare la CPU.
  Esto no es necesariamente una carrera entre hilos: la llamada se efectúa en la misma
  pila síncrona. Hay además una consecuencia concreta que T08 debe comprobar: en
  `sh4_sched.cpp::handle_cb`, la entrada se marca `end=-1` antes de llamar la callback,
  y solo se vuelve a programar al retornar con `re_sch > 0`. Una captura reentrante desde
  el `Event::VBlank` serializa el `vblank_schid` en ese estado desarmado
  (`sh4_sched_serialize` guarda `start/end`). El load ejecuta `spg_Deserialize()` (que
  restaura registros internos pero no llama `rescheduleSPG()`), restaura el scheduler y
  luego `sh4_sched_ffts()`; esta última excluye entradas con `end == -1`. Por inspección,
  el estado restaurado podría quedarse sin el siguiente callback SPG/VBlank porque la
  pila de `spg_line_sched()` que reprogramaría el evento no se conserva. Es una
  predicción causal de fuente, aún falta un repro round-trip controlado antes de
  declararla fallo observado. La instantánea además se toma antes de que
  `spg_line_sched()` termine el trabajo que sigue a `rend_vblank()`.
  En conjunto, las rutas ordinarias GUI/hotkey paran antes de salvar; no asumir que toda
  invocación nativa/Lua hereda esa garantía. T08 debe probar afinidad/estado CPU en todos
  los callers y estabilidad entre las dos pasadas, además de revisar errores/excepciones
  del ciclo stop/start.

### Repro T08 propuesto (no ejecutado)

En un build scratch idéntico y con una ROM/saves de prueba autorizados, cargar un
script Lua con `flycast_callbacks.vblank` que solicite un único `saveState()` en el
callback VBlank bajo una condición controlada donde `achievements::canPause()` resulte
false. Registrar el valor `end` del `vblank_schid` al entrar en `dc_serialize()`; luego,
en un proceso nuevo de la misma identidad, cargar ese estado y comprobar si se emite el
siguiente `Event::VBlank` dentro del intervalo guest esperado. Usar como control otro
checkpoint tomado tras retornar completamente de `spg_line_sched()` y verificar que sí
recibe el VBlank siguiente. Si el save reentrante restaura sin evento y el control sí lo
recibe, queda reproducida la hipótesis; si ambos reciben el evento, investigar qué ruta
rearma el callback y corregir la hipótesis. Mantener la prueba offline/scratch, sin
guardar ningún save runtime en Git. Requiere coordinación y aprobación T08 antes de
  instrumentar o cambiar el core; no es permiso para implementar el hook/recorder.

Este inventario no prueba atomicidad, propiedad de hilo, integridad de todos los
dispositivos, ni round-trip de save/restore; queda como lista de verificación para la
revisión que asumió el dueño. No habilita cross-load entre backends ni implementación
de hooks/recorder.

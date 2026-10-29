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

# Flycast especializado en KOF XI: emulación, rollback y online verificables

Fecha: **2026-10-28**. Estado: **plan de ejecución; experimentos pendientes**.
Prioridad y continuidad del fork: [roadmap rector](../roadmap.md).
Procedencia del plan y evidencia histórica: [traspaso](../evidence/research-handoff.md).
Trasladado el 28-oct desde `kofxi_aw_modding`; las referencias al maestro en los
procedimientos de este documento corresponden ahora al roadmap rector del fork.

## 1. Objetivo y contrato de éxito

Conseguir una mejora real de KOF XI Atomiswave sobre versiones identificadas de
Flycast y Dojo: rendimiento, respuesta, estabilidad o fidelidad. Construir un fork
mantenible y online propio solamente después de demostrar la ventaja del núcleo.
**Requisitos obligatorios ampliados por el usuario el 28-oct:** Windows, Linux,
Android y Nintendo Switch, cross-play entre ellos, servidor propio de matchmaking
y cliente de bajo consumo. Windows puede iniciar el laboratorio, pero no define
por sí solo el éxito del producto. Diseñar y probar x86-64/AArch64 desde G0/G2.
macOS y otros destinos se incorporan mediante la matriz de plataformas inferior.

Tres afirmaciones distintas, con pruebas distintas:

| Afirmación | Evidencia exigida |
|---|---|
| Mejor manejo/rendimiento | Comparación A/B repetible en el mismo equipo, calidad y carga; latencia o coste menor sin regresiones funcionales. |
| Mejor online | Menos stalls/correcciones/cortes con inputs, red y delay comparables; partidas reales entre máquinas y diagnóstico de desync. |
| Mayor fidelidad de emulación | Fallo reproducible, comportamiento esperado sustentado por hardware AW o referencia independiente pertinente y corrección validada. |

Coincidir con Flycast prueba conservación respecto a esa base, no exactitud del
hardware. Flycast y Dojo comparten código: su acuerdo no constituye dos oráculos
independientes. Fullspeed, filtros gráficos o más FPS tampoco prueban fidelidad.
La aceptación de XI no demuestra compatibilidad general con Atomiswave.

**Éxito mínimo del piloto:** G0–G3 aprobadas y al menos una mejora primaria de
G3 respecto al upstream elegido y al Dojo de uso real, sin regresiones en el
corpus. Si solo mejora una base, describir exactamente esa comparación y mantener
pendiente la superioridad sobre ambas. **Éxito del producto online:** además G4–G6,
matriz obligatoria multiplataforma/cross-play y presupuestos de recursos aprobados.
La afirmación de mayor fidelidad exige por separado la puerta H de la sección 8.

## 2. Base conocida y reutilización

- Dojo 6.46 ya tiene preparación aislada y probes Lua ejecutados:
  [referencia y límites históricos](../evidence/research-handoff.md).
  El contador Dojo no equivale al contador de vblanks durante el arranque.
- `scripts/prepare_flycast_reference.py` prepara copias y probes; no ejecuta el
  emulador ni es un benchmark completo. Usar directorios nuevos por ensayo.
- Auditar antes de reutilizar `capture_flycast_v3.py`, `flycast_ref_log.py`,
  `analyze_flycast_ref.py` y `compare_recomp_flycast_ref.py`: sus contratos no
  garantizan sincronización CPU/IRQ ni conversión automática de replays.
- Reutilizar los escenarios y mapas de memoria existentes, confirmando versión,
  direcciones y semántica. Una RAM de 16 MiB no es un savestate completo.
- Flycast master consultado el 28-oct ya contiene GGPO, deltas por páginas y
  supresión de render/audio durante resimulación. En G0 fijar SHA y verificar
  esas rutas en la versión elegida; no proponerlas como funciones inexistentes.
- El rendimiento histórico del recomp explica evaluar esta vía, pero no sirve
  como control A/B para atribuir mejoras al fork.

Los scripts y corpus de esta sección pertenecen al repositorio de investigación;
no se han importado ni están disponibles automáticamente en este checkout.
T01/T05 deben auditar sus contratos y portarlos o enlazarlos con identidad fija.

## 3. Arquitectura objetivo

```text
Cliente XI
  interfaz / configuración competitiva / entrenamiento
  perfil XI: identidad, inputs, observadores y escenarios
  sesión rollback: inputs por tick, predicción, estados, hashes
  núcleo Flycast: SH-4, scheduler, PVR, AICA/ARM7/DSP, DMA, memoria
  presentación y audio host
            |
       P2P o relay regional
            |
  servicios: salas, matchmaking, resultados, replays y espectadores
```

El perfil XI observa el juego sin sustituir su lógica competitiva. Mantener el
mapa de memoria guest y sus alias; optimizar primero almacenamiento host,
restauración y sincronización. Parches de reglas o entrenamiento son perfiles
explícitos con identidad propia, incompatibles con competitivo vanilla.

Fork con upstream remoto, revisión fijada y parches pequeños por hipótesis;
documentar licencia y procedencia al distribuir. El repositorio de investigación
conserva contratos/informes y referencia al commit del fork. La creación/publicación
del repositorio remoto y el despliegue se ejecutan en sus fases, no al redactar.

## 4. Método obligatorio de comparación

### 4.1 Plataformas, cross-play y servidor común: obligatorios

| Destino | Variantes que se deben fijar en G0 | Aceptación en equipo real |
|---|---|---|
| Windows | x86-64, versión mínima, GPU integrada y equipo de referencia | Instalación, corpus, recursos, mando, matchmaking y cross-play. |
| Linux | x86-64 y AArch64; distro/toolchain mínimos, Wayland/X11 donde se anuncien | Mismas puertas; probar portátil tipo Steam Deck si se anuncia. |
| Android | AArch64, API mínima, dispositivo modesto y medio con SoC/RAM identificados | APK instalable, Vulkan/GLES según soporte real, USB/Bluetooth/táctil, Wi-Fi, ciclo de vida y sesión térmica. |
| Nintendo Switch | AArch64, modelo, toolchain y entorno de ejecución identificados | Hardware real portátil y dock, Joy-Con/mando, red, memoria, temperatura y ritmo sostenido. |
| macOS | Apple Silicon; Intel si se mantiene | Integración prevista con protocolo común; requisitos/equipo fijados antes de anunciar soporte. |
| Otros | PS4 u otros destinos solicitados, por ficha explícita | Registrar arquitectura, backend, controles, transporte, presupuesto y pruebas; no inferir aceptación por usar C++. |

Windows/Linux/Android/Switch son alcance requerido: falta de hardware o toolchain
deja la fila BLOCKED y el producto multiplataforma incompleto, no elimina el destino.
En Switch decidir y documentar entorno homebrew frente a distribución oficial y
sus dependencias reales; no asumir acceso a SDK, tienda ni servicios Nintendo.
El cliente se conectará a nuestro servicio mediante red IP, según ese entorno.

Núcleo compartido y adaptadores de plataforma para ventana/render, entrada, audio,
reloj, almacenamiento y red. CI compila cada destino viable; ninguna compilación
sustituye partidas en el dispositivo. Elegir backends existentes compatibles y
medirlos; no imponer Vulkan a todos ni un backend nuevo sin necesidad demostrada.

**Contrato cross-play:** protocolo versionado, formato canónico de inputs/estado,
orden de bytes y anchos explícitos, sin punteros ni padding host. Handshake de
versión de simulación compatible, perfil y contenido; registrar hash del ejecutable
por plataforma sin exigir binarios idénticos entre SO. Controlar FPU/rounding,
fast-math, dynarec x86-64/AArch64, scheduler y threading para obtener la misma
simulación. Configuración visual distinta solo si no cambia resultados guest.

G2 ejecuta el mismo replay y rollback en x86-64 y AArch64 y compara estado canónico.
G4 prueba las seis parejas distintas Windows/Linux/Android/Switch, ambos roles,
además de parejas del mismo destino; Linux AArch64 requiere cobertura adicional.
Cada pareja debe completar LAN, perfil regional y perfil difícil, por conexión
directa y relay, con al menos tres replays de 10 minutos por combinación y hashes
confirmados iguales. La matriz A/B intensiva de G4 se realiza en una pareja
homogénea y otra heterogénea elegidas antes de medir. Fallos específicos amplían
la matriz afectada; no se ocultan restringiendo el matchmaking por plataforma.

### 4.2 Servidor de matchmaking propio

Dojo ya ofrece matchmaking mediante códigos y uso con Fightcade según su README
consultado. Eso no acredita que dispongamos de un backend propio desplegado ni
que el servicio existente resuelva este producto. G0 inventaría código/licencias,
protocolos y posibilidad de reutilización; G1 fija API, G4 usa servidor de staging
y G5 entrega el servicio operable, en lugar de dejarlo como opción futura.

- Backend Linux desplegable con configuración y migraciones versionadas, CLI o
  contenedores reproducibles; endpoint común para todas las plataformas.
- API HTTPS y canal de eventos compatible con clientes nativos; identidad/sesión,
  presencia, salas privadas por código, cola pública y cancelación/reconexión.
- Emparejar por versión/perfil compatible, región y calidad de ruta medida;
  no excluir sistemas operativos compatibles. Medir RTT, jitter y pérdida.
- Negociación P2P/NAT traversal y relay UDP regional de respaldo; comprobar CGNAT,
  redes móviles, IPv4/IPv6 y cambio de red Android. Registrar recuperación o fin
  ordenado; no prometer continuidad automática al cambiar de IP.
- Matchmaking y transporte de pelea separados: servicio caído no debe detener
  una partida P2P establecida; un relay caído requiere recuperación o cierre
  explícito. Espectadores/replays no bloquean los inputs de jugadores.
- Sesiones autenticadas, expiración y límites de solicitudes; guardar secretos
  fuera del cliente. Métricas de salas, errores, tiempos y tráfico por relay.

**Aceptación del servicio:** crear/buscar/unirse/cancelar/reconectar desde cada
plataforma y completar partida cruzada. Prueba inicial de 1000 conexiones de
presencia y 100 partidas con tráfico relay representativo durante 1 h, declarando
hardware, CPU/RAM/banda y coste; API p95 <250 ms dentro de la región de ensayo
y errores <1 %. Medir asignación técnica de sala p95 <2 s con ambos jugadores ya
disponibles; no confundirlo con espera humana en cola. Inyectar reinicio del
matchmaker y fallo de relay. La capacidad publicada será la medida, sin dar estos
objetivos por alcanzados ni reutilizar el resultado de un servidor más potente.

### 4.3 Cliente liviano y ritmo Atomiswave

Crear perfil **Ligero competitivo**: resolución interna nativa, escalado sencillo,
sin efectos host adicionales, interfaz liviana, cachés limitadas y sin grabación
de vídeo obligatoria. Opciones visuales superiores son locales y medibles.
Usar timings guest originales; no acelerar la lógica, saltar actualizaciones ni
reducir fidelidad de audio/dispositivos para cumplir presupuesto.

En G0 elegir el dispositivo mínimo por plataforma y en G1 publicar presupuesto
**numérico** previo a optimizar: RAM total estable/pico, memoria de snapshots,
caché de texturas, GPU/VRAM o memoria unificada, CPU/GPU ms, potencia/temperatura,
audio-buffer ms e input-to-photon. Sin esos valores no se acepta G3. No fijar
un único límite de RAM a ciegas: contar ROM/cartucho residente, archivos mapeados,
dynarec, dispositivos, frontend y rollback. Medir RSS/PSS/private working set
según SO con definición documentada; la memoria GPU compartida no se suma dos veces.

Metas iniciales de ligereza, además de conservar G2:

| Área | Objetivo y prueba |
|---|---|
| RAM | Reducir >=20 % el pico de memoria atribuible al cliente frente a la base comparable por plataforma; buffers/cachés acotados, sin crecimiento sostenido en 2 h. Presupuesto absoluto fijado en G1. |
| Vídeo | Reducir >=15 % GPU ms/frame en escenas preseleccionadas a resolución/calidad iguales; informar ancho de banda y energía cuando sean medibles. No usar únicamente porcentaje de uso GPU. |
| Controles | Medir captura → consumo guest → presentación y entrada-a-fotón. A/B con el mismo mando/transporte; USB, Bluetooth y controles integrados por separado. Mantener umbral G3 para afirmar mejora de latencia. |
| Ritmo | Velocidad guest/tiempo real de 1,000 ±0,005 en ventanas de 60 s de juego estable, tras calentamiento, sin deriva acumulada >0,5 % en 30 min; p99 de cómputo normal <= T y audio sin underruns atribuibles al cliente. |
| Móviles/portátil | 30 min de calentamiento/carga y 60 min medidos en Android y Switch, clocks de fábrica, modo de energía/carga/brillo/red declarados; cumplir presupuestos también en el tramo térmico final. |

Los porcentajes RAM/GPU son objetivos de mejora, no permisos para degradar la
imagen guest. Comparar por separado stock equivalente y perfil ligero de producto;
quitar filtros del control no acredita una optimización del núcleo. Si no se
alcanza un objetivo, registrar GAP y revisar con evidencia; no declarar cliente
liviano global por mejorar solo Windows. Los presupuestos absolutos y ritmo son
puertas obligatorias por dispositivo, incluso cuando la base ya es eficiente.
La velocidad 1,000 se refiere al timing guest configurado/verificado: demostrar
que coincide con una placa AW exige además H, con frecuencia y tolerancia medidas.

Política de memoria: pools y arenas acotados, recursos inmutables compartidos
cuando proceda, evitar copias de ROM y readbacks GPU innecesarios; evaluar compresión
solo si su coste de CPU/latencia no destruye el margen de rollback. Perfil de red
compatible negociado entre pares, sin truncar estados silenciosamente en móviles.
Reducir colas host y bloqueos, no el presupuesto de simulación. Añadir contadores
de bytes por componente y de frames en vuelo para explicar las mejoras.

### Identidad y controles

Registrar commit, hash del ejecutable, compilador/opciones, ROM/BIOS, NVRAM/flash,
RTC, región/idioma, DIP, inputs/SOCD, estado inicial, backend, resolución, VSync,
audio, mando, pantalla/Hz/VRR, CPU/GPU/RAM, SO/driver y modo de energía.
No incluir ROM/BIOS ni credenciales en paquetes públicos de evidencia.

Controles: upstream stock fijado, Dojo distribuido usado por el usuario y
candidato derivado del mismo upstream. Añadir Dojo más reciente identificado si
el instalado es antiguo; declarar qué comparación mide actualidad y cuál uso real.
Las compilaciones instrumentadas sirven para explicación; repetir rendimiento
en Release con instrumentación mínima y medir su overhead contra stock.

No compartir savestates entre builds sin demostrar compatibilidad. Preferir
arranque controlado y un replay de inputs consumidos en el mismo punto lógico.
Registrar vblank, tick de red, ciclo guest y cada poll relevante; validar el
adaptador entre ellos con pulsos y liberaciones, incluyendo inputs simultáneos.
Las pausas de hitstop y transiciones no deben cambiar la identidad de ticks.

### Estadística y artefactos

- Micropruebas: calentamiento fijo documentado y **10 pares A/B** de al menos
  120 segundos guest por escenario de rendimiento; alternar orden AB/BA con seed.
  Arranque y primera carga se miden aparte, sin ocultarlos mediante calentamiento.
- No correr benchmarks competidores en paralelo. Fijar carga/calidad y registrar
  temperatura, frecuencia y procesos que contaminen el ensayo.
- Informar p50/p95/p99/máximo, deadlines, tiempo guest/host, memoria pico,
  CPU/GPU, coste de save/load/resimulación y asignaciones por frame.
- Estimar intervalo de confianza del 95 % por pares de corridas, no tratando
  cada frame correlacionado como una muestra independiente. Registrar fallos y
  outliers; exclusiones solo por reglas fijadas antes y con resultados completos.
- Diferenciar tiempo de cómputo, espera voluntaria de pacing y cadencia presentada.
  El plazo T procede del timing guest medido; no redondear automáticamente a 60 Hz.
- Guardar `manifest.json`, configuración, inputs, métricas CSV/JSON, stdout/stderr,
  hashes por ancla, resumen, primera divergencia y comandos exactos en
  `artifacts/flycast-kofxi/<fecha>/<experimento>/<run>/` (estructura propuesta).
- Cada lote deja informe fechado con receta y límites, enlazado desde el maestro.
  Los scripts/harness que falten se implementan en G1; esta ruta no implica existencia.

## 5. Corpus de aceptación

| Grupo | Casos mínimos |
|---|---|
| Arranque/flujo | Cold boot con flash controlada, crédito, selección, seis órdenes de equipo, líder, combate, KO, relevo, resultado, revancha y Game Over. |
| Combate | Duck/Gato histórico, Ash hit/block/whiff, proyectiles, agarre, Shen supercancel; añadir quick shift, saving shift, dream cancel, hitstop y efectos simultáneos mediante ejecución natural. |
| Cobertura ampliada | Todo personaje seleccionable del alcance AW participa en combate; todos los escenarios jugables; luchas de tres contra tres, cambios de música y cargas. Registrar interacciones no cubiertas. |
| Sistema | Test Menu natural y salida, modo Evento 1P/2P, inglés/español/japonés, persistencia de ajustes y reset. |
| Rollback | Restaurar antes/durante/después de impacto, cambio de personaje, KO, carga de recursos, DMA y eventos de audio; repetir desde frío y caliente. |
| Robustez | Sesión de 2 h, conexiones cortadas, pérdida en ráfaga, foco/alt-tab y mando desconectado; separar recuperación host de determinismo guest. |

G1/G2 comienzan por el corpus reducido (arranque, selección y pilotos de combate).
G3 exige esos casos más KO/relevo/resultado y transiciones de carga. G5/G6 exigen
la matriz ampliada con cada casilla PASS/FAIL/BLOCKED y artefactos. No sustituir
recorridos naturales por escrituras en RAM. Los fixtures sintéticos sirven para
diagnóstico y llevan etiqueta distinta.

## 6. Fases y puertas de avance

Todas las fases están **pendientes** al crear este documento.

### G0 — Reproducir las bases y escoger el núcleo

1. Identificar instalaciones, fuentes, revisiones, ROM/BIOS y equipo disponible.
2. Preparar copias aisladas; compilar upstream y registrar receta verificable.
3. Ejecutar arranque → selección → combate → resultado en upstream y Dojo.
4. Inventariar integración GGPO, formato de estado, memwatch, renderer, audio,
   polling y configuración efectiva. Comparar mantenimiento/coste de divergencia.
5. Elegir base por resultados, no por nombre; documentar defectos reproducidos.
6. Completar inventario de plataformas de §4.1, dispositivos mínimos, backends,
   toolchains y rutas de build. Ejecutar smoke tests en los destinos disponibles
   y registrar bloqueos de los restantes; inventariar matchmaking existente.

**Entrega:** informe de bases, builds, vídeo y logs del recorrido, tabla de
limitaciones y decisión de fork. **Puerta:** ambas bases ejecutadas e identificadas;
si alguna no está disponible, comparación contra ella BLOCKED. No afirmar mejora.

### G1 — Laboratorio reproducible y baseline

1. Implementar replay de inputs consumidos y anclas; comprobar dos arranques
   equivalentes y documentar cualquier estado no controlado.
2. Capturar estado canónico guest por subsistema, excluyendo solo datos host
   documentados (punteros/cachés reconstruibles), nunca discrepancias inexplicadas.
3. Integrar timers y contadores save/load, páginas sucias, asignaciones,
   resimulación, GPU waits, underruns y profundidad de cola.
4. Automatizar matriz reducida y 10 pares de controles stock/stock; medir ruido.
5. Fijar escenarios de rendimiento y métrica primaria de cada hipótesis antes
   de tocar su implementación. Verificar que el comparador detecta una mutación
   controlada de input y otra de estado; retirar ambas de las corridas normales.
6. Fijar presupuestos por dispositivo de §4.3 y contrato API/cross-play de §4.1–4.2;
   introducir CI multiplataforma y medición de recursos portable.

**Entrega:** harness ejecutable, baseline y esquema de artefactos.
**Puerta:** replays repetibles, anclas verificadas, controles negativos detectados
y ruido cuantificado. Si no hay determinismo, resolver primera divergencia antes
de intentar reducir estados.

### G2 — Rollback correcto antes de rollback rápido

1. Comparar avance continuo con save → avanzar → load → repetir, a profundidades
   1/2/4/8 frames (8 como estrés aunque exceda el límite online configurado).
2. Ejecutar al menos 10 000 restauraciones distribuidas en el corpus reducido,
   incluyendo rollbacks anidados/repetidos y límites del buffer circular.
3. Comparar RAM/VRAM/ARAM, SH-4/FPU, ARM7/DSP, timers, IRQ, DMA, scheduler y estado
   relevante de PVR/TA. Auditar invalidación del dynarec, texturas y alias.
4. Verificar que CPU, DMA y dispositivos quedan incluidos en el seguimiento de
   escrituras. Un dirty tracker de stores SH-4 por sí solo no es suficiente.
5. Comparar futuros estados tras restaurar; además, imagen final y audio guest.
   El audio host ya emitido no se puede deshacer: verificar aparte su política.

**Puerta:** cero divergencias inexplicadas en el dominio probado, con contadores
que demuestren las restauraciones y controles negativos. Un hash solo de actores
o RAM no acepta la integridad del snapshot. No afirmar determinismo universal.

### G3 — Mejoras de núcleo y memoria con A/B

Probar de una en una, solo si G1 identifica coste significativo:

| Hipótesis | Intervención candidata | Riesgo a comprobar |
|---|---|---|
| Asignaciones/copias de snapshots | Pool acotado, buffers preasignados, ring por tick | Vida útil, wrap, cargas repetidas y crecimiento de memoria. |
| Gestión de páginas | Layout compacto/índices y restauración eficiente | DMA, alias, VRAM/ARAM e invalidaciones. |
| Esperas en resimulación | Reducir sincronización/presentación host innecesaria | Mantener todo efecto PVR/TA observable por el guest. |
| Input/presentación | Sondeo en frontera definida y cola host acotada | Mismo input guest, pacing, tearing y respuesta real. |
| Audio | Cola y reconciliación de salida rollback | Conservar avance ARM7/DSP, sin duplicados ni cortes añadidos. |

No optimizar quitando IRQ, audio guest, DMA o actualizaciones del juego. No
reordenar structs guest ni reducir snapshots a vida/posición. Exclusiones de
estado solo tras demostrar reconstrucción y dependencia futura, pasando G2.

**Umbrales iniciales de aceptación, no resultados:**

- Rendimiento: reducción >=15 % del p95 del coste total de recuperación de
  4 frames (restore + resimulación + bookkeeping), IC95 % de mejora por encima
  de cero, y cómputo normal p99 sin empeorar >5 %.
- Respuesta como vía alternativa: reducción >=4 ms en mediana entrada-a-fotón,
  por encima de la incertidumbre del equipo y con IC95 % positivo; p95 no peor.
  Al menos 100 pulsos por condición, mismo mando/pantalla/punto de pantalla.
  Preferir LED/actuador + cámara >=1000 fps o sensor; 240 fps por sí solo no
  resuelve de forma concluyente una mejora de 4 ms. Sin equipo: puerta bloqueada,
  latencia software solo diagnóstica.
- En ambos casos: G2 y corpus G3 sin regresión, sin crecimiento sostenido de
  memoria en 2 h, audio/imagen conservados según contrato y hardware declarado.
- Medir `restore + N*step_sin_presentar + save/bookkeeping + presentación final`
  como ráfaga real. Objetivo: p99 <= T para N=4 en el equipo mínimo elegido en G0;
  si falla, declarar la ventana sostenible y no vender 8 frames como garantizados.

Los umbrales pueden revisarse antes del siguiente experimento con justificación
en el maestro; no se rebajan después para convertir un resultado negativo en PASS.
Si no aparece mejora, cerrar hipótesis con evidencia y volver al perfil. API y
staging de matchmaking son necesarios para validar cross-play; escalar el servicio
de producto sigue condicionado a demostrar ventaja. Aplicar además §4.3 en cada
destino obligatorio; un PASS Windows no cierra la optimización multiplataforma.

### G4 — Netcode bajo red controlada y entre dos máquinas

Conservar inicialmente GGPO. Añadir handshake de versión compatible/perfil, ROM/BIOS/estado
inicial, y hashes de estado **confirmado** del mismo tick; no comparar predicciones
locales distintas como si fueran desync. Emitir primera divergencia por subsistema.
Auditar qué aporta ya el transporte antes de cambiar redundancia/ACK o predicción.

Usar dos máquinas físicas y un router/puente Linux con `tc netem` (o equivalente
verificado). Guardar comandos, seed, distribución y captura para comprobar el
RTT/pérdida realmente aplicados. Describir delay por sentido y jitter; evitar
sumar el RTT completo en ambos sentidos.

| Perfil | RTT objetivo | Variación de delay por sentido | Pérdida |
|---|---:|---:|---:|
| LAN | medido, sin inyección | ninguna | ninguna |
| Regional | 40 ms | ±2 ms uniforme | 0 % |
| Distancia media | 80 ms | ±5 ms uniforme | 0,1 % independiente |
| Difícil | 120 ms | ±10 ms uniforme | 1 % independiente |
| Estrés | 180 ms | ±20 ms uniforme | ráfagas de 3 paquetes cada 10 s |

Añadir asimetría (20/60 ms de ida/vuelta), reordenamiento de 1 % y corte de 2 s
como ensayos separados. Registrar tráfico y red real, no solo configuración.
Fijar misma ventana/delay para el A/B primario; después comparar perfiles de
producto reportando su compromiso latencia/correcciones. No ganar fluidez
añadiendo delay oculto ni eliminando frames del juego.

**Ejecución:** 10 pares de partidas/replays de al menos 5 minutos por perfil,
alternando rol y build; registrar rollbacks, profundidad, stalls >T, duración
de espera de predicción, frames presentados, audio y hashes confirmados.
**Puerta online:** cero desync inexplicado en esas corridas; >=25 % menos stalls
>T por minuto en dos perfiles degradados preseleccionados con baseline no nulo,
IC95 % positivo, delay igual y p95 entrada-a-fotón no peor más allá de la
incertidumbre medida. Si el baseline ya no tiene stalls, elegir y preregistrar
otra métrica antes del experimento; no atribuir una reducción ficticia.
LAN/regional sin regresión; estrés documenta límite y desconexión ordenada, no
promesa de juego fluido a cualquier distancia.

### G5 — Producto online mínimo y pruebas humanas

Entregar el servidor propio de matchmaking de §4.2, cola pública, salas privadas,
descubrimiento/NAT traversal y relay de respaldo;
mantener conexión directa para rutas favorables. Medir P2P/relay por separado.
Replays con identidad/inputs confirmados, espectadores fuera de la ruta crítica,
diagnóstico exportable y rechazo claro de versiones incompatibles.
Ranking público y validación de resultados quedan después del piloto, no son
evidencia de mejor netcode.

**Puerta:** al menos 20 sets de FT5 entre personas en >=3 pares de ubicaciones
reales y >=2 clases de hardware, con sesiones base/candidato en orden alternado,
registro de red, feedback y métricas. Dos horas continuas de estabilidad y corpus
ampliado aprobado; cero crash/desync inexplicado observado. Internet variable no
sustituye el A/B controlado de G4. Si faltan jugadores/equipos, marcar BLOCKED.
Además, aprobar la matriz cross-play y servicio de §4.1–4.2; incluir Android y
Switch reales en los sets humanos. El servicio común es requisito de entrega.

### G6 — Entrega y sostenibilidad

Build reproducible instalable fuera del repositorio, configuración competitiva
versionada, informe público de resultados y límites, prueba de actualización de
upstream en rama y regresiones del corpus. Entregar paquetes para Windows/Linux,
APK Android y paquete Switch del entorno definido, con conexión al mismo servicio.
Revalidar cada backend/plataforma; Linux/Android/Switch no se aceptan por compilar
Windows. macOS y otros destinos mantienen su ficha y estado explícito.
**Puerta:** otra máquina reproduce instalación, replay y al menos un resultado
primario usando el paquete de evidencia. Las mediciones deben sobrevivir al build
distribuido, no solo a un ejecutable privado de laboratorio.
No cerrar G6 sin aceptación de las cuatro plataformas obligatorias, sus recursos
y cross-play. Documentar operación/backup/actualización del matchmaking y prueba
de compatibilidad del protocolo durante despliegues.

## 7. Decisiones go/no-go

- Tras G0/G1: si no hay reproducción o falta instrumentación, desarrollar el
  laboratorio. No atribuir fallos al netcode sin separar carga local y red.
- Tras G2: si no restaura correctamente, corregir integridad antes de optimizar.
- Tras G3: adoptar el fork como frente de producto solo con mejora demostrada;
  de lo contrario mantenerlo experimental y registrar qué hipótesis fallaron.
- Tras G4/G5: promover online solamente con beneficios reproducidos y estabilidad.
- En cada cierre actualizar maestro y enlazar informe; inventario de funciones
  únicamente si cambian sus estados. No contar este plan como recuperación C.

## 8. Puerta H: mayor fidelidad y alcance Atomiswave

Abrir H cuando G0 o el corpus detecte un defecto concreto de XI: temporización,
gráficos, audio, inputs o dispositivos. Guardar reproducción mínima y contraste.
Prioridad de referencia: placa AW identificada con misma revisión/ajustes;
después documentación/pruebas de hardware independientes pertinentes. Otro
emulador puede señalar discrepancias, no decide automáticamente cuál es correcto.

Capturar vídeo/audio y timing de placa con incertidumbre declarada para lo
observable; esas capturas no certifican RAM/IRQ internos. Alinear inputs y eventos
sin normalizar diferencias que sean precisamente el fallo. Exigir que el caso
falle antes, pase después y conserve los controles del corpus.

Para afirmar mejora **general de Atomiswave**, añadir al menos tres títulos AW
distintos de XI, elegidos por los dispositivos/rutas afectados, con referencias
adecuadas y sesiones completas en ese alcance. La aceptación sigue limitada a
esos títulos/rutas, no a todo el catálogo. Sin placa o evidencia independiente,
H queda BLOCKED y solo se declara rendimiento/manejo mejorado frente a la base.

## 9. Primer lote ejecutable

**FCT-00 = G0 + preparación de G1**, sin optimizaciones todavía:

1. Localizar y fijar upstream/Dojo, versiones, toolchain y equipo de prueba.
2. Repetir preparación aislada con el script existente y ejecutar el probe;
   guardar hashes y verificar selección natural en la copia.
3. Compilar upstream, reproducir el recorrido a combate/resultado y medir una
   primera muestra exploratoria; esta muestra selecciona hipótesis, no las acepta.
4. Documentar diferencias de input/tick/config y las interfaces faltantes para
   replay/snapshot/medición; implementar el adaptador mínimo en el siguiente lote.
5. Entregar tabla base-versión-equipo-recorrido-métrica-artefacto-límite y escoger
   el primer cuello de botella a medir con G1.
6. Entregar matriz Windows/Linux/Android/Switch (más destinos de extensión),
   equipos/toolchains disponibles y bloqueos; inventario del matchmaking y diseño
   del endpoint común. Preparar el baseline de recursos por dispositivo y el
   primer replay cruzado x86-64/AArch64, no posponerlos hasta finalizar Windows.

No hay plazo prometido antes de conocer build, determinismo y coste real. El
avance se mide por puertas y evidencia entregada, incluyendo resultados negativos.

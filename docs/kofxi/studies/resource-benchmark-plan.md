# Benchmark propuesto: cartucho, G1ROM y recursos

Fecha: 2026-10-28. Estado: **plan, no resultados**. Prioridad gobernada por
[roadmap rector](../../roadmap.md); evidencia inicial en [estudio AW](aw-rom-processing.md).

## Objetivo y coordinación

Reducir CPU host por frame y RAM para disponer de margen de rollback. Comparar
representaciones de ROM y recursos, y determinar si basta cambiar el backend del
cartucho o hace falta interceptar/modificar loaders del ejecutable guest.
Acortar cargas se informa por separado de reducir coste durante combate.

Responsable de esta propuesta: agente de esta sesión, frente T05. Dependencias:
T01 (identidad/replay), T00 (hooks/contratos), K01 (formatos), K02 (loaders), T08
(estado/rollback). Rutas propuestas: `benchmarks/resources/` para herramientas,
`core/hw/naomi/` para diagnóstico/backend y `docs/kofxi/studies/` para evidencia.
Scheduler, dynarec y serialización requieren coordinación antes de editar.
Este documento no autoriza un driver de producción ni parches del juego.

## Datos conocidos y pendientes

- Cartucho actual: 320 MiB de asignación; chips catalogados: 232 MiB. Los 88 MiB
  de diferencia son espacio lógico sin payload catalogado, no ahorro RAM probado.
- `decrypted/`: ocho chips descifrados; no recursos descomprimidos.
- El inventario local de `fs_extracted/` cuenta 1.058 archivos **incluyendo**
  `_manifest.txt`: 1.057 payloads. Total 230.591.080 bytes; manifiesto 57.169 bytes;
  payload 230.533.911 bytes. Identidad contra chips y ejecución aún no verificada.
- Extraer G1ROM no elimina compresión interna. Reemplazar un `.lz` por su salida
  en DMA rompería el contrato del consumidor que espera datos comprimidos.
- Flycast ya sirve DMA desde memoria host. Archivos sueltos no garantizan menos I/O.
- Precarga host no reduce por sí sola ciclos guest de DMA ni tiempo de carga guest.
- Hay una contradicción documental que resolver: `formats/lzss.md` transcribe un
  decoder de ventana de 4096 bytes, pero la actualización `DECORD_TASK` del 22-sep
  en `kofxi_aw_modding/docs/plans/plan_experimentos_lzss_recursos.md` lo reemplaza.
  Validar implementación vigente y ejecución identificada antes de generar assets.

## Matriz de alternativas

| ID | Fuente y ruta | ¿Cambiar ejecutable? | Hipótesis |
|---|---|---|---|
| A | AWCartridge y ROM original | No | Control |
| B | Chips cifrados segmentados/mapeados, huecos virtuales | No | Menos RAM; aislar almacenamiento del descifrado |
| C | Chips `.dec` segmentados/mapeados con DMA directo | No, si conserva interfaz AW | Evitar decrypt con coste de lookup/lectura menor |
| D | `fs_extracted` + mapa de intervalos + EPR/tablas/fallback | No, si reconstruye exactamente la vista DMA | Servir archivos por offset sin cartucho heap completo |
| E | Caché host de salidas descomprimidas | Intercepción del decoder guest; driver DMA solo no basta | Evitar trabajo SH-4 de recursos |
| F | Ejecutable parcheado y API/loader para recursos procesados | Sí, perfil e identidad distintos | Evaluar máximo ahorro potencial y coste de modificar el juego |

D debe conservar directorios, vacíos, tablas, padding, bancos, lecturas parciales
y cruces de intervalos. El manifiesto recupera parte del mapa: verificar cobertura,
no asumir que perdió toda la información. Mantener fallback para rangos desconocidos
y PIO crudo, contabilizando su memoria. No abrir archivos por cada bloque de 32 bytes;
comparar índice precalculado y caché/mapeo acotado.

E debe conservar registros, memoria temporal observable, allocator, yields, errores,
interrupciones y ciclos, además de los bytes finales. F es juego modificado y no
acredita equivalencia vanilla por presentar la misma imagen. Los recursos pueden
llegar comprimidos por DMA y decodificarse después: elegir bien el punto de intervención.

## Fase 1: identidad, cobertura y perfil

1. Fijar commit/diff, EXE/PDB y hashes, compilador/opciones, SO/driver, equipo,
   energía, renderer/resolución/VSync/audio, ROM/BIOS/NVRAM/región/idioma e inputs.
2. Auditar extractor y manifest; comparar **1.057 payloads** contra sus intervalos
   MPR y los ocho chips contra descifrado de referencia. Reportar cambios/huecos,
   truncamientos y cobertura; distinguir assets modificados de errores del backend.
3. Traza por transferencia: offset/banco/modo, bytes, destino, ciclo guest, tiempo
   host y recurso asociado. Contar bytes únicos/totales/repetidos y cruces. Separar
   Init, boot, metadatos y PIO del DMA de payload.
4. Resolver G1ROM → DMA → buffer → decoder → destino para UNI, `.lz` gráfico y
   audio/streaming. Confirmar solicitudes reales durante carga y combate; direcciones
   históricas son pistas de una build, no hooks estables.
5. Perfil global CPU: SH-4/dynarec, render/texturas, audio, cartucho, copia DMA y
   memoria. Si otro hotspot domina, reportarlo; no forzar una solución de ROM.

El timer AW actual muestrea 1/64 bucles, acumula al cerrar y mide elapsed, no CPU
exclusivo. No separa escenas ni mide decoder guest. Añadir límites de fase y medir
overhead antes de usarlo como benchmark. ETW/PDB ayudan con código host; atribuir
código SH-4 generado requiere mapa de bloques o trazas guest adicionales.

## Fase 2: receta numérica

Las siguientes cifras son **diseño del ensayo**, no resultados ni aceptación G3.

| Ensayo | Cantidad y condición |
|---|---|
| Exploratorio A | 3 arranques y 3 recorridos de 300 s de combate |
| Escenas | Inicio→selección, selección→primer control, combate, cambio de equipo/stage, revancha |
| Piloto | 3 pares A/candidato por recorrido |
| A/B local | 10 pares por candidato superviviente: 5 AB y 5 BA, orden fijado previamente |
| Caché | Separar primera ejecución y caliente; reiniciar Flycast no acredita caché de disco fría |
| Memoria | Muestrear cada 100 ms + high-water; 60 min del superviviente para detectar deriva |
| Generación | 3 corridas por formato: CPU/elapsed, pico RAM, bytes I/O, disco lógico/asignado |
| Caché E | Límites 0, 8, 32 y 64 MiB; hit/miss/evicción y RAM total |

T01 fija replay, estado inicial, seis personajes y stage, ataques/efectos, cambios
y audio activo. Sin replay verificable, las partidas manuales son exploratorias,
no A/B formal. Separar fases; no diluir coste de carga en media de combate. No
ejecutar builds u otros benchmarks simultáneos a las capturas.

Métricas CPU: ms host/frame totales y por hilo/subsistema, tiempo de cómputo
mediana/p95/p99, velocidad guest/real, frames fuera de plazo y underruns de audio.
RAM: Working Set, private working set donde exista, Private Bytes/commit, picos,
file-backed, fallos de página y VRAM. Desglosar cartucho, snapshots, texturas,
dynarec y audio; evitar doble conteo de páginas compartidas.

Rollback: medir tamaño/tiempo de guardar y restaurar; con adaptador T08 validado,
ráfagas de 2, 4 y 8 frames resimulados, 1.000 eventos por profundidad. Medir CPU
por ráfaga y p99 de recuperación. Sin adaptador: BLOCKED. No extrapolar CPU total
a coste rollback: render/audio suprimidos cambian la distribución del trabajo.

## Fase 3: gates y equivalencia

- Overhead diagnóstico objetivo <=2 % CPU/frame. Si lo supera, reducir medición
  o usarla solo para atribución y repetir rendimiento sin instrumentación.
- Gate CPU para prototipo: coste evitable >=5 % CPU de combate o picos atribuibles
  >=1 ms/frame en p99. Gate RAM independiente: hipótesis verificable de >=32 MiB
  menos memoria sin duplicación equivalente en otra categoría.
- A/B: delta pareado, mediana e intervalo bootstrap 95 %. No afirmar mejora si
  cruza cero. Objetivo piloto >=5 % CPU o >=32 MiB RAM, sin regresión p99 >2 %,
  deriva sostenida ni divergencias. Estos gates no reemplazan el plan rector.
- Comparar bytes de todas las lecturas capturadas, límites de banco/segmento,
  padding, accesos parciales y PIO. Hashes guest por frame y antes/después de carga;
  auditar qué estado cubren. Cero divergencias en el corpus requerido.
- Save/restore y rollback con caché vacía/caliente/evictada: la caché host descartable
  no debe alterar resultado ni timing guest.
- Expandir a x86-64/ARM64 y Windows/Linux/Android/Switch antes de aceptación global;
  el piloto Windows no demuestra cross-play ni fidelidad de hardware.

## Estimación de esfuerzo

Rangos preliminares de **horas-persona**, un desarrollador familiarizado con el core.
No son mediciones/promesas. Excluyen espera de hardware/usuario, ejecución prolongada
de matrices, portabilidad completa y mantenimiento. Revisar después de fase 1.

| Trabajo | Horas | Incertidumbre dominante |
|---|---:|---|
| Identidad, corpus y mapa | 8–16 | Contenido modificado y huecos |
| Perfil global, trazas y receta | 16–32 | Replay y atribución guest |
| B: backend experimental segmentado | 12–24 | RomPtr y lifetime |
| C: backend `.dec` y manifest/generación | 16–32 | PIO, identidad y rangos |
| D: backend extraído y fallback | 24–48 | Mapa físico y cruces |
| E: resolver decoder/caché equivalente | 40–80 | ABI, tareas y efectos observables |
| F: parche ejecutable/API | 80–160 | Loaders múltiples y perfil modificado |
| Validación por candidato B–D | 16–32 | Corpus, A/B y controles |
| Validación por candidato E/F | 40–80 | Timing y consumidores |

Primera inversión propuesta: **24–48 h de investigación**, después seleccionar
por evidencia. B–F son alternativas, no un total comprometido. Registrar horas
reales de análisis/código/debug/tests/docs, archivos/subsistemas/líneas modificadas,
hooks y parches guest. Separar horas humanas de build/generación/corridas.

## Entrega y decisión

Tabla A–F: CPU/frame, p99, RAM/pico, carga, disco/generación, equivalencia, horas
reales, esfuerzo restante y decisión seguir/descartar/bloqueado. Para resultados
no ejecutados usar `pendiente`, nunca cero. Adjuntar receta, hashes, CSV agregados
y cobertura, sin ROM/BIOS ni recursos procesados.

Preferir backend alternativo cuando preserve la interfaz AW. Estudiar intercepción
o patch guest solo si el perfil identifica trabajo material que no puede evitarse
en el driver y se acuerda el contrato de estado/compatibilidad correspondiente.

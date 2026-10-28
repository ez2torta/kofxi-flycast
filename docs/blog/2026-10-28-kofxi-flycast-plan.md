# Un Flycast pensado para KOF XI: emulación, online y herramientas de entrenamiento

*Borrador de blogpost — 28 de octubre de 2026*

Quiero empezar un proyecto nuevo alrededor de KOF XI. No porque Flycast parta
de cero: justamente lo contrario. Flycast ya emula Atomiswave y tiene netplay y
rollback. La idea es tomar esa base, conservar su historia y trabajar en una
versión especializada donde cada mejora se gane con pruebas, no con promesas.

## Primero, que funcione mejor; después, decir cuánto

El proyecto nace con una pregunta concreta: ¿podemos hacer que KOF XI se sienta
más responsivo, funcione de forma más estable y consuma menos recursos sin cambiar
el comportamiento del juego?

Para contestarla vamos a comparar builds identificadas, en los mismos equipos,
con los mismos recorridos y controles. Mediremos memoria, GPU, tiempo por frame,
audio, rollback, latencia de entrada e incidencias de red. Si no hay mejora
reproducible, no la llamaremos mejora. Si una prueba cubre un solo escenario,
diremos cuál y no extrapolaremos al juego entero.

Eso implica empezar por reproducir una partida y construir un laboratorio. Antes
de acelerar una restauración de rollback hay que demostrar que restaurar y volver
a simular produce el mismo futuro. Antes de presumir menos input lag hay que
medir desde el control hasta la imagen, no solo contar milisegundos dentro del
código.

## Multiplataforma desde el diseño

Windows, Linux, Android y Nintendo Switch forman parte del objetivo, junto con
partidas cross-play y un servidor de matchmaking propio. macOS y otros destinos
pueden sumarse con una matriz de aceptación explícita.

No alcanza con que compile. Cada plataforma necesitará una build utilizable,
controles correctos, red compatible, mediciones de recursos y partidas reales.
También queremos comparar x86-64 y ARM64 para descubrir pronto si una diferencia
de arquitectura rompe la sincronización.

El matchmaking organizará jugadores y salas; la partida buscará conexión directa
y podrá usar relay cuando sea necesario. Son responsabilidades distintas. El
servidor de salas no tiene por qué retrasar cada input de una pelea.

## Que el cliente sea liviano y conserve el ritmo

Un emulador puede estar correctamente sincronizado y aun sentirse pesado. Por
eso mediremos RAM, GPU, colas de presentación, buffers de audio, temperatura y
respuesta de controles. Android y Switch requieren pruebas sostenidas: un buen
primer minuto no basta si el dispositivo se ralentiza por temperatura.

El objetivo no es hacer que el juego corra más rápido que la placa Atomiswave ni
quitar trabajo de emulación para alcanzar una cifra. Queremos que mantenga el
ritmo original de forma estable. Afirmar fidelidad frente al hardware real
requerirá una comparación aparte; coincidir con otro emulador no demuestra por
sí solo cuál comportamiento es correcto.

## El conocimiento de KOF XI como herramienta

Además del netplay, el proyecto puede aprovechar lo que ya sabemos del juego:
formatos UNI y PVR, recursos comprimidos, estructuras de jugadores, animaciones,
acciones y hitboxes. Ese conocimiento tendrá fichas con versión investigada,
fuente, método y grado de confianza. Una dirección observada en una build no se
convertirá sin más en una constante universal.

La separación importa: el núcleo debe seguir siendo un emulador de Atomiswave.
Las funciones específicas de KOF XI pueden vivir en un perfil de juego y ofrecer
overlays de hitboxes, frame data, visualización de inputs, replays y training.
Empezaremos por herramientas de solo lectura. Si luego el entrenamiento escribe
memoria o modifica estados, será opt-in, visible y separado de las partidas
competitivas vanilla.

## Un plan que se puede repartir

El repositorio parte del historial de `flyinghead/flycast`, con el original como
upstream. El roadmap define las puertas del proyecto; el backlog divide el trabajo
en contratos, laboratorio, plataformas, recursos, latencia, matchmaking,
determinismo, optimizaciones y empaquetado.

Hay frentes que pueden avanzar en paralelo, pero no todo es independiente. Primero
hay que acordar cómo se identifican ticks, inputs y estados compartidos. Con ese
contrato, distintas personas pueden estudiar Android, Switch, Linux,
instrumentación o backend sin inventar interfaces incompatibles.

## Sin atajos en las afirmaciones

El repositorio es el inicio, no el resultado. Todavía no hay mejoras propias
medidas, servidor desplegado ni aceptación de builds en las cuatro plataformas.
Algunas pruebas dependen de hardware y equipos que habrá que reunir. Registraremos
también resultados negativos y bloqueos; una tarea bloqueada no desaparece del
alcance.

La meta es que jugar KOF XI online sea sencillo y que el emulador responda bien
incluso en equipos modestos. También queremos construir herramientas que ayuden
a entender el juego y entrenar mejor. Para llegar ahí, cada paso tendrá una
receta que otra persona pueda repetir y un resultado que se pueda discutir.

*Este borrador describe el plan y las metas del proyecto; no afirma que las
mejoras, el matchmaking o las features de entrenamiento ya estén implementados.*

---

**Referencias del proyecto:** `docs/roadmap.md`,
`docs/plans/plan-flycast-kofxi-online.md`, `docs/tasks/backlog.md` y `docs/kofxi/`.

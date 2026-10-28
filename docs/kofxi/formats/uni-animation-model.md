# Animaciones de personajes UNI — cadena estructural

- **Estado del traspaso:** `IN_REVIEW`; no es una implementación Flycast.
- **Fuente:** `SRC-3c505df8fab8`, `docs/architecture/formats/uni-animations.md`.
  Fuente previa/correcciones: `SRC-030e72078152`, `docs/architecture/formats/uni.md`.
- **Dominio reportado:** análisis end-to-end histórico de UNI y framecap de Ash,
  Oswald y Shen Woo; el documento señala que el framecap no está disponible en
  ese checkout, por lo que el resultado citado no se reproduce aquí.
- **No demuestra:** cobertura de todo el roster/build/región, reproducción exacta
  de timings de combate, ni validez de observaciones de memoria PS2 para AW.

## Modelo reportado

La fuente describe la relación:

```text
actionID
  -> Sec19 (categorías / animDataPtr)
  -> Sec5 (despacho de acción a Sec4)
  -> script Sec4 (incluye comandos de animación)
  -> Sec10 (tabla animID -> secuencia de frames visuales)
```

Sec10 se modela como tabla de offsets relativos al inicio de sección. La entrada
de animación se localiza con tabla `[animID+1]`; tamaño derivado de offsets
adyacentes cuando son válidos. La fuente reporta registros base de 20 bytes más
bloques variables, incluyendo registros asociados a hitboxes. Nunca asumir que
la capacidad siempre es 1024 ni que los offsets alias son animaciones distintas.

La fuente advierte explícitamente que modelos previos de Sec10 fueron refutados:
no tratarla como una tabla plana de 1027 frames ni aplicar el desfase del análisis
antiguo. Campos de offsets de dibujo `+12/+14` se describen como enteros con
signo de 10 bits; `+16` tiene packed fields de frame/flags/descriptor según el
modelo actual. Consultar el origen completo para masks, variantes y excepciones;
esta síntesis no reemplaza el contrato binario campo por campo.

## Evidencia y alcance

El origen reporta 142/142 episodios históricos de acción reproduciendo frames
observados para tres personajes. El propio informe aclara que faltan los framecaps
para repetición independiente local. Por ello este traspaso usa estado `OBSERVED`
en su ledger para esos claims, no `VERIFIED` por nosotros. El modelo es candidato
fuerte para un asset inspector, pero requiere fixtures/hash/CLI y tests de bounds.

Para frame data competitivo hace falta adicionalmente relacionar frame visual,
duración guest, hitstop y tick de simulación. Un frame de animación no equivale
necesariamente a un frame de combate. Los offsets de player +0x2A4 y otros datos
runtime se documentan por separado y deben segmentar AW/PS2.

# Compresión LZSS de recursos `.lz`

- **Estado del traspaso:** `IN_REVIEW`.
- **Fuente:** `SRC-750b6de7ea1d`, `docs/architecture/formats/lzss.md`.
- **Afirmación reportada:** variante LZSS de KOF XI con flags LSB-first, ventana
  de 4096 bytes, posición inicial `0xFEE`, referencias de offset absoluto y
  longitud codificada en 4 bits más 3.
- **Evidencia histórica citada:** 12 archivos contrastados byte a byte con `.dec`
  de referencia y 301/301 archivos `.lz` del filesystem extraído, según el doc.
  Fixtures/resultados no están en este repo y no se han vuelto a ejecutar aquí.
- **No demuestra:** que cada `.lz` tenga el mismo envoltorio, que todos los
  recursos sean texturas, ni que el decoder sea seguro ante entradas malformadas.

## Parámetros que reporta el origen

| Propiedad | Valor reportado |
|---|---|
| Flags | Byte de control, bit menos significativo primero. |
| Bit 1 | Literal. |
| Bit 0 | Back-reference de dos bytes. |
| Ventana | 4096 bytes. |
| Posición inicial | `0xFEE`. |
| Offset | 12 bits, posición absoluta dentro de la ventana. |
| Largo | nibble bajo + 3; rango 3–18. |
| Codificación reportada | `b0 = offset & 0xff`; `b1 = ((offset >> 4) & 0xf0) | ((length - 3) & 0x0f)`. |

El documento contrasta esta variante con codificaciones relativas atribuidas a
otros juegos y reporta que esas alternativas no coincidieron con los 12 fixtures.
No cambiar offset absoluto por distancia relativa sin nuevo corpus que lo exija.

## Port seguro y prueba pendiente

Decoder candidato debe limitar bytes consumidos/producidos, validar backreference
antes de leer, definir comportamiento de referencias a bytes aún no emitidos,
rechazar tamaños declarados excesivos, y evitar integer overflow/loops sin
progreso. Tests: literal único, wrap de ventana, longitudes 3/18, offset 0/4095,
salida truncada, input truncado, tamaño declarado cero/máximo, backreference
inválido y corpus positivo pequeño con hash. Ejecutar diferencial contra decoder
origen en fixtures permitidos. La investigación del algoritmo no autoriza copiar
`src/lz_tool.py`; K01 revisa su licencia y tests por separado.

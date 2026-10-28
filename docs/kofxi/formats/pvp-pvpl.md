# Paletas PVP/PVPL y texturas `.lz` asociadas

- **Estado del traspaso:** `IN_REVIEW`.
- **Fuente:** `SRC-1b917a6f52d1`, `docs/architecture/formats/lz-pvp.md`.
- **Dominio reportado:** grupo de assets tutorial/demo How to Play de Ash; no
  asumir que describe paletas de combate o todos los archivos PVP.
- **No demuestra:** render correcto de Flycast/PVR, colorimetría final, ni
  semántica universal de cada valor del header.

## Estructura reportada

Para una muestra citada, PVP/PVPL tiene magic ASCII `PVPL`, 12 bytes de header
después de los cuatro bytes de magic, y 256 entradas ARGB1555 de 16 bits: total
528 bytes. La fuente diferencia este caso de PAL crudo usado para sprites de pelea
(sub-paletas 4bpp); no mezclar ambos contenedores por compartir color packing.

El grupo ilustrativo menciona archivos `.lz` 8bpp 512x256 y una paleta compartida;
eso caracteriza esos assets del tutorial, no el formato de textura general.
La fuente LZSS correspondiente está en [`lzss.md`](lzss.md).

## Validación requerida

Antes de parser: comprobar magic, tamaño mínimo/exacto cuando sea válido, bytes de
header, endianess de colores, alpha bit, cardinalidad y comportamiento ante
truncado/extra bytes. Para render, usar fixture permitido con imagen esperada,
orientación, stride y backend fijados. La mención de colores no usados/chroma-key
es observación de la muestra, no regla del formato.

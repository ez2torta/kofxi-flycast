# Procedencia del repositorio y del plan

Fecha: 2026-10-28. Clon completo de https://github.com/flyinghead/flycast.git,
upstream master `1b63c8a3677cc8eb1292863c4fe8fc8f94674fc8`.
Rama propia `kofxi-main`; remoto `upstream`, sin `origin` todavía.
Historial y archivos de licencias se conservan. Submódulos referenciados por
upstream, no inicializados en esta entrega documental.

Plan importado de `E:/github/kofxi_aw_modding/docs/plans/plan-flycast-kofxi-online.md`
el 28-oct; incluye ampliación multiplataforma/matchmaking. Esa versión estaba en
el árbol de trabajo, sin commit propio: no se inventa un SHA de procedencia.
Se adapta su autoridad al roadmap de este repo, manteniendo sus puertas.

Documentos históricos a consultar en `kofxi_aw_modding`:

- `docs/research/runtime/flycast-dojo-reference-2026-10-22.md`: probes Lua y
  selección automatizada; contador Dojo no equivale a vblank en todo el arranque.
- `docs/research/runtime/r05-windows-performance-baseline-2026-10-27.md`:
  rendimiento del recomp, no benchmark de este fork.
- `docs/research/runtime/flycast-kofxi-feasibility-plan-2026-10-28.md`:
  decisión inicial y límites de consultas a ramas upstream mutables.
- `docs/research/runtime/flycast-kofxi-multiplatform-scope-2026-10-28.md`:
  requisitos multiplataforma y de recursos.

Las rutas son referencias al repositorio de investigación, no archivos presentes
en este checkout. Scripts/corpus no importados; T01/T05 auditan antes de portarlos.
No se transfieren ROM, BIOS, saves ni snapshots del usuario.

## Resultado inicial

Código upstream disponible y documentación de proyecto preparada. No se ejecutó
compilación ni partida ni benchmark; servidor sin implementar. No se creó remoto
GitHub, commit propio ni push. Los checks documentales no acreditan G0 ni G1.

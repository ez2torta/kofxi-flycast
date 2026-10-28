# Registro de migración KOF XI

Fuente primaria: checkout externo `kofxi_aw_modding`. Procedimiento:
[`../MIGRATION-PLAYBOOK.md`](../MIGRATION-PLAYBOOK.md).

## Estado inicial

El playbook y el generador portable están preparados; M00 tiene inventario
descubierto; M01 está en primera revisión y M02 en triage bloqueado. Hay cuatro
fichas de formato, una ficha runtime y once claims transcritos, todavía no
revalidados independientemente. Existe un índice preliminar en
[`../research-index.md`](../research-index.md), pero no equivale a inventario ni
acepta sus claims.

## Archivos del registro

- `source-manifest.csv`: una fila por fuente candidata.
- `claim-ledger.csv`: una fila por afirmación que vaya a consumirse.
- `generated-summary.md`: conteos derivados; no editar manualmente.
- `decisions.md`: exclusiones, contradicciones, supersesión y bloqueos.

Generar L0 desde el root del fork:

```powershell
python scripts/build_knowledge_manifest.py --source-root E:\github\kofxi_aw_modding
```

El script indexa `docs/**/*.md`, `docs/**/*.json`, `scripts/**/*.py` y
`src/**/*.py`: calcula SHA-256, bytes, categoría y título Markdown; no copia
contenidos. Revisa el diff y recuenta después. La revisión del repo origen se
deja vacía deliberadamente porque contiene cambios locales al preparar el plan;
rellenarla cuando se fije una revisión común. La lista recién generada queda
`TODO/UNTRIAGED`; no significa que cada fuente sea pertinente o redistribuible.
El resumen por categoría se regenera en `generated-summary.md`; no incluye
rutas absolutas del equipo.

No crear CSV parcialmente poblados y presentarlos como inventario completo.
M00 debe registrar fecha, raíz, criterios de descubrimiento, patrones excluidos,
cantidad por clase y SHA-256. Paths permanecen relativos al origen y se conserva
la disponibilidad necesaria para que otro colaborador los pueda consultar.

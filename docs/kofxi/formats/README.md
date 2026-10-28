# Formatos de recursos KOF XI — primer traspaso

Estas páginas son una síntesis fiel de fuentes del repo `kofxi_aw_modding`, no
una reimplementación ni una validación independiente del fork. Claims atómicos,
hashes y estado de migración están en
[`../migration/claim-ledger.csv`](../migration/claim-ledger.csv) y
[`../migration/source-manifest.csv`](../migration/source-manifest.csv).

| Formato | Ficha | Fuente canónica de origen | Uso candidato |
|---|---|---|---|
| UNI | [Contenedor y secciones](uni-container.md) | `docs/architecture/formats/uni.md` | Indexar assets/personaje de forma segura. |
| Animaciones dentro de UNI | [Cadena de animación](uni-animation-model.md) | `docs/architecture/formats/uni-animations.md` | Frame data y overlay; no equivale a timing completo de gameplay. |
| LZSS `.lz` | [Decodificación](lzss.md) | `docs/architecture/formats/lzss.md` | Asset inspector y tests del decoder. |
| PVP/PVPL | [Paleta de tutorial](pvp-pvpl.md) | `docs/architecture/formats/lz-pvp.md` | Visualización de ciertos assets UI/tutorial. |

**Estado conjunto:** `IN_REVIEW`. Las pruebas numéricas son resultados reportados
por la fuente histórica. El checkout del origen no tiene una revisión Git limpia
fijada en esta captura y los fixtures privados no se copiaron; la evidencia no se
presenta como reejecutada por Flycast. Cada formato debe pasar pruebas localmente
con fixtures pequeños permitidos antes de que un decoder se integre al producto.

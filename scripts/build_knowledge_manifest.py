#!/usr/bin/env python3
"""Inventory knowledge-source files without copying their contents.

Example:
  python scripts/build_knowledge_manifest.py --source-root E:/github/kofxi_aw_modding

The CSV is a discovery list, not a migration approval. Review generated paths,
hashes, licensing, sensitivity, relevance, and status before consuming anything.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
from pathlib import Path


PATTERNS = ("docs/**/*.md", "docs/**/*.json", "scripts/**/*.py", "src/**/*.py")
FIELDS = (
    "source_id", "source_path", "sha256", "size_bytes", "category", "title",
    "source_revision", "platform_game_revision", "evidence_type", "claim_ids",
    "priority", "migration_status", "destination_paths", "related_sources",
    "license_review", "exclusion_or_blocker", "reviewer", "review_date",
)
def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def category(relative: str) -> str:
    parts = Path(relative).parts
    if parts[0] == "docs" and len(parts) > 1:
        return "/".join(parts[1:-1]) or "docs"
    return parts[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument(
        "--output", type=Path,
        default=Path("docs/kofxi/migration/source-manifest.csv"),
    )
    parser.add_argument(
        "--summary", type=Path,
        default=Path("docs/kofxi/migration/generated-summary.md"),
    )
    args = parser.parse_args()
    root = args.source_root.resolve()
    if not root.is_dir():
        parser.error(f"source root is not a directory: {root}")

    candidates: dict[str, Path] = {}
    for pattern in PATTERNS:
        for path in root.glob(pattern):
            if path.is_file():
                candidates[path.relative_to(root).as_posix()] = path

    output = args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    previous: dict[str, dict[str, str]] = {}
    if output.is_file():
        with output.open("r", encoding="utf-8-sig", newline="") as stream:
            previous = {row["source_path"]: row for row in csv.DictReader(stream)}
    counts: Counter[str] = Counter()
    statuses: Counter[str] = Counter()
    with output.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        for relative in sorted(candidates):
            path = candidates[relative]
            title = ""
            if path.suffix.lower() == ".md":
                try:
                    for line in path.read_text(encoding="utf-8").splitlines():
                        if line.startswith("# "):
                            title = line[2:].strip()
                            break
                except (UnicodeDecodeError, OSError):
                    title = "[title unavailable]"
            writer.writerow({
                # Stable across inventory insertions/reordering.
                "source_id": "SRC-" + hashlib.sha256(relative.encode("utf-8")).hexdigest()[:12],
                "source_path": relative,
                "sha256": sha256(path),
                "size_bytes": path.stat().st_size,
                "category": category(relative),
                "title": title,
                **{
                    key: previous.get(relative, {}).get(key, default)
                    for key, default in {
                        "source_revision": "",  # fill with source commit if clean/published
                        "platform_game_revision": "",
                        "evidence_type": "",
                        "claim_ids": "",
                        "priority": "UNTRIAGED",
                        "migration_status": "TODO",
                        "destination_paths": "",
                        "related_sources": "",
                        "license_review": "PENDING",
                        "exclusion_or_blocker": "",
                        "reviewer": "",
                        "review_date": "UNREVIEWED",
                    }.items()
                },
            })
            counts[category(relative)] += 1
            statuses[previous.get(relative, {}).get("migration_status", "TODO")] += 1
    summary = args.summary
    summary.parent.mkdir(parents=True, exist_ok=True)
    with summary.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# Resumen generado del inventario de conocimiento\n\n")
        stream.write("Generado por `scripts/build_knowledge_manifest.py`. No editar a mano.\n\n")
        stream.write(f"- Root fuente etiquetado: `{root.name}`; la ruta absoluta no se incluye para mantener portabilidad.\n")
        stream.write(f"- Archivos indexados: **{len(candidates)}**.\n")
        stream.write("- Estado por fila (conservado desde el manifiesto anterior si existe):\n")
        for status, count in sorted(statuses.items()):
            stream.write(f"  - `{status}`: {count}\n")
        stream.write("- Las revisiones fuente están vacías hasta fijar un commit común.\n\n")
        stream.write("| Categoría | Archivos |\n|---|---:|\n")
        for name, count in sorted(counts.items()):
            stream.write(f"| `{name}` | {count} |\n")
        stream.write("\nLos hashes identifican bytes observados en esta ejecución, incluidos cambios locales. ")
        stream.write("El manifiesto es inventario de descubrimiento, no decisión de migración.\n")
    print(f"Indexed {len(candidates)} files from {root} into {output}")
    print(f"Wrote category summary to {summary}")
    print("Discovery only: no source contents copied and no claims approved.")


if __name__ == "__main__":
    main()

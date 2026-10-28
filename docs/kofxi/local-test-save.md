# Save local de prueba KOF XI

El usuario autorizó versionar una única NVRAM para pruebas locales:
[`tests/fixtures/kofxi/unlocked-spanish.nvmem`](../../tests/fixtures/kofxi/unlocked-spanish.nvmem).
La regla no cubre saves personales adicionales, NVRAMs modificadas en runtime,
backups, ROMs ni BIOS. `local-saves/` y `roms/` permanecen ignorados por Git.

## Restaurar/importar en Debug o Release

Primero cierra Flycast normalmente. El script se niega a copiar si detecta un
proceso `flycast.exe`, para evitar que el emulador pise el archivo al salir.
Desde PowerShell en la raíz del repo:

```powershell
# Importación con confirmación interactiva
.\scripts\install-kofxi-test-save.ps1 -Configuration Release

# O restaurar los dos perfiles aislados
.\scripts\install-kofxi-test-save.ps1 -Configuration Both
```

El script usa por defecto el fixture versionado, espera una NVRAM de 128 KiB,
la copia con nombre `kofxi.zip.nvmem`,
verifica SHA-256 y respalda cualquier archivo de destino antes de reemplazarlo.
No modifica `kofxi.zip.nvmem2`; la muestra desbloqueada disponible no incluye ese
segundo archivo. Para suministrar otra copia local:

```powershell
.\scripts\install-kofxi-test-save.ps1 -Configuration Release -SourcePath 'D:\ruta\kofxi.zip.nvmem.unlocked.spanish'
```

El script opera solo en `build-windows-msvc\Debug\data` y/o
`build-windows-msvc\Release\data`; no migra savestates `.state`, que pueden ser
incompatibles entre builds.

## Para una distribución futura

No incluir este save personal. La opción para usuarios finales debería ser una
importación explícita: detectar un save previo, preguntar antes de copiar,
respaldar el destino y dejar intacto el origen. No copiar automáticamente en
silencio ni sobrescribir el progreso de alguien.

# Entorno de desarrollo ideal — Windows + VS Code

Esta es la ruta recomendada para trabajar en Flycast desde Windows. Usa Visual
Studio 2022/MSVC x64, CMake Tools y presets con nombres explícitos. No requiere
GCC ni LLVM/Clang para la primera build.

## 1. Qué instalar

En **Visual Studio Installer → Build Tools 2022 → Modify**, habilitar:

- **Desktop development with C++**.
- MSVC v143 x64/x86 build tools.
- Windows 10/11 SDK.
- C++ CMake tools for Windows.
- Git for Windows (si aún no está instalado).

En VS Code tener las extensiones **CMake Tools** (`ms-vscode.cmake-tools`) y
**C/C++** (`ms-vscode.cpptools`). El workspace puede recomendarlas al abrir la
carpeta. El repo declara CMake mínimo 3.22.1; los componentes de VS incluyen una
versión compatible. CMake Tools puede localizar CMake de Visual Studio aunque
`cmake.exe` no esté en el PATH de una PowerShell normal.

## 2. Preparar dependencias una sola vez

Abre PowerShell y ejecuta:

```powershell
cd E:\github\kofxi-flycast
git submodule update --init --recursive
```

Descarga las dependencias fijadas por Flycast; requiere red y espacio en disco.
No reemplaza ni altera archivos de juego/saves. Espera a que termine antes de
configurar. Para comprobar:

```powershell
git submodule status
```

Cada ruta debería mostrar su hash de checkout sin prefijo `-` (no inicializado).

## 3. Abrir correctamente en VS Code

Abre la **raíz del repo**, no `build-clang/` ni `core/`:

```powershell
code E:\github\kofxi-flycast
```

En VS Code:

1. Acepta instalar las extensiones recomendadas si aparece el aviso.
2. `Ctrl+Shift+P` → **CMake: Select Configure Preset**.
3. Elige **Windows x64 — Visual Studio 2022 / MSVC** (`windows-msvc`).
4. `Ctrl+Shift+P` → **CMake: Configure**. Espera al mensaje de configuración
   exitosa y revisa la pestaña Output → CMake si falla.
5. `Ctrl+Shift+P` → **CMake: Select Build Preset** → `windows-msvc-debug`.
6. `Ctrl+Shift+P` → **CMake: Build**. Para compilar como Release, selecciona
   `windows-msvc-release` y vuelve a Build.

El preset configura Visual Studio 2022, arquitectura x64 y deja los archivos en
`build-windows-msvc/`. CMake Tools ejecuta CMake; para este primer recorrido no
hay que escoger una ruta de GCC ni editar el compilador a mano.

## 4. Presets y la carpeta previa `build-clang`

Flycast upstream trae presets `clang` y `gcc` con Ninja/Debug. Esos nombres son
etiquetas de preset; si no se proporciona un compilador, CMake puede tomar el
compilador disponible en el entorno. En esta máquina `build-clang/CMakeCache.txt`
registró `cl.exe` (MSVC), Ninja y Debug: **no era una compilación Clang**.

No borrar ni renombrar `build-clang/`: las cachés de CMake guardan rutas internas.
Se puede dejar como está e ignorada localmente. El nuevo preset MSVC usa otra
carpeta (`build-windows-msvc/`) y evita pisarla. `.gitignore` excluye `build-*/`
para que directorios de configuración/objetos no se publiquen.

Los presets upstream `clang`/`gcc` se conservan para que el trabajo upstream siga
disponible; en Windows, GCC solo debe elegirse si se configura deliberadamente
un toolchain MinGW completo. No usar el `gcc.exe` de 32 bits del PATH para este
build x64. Android/Switch requieren sus propios toolchains y no se configuran con
este preset Windows.

## 5. Diagnóstico rápido

### VS Code dice que no encuentra CMake

Primero confirmar que se instaló **C++ CMake tools for Windows** y reiniciar VS
Code. Ejecutar `CMake: Scan for Kits` y `CMake: Select a Configure Preset`.
Si sigue sin localizarlo, buscar `cmake.exe` dentro de la instalación VS y
configurar en **Settings → Extensions → CMake Tools → Cmake Path** (`cmake.cmakePath`)
la ruta local descubierta. No guardar una ruta absoluta de usuario en el repo.
En la instalación usada para preparar esta guía estaba bajo
`Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`; cada versión
de VS puede instalarlo en otro lugar.

### El preset `windows-msvc` no aparece

Actualizar CMake Tools, cerrar/reabrir la carpeta raíz y ejecutar `CMake: Select
Configure Preset` otra vez. Confirmar que `CMakePresets.json` está en el root y
que la versión de CMake soporta schema v3 (el mínimo del proyecto, 3.22.1, lo hace).

### CMake dice que faltan archivos de dependencias

Ejecutar `git submodule update --init --recursive` desde la raíz y volver a
**CMake: Configure**. No copiar dependencias de otra build directory.

### Quiero verificar el toolchain efectivo

En la salida de CMake debe aparecer el generador Visual Studio 17 2022 y x64.
En `build-windows-msvc/CMakeCache.txt`, verificar:

```text
CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022
CMAKE_GENERATOR_PLATFORM:INTERNAL=x64
```

CMake puede mostrar el compilador en `CMakeFiles/*/CMakeCXXCompiler.cmake` o en
la salida del configure. `build-clang` no es una prueba de Clang por su nombre.

## 6. Alternativa oficial de upstream

El script `shell/windows/generate_vs_project.bat` genera el proyecto Visual
Studio 2022 x64 que describe el README upstream. Es útil si se quiere abrir la
solución generada directamente en Visual Studio. Para el flujo diario de VS Code
usa los presets anteriores; no ejecutes dos configuraciones distintas en la
misma carpeta de build.

## Estado de esta receta

Los componentes VS/MSVC/CMake fueron detectados en la máquina al redactar esto,
y el cache previo confirma MSVC. Sin embargo, esta receta **todavía no ha pasado
una build completa de Flycast**: los submódulos no estaban inicializados y no se
ejecutó el build después de configurarlos. El primer build tras preparar las
dependencias será la aceptación de este setup.

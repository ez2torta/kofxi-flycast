# Preparación local y publicación futura

Directorio inicial: `E:/github/kofxi-flycast`. Rama: `kofxi-main`.
Remoto `upstream`: https://github.com/flyinghead/flycast.git.
No hay remoto `origin`, commit propio ni publicación de esta preparación.

## Dependencias/build

La copia tiene historial completo. Antes de compilar:

```sh
git submodule update --init --recursive
```

Seguir las recetas upstream del README y workflows para cada plataforma; T02–T04
deben fijarlas y verificarlas en la base indicada. Build y dependencias no están
probados todavía. El script de preparación del repo de investigación no forma
parte de este checkout.

En este volumen Windows Git detecta propiedad no verificable. Para inspeccionar
solo este repo se usó configuración por invocación (sin cambiar configuración global):

```powershell
git -c safe.directory=E:/github/kofxi-flycast status --short
```

## Cuando se decida publicar

1. Elegir propietario y nombre definitivo de GitHub. Crear preferentemente un
   fork GitHub de `flyinghead/flycast` para conservar la relación de procedencia.
2. Revisar documentación/diff y preparar el primer commit propio; hoy está sin
   commit. Confirmar que solo incluye archivos del proyecto y no artefactos privados.
3. Añadir `origin` con la URL elegida y subir `kofxi-main`; configurar esa rama
   como predeterminada y reglas de revisión según el equipo.
4. Revisar workflows heredados antes de activar releases/despliegues del fork.
5. Convertir T00–T11 en issues conservando IDs, dependencias y criterios; asignar
   responsables y equipos reales. Actualizar el estado de bootstrap en el roadmap.

Estas instrucciones no han creado recursos remotos ni ejecutado pushes.

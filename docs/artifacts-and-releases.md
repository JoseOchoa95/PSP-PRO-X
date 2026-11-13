# Artefactos en Git y distribución de releases

## Estado comprobado del repositorio

El commit `d535f78` incorporó **25 archivos de `artifacts/`**, con un tamaño
total de **3 035 675 bytes** en el árbol Git: los ZIP binario y fuente alpha,
logs de compilaciones y tests, manifiestos, hashes y versiones de herramientas.
`artifacts/work/` y `.phase1/` quedaron ignorados. No se ha comprobado si ese
commit se publicó en el remoto; esta afirmación describe el commit local.

Versionarlos no modifica el comportamiento del firmware, pero guardar nuevas
copias de ZIP y logs en cada release hace crecer el historial y mezcla entradas
de desarrollo con sus salidas generadas.

## Organización adoptada

| Contenido | Ubicación |
| --- | --- |
| Fuentes, scripts, tests, documentación, changelog y licencias | Repositorio Git |
| Resumen de pruebas y metadatos pequeños que se decida conservar | Documentación versionada, con versión/revisión explícitas |
| ZIP instalable, fuentes correspondientes, manifiesto y SHA-256 de cada release | Assets de la release |
| Logs completos y archivos intermedios | Artefactos de CI o almacenamiento local |

[GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
permite distribuir versiones asociadas a tags, notas y archivos binarios. Para
PRO-X debe adjuntar el paquete, su fuente acompañante, manifiesto y hashes
a la release concreta y enlazarlos desde el README. Los ZIP locales actuales
siguen siendo snapshots alpha; no se renombraron a beta ni se publicaron aquí.

Los binarios heredados que **son entradas necesarias de compilación**, como
March33 y bibliotecas en `firmware/`, requieren una decisión de procedencia y
licencia separada; no deben confundirse con salidas generadas de `artifacts/`.

## Limpieza del contenido ya seguido

Por instrucción del responsable, este cambio retira del índice los 25 archivos
de `artifacts/` mediante `git rm --cached` y conserva sus copias locales. El
responsable publicará los paquetes y fuentes correspondientes como assets de
Releases; esta tarea no sube archivos ni crea una release.

La carpeta completa `artifacts/` queda ignorada, junto con `release/`,
`releases/`, `dist/`, objetos, ELF y otras salidas de compilación. Se mantienen
los scripts que generan los paquetes y la evidencia resumida en documentación.
El registro de herramientas se conserva en [build-toolchain.txt](build-toolchain.txt)
para que el empaquetador siga funcionando desde un checkout nuevo.

Añadir una ruta a `.gitignore` no deja de seguir archivos ya versionados, según
la [documentación de Git](https://git-scm.com/docs/gitignore). Retirarlos en un
commit posterior tampoco los borra de commits anteriores. No hace falta
reescribir el historial para adoptar una política más limpia en adelante.

El historial anterior permanece intacto, por lo que `d535f78` sigue conteniendo
los artefactos originales. El README enlaza a la página de Releases en lugar de
ZIP dentro del código. No se alteran tags ni se publican assets en esta tarea.

Comprobaciones de la limpieza: SHA-256 de los 25 archivos locales sin cambios,
ausencia de `artifacts/` en el índice, exclusiones de nuevas salidas y enlaces
documentales revisados. El empaquetador y su verificación se ejecutaron además
en una copia aislada sin `artifacts/toolchain.txt`: funcionaron usando el registro
documentado y los binarios de firmware existentes, sin recompilar el firmware.

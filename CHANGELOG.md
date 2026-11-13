# Changelog

## Unreleased

- Fix: paquetes ZIP, logs, manifiestos y demás salidas generadas de `artifacts/`
  dejan de seguirse en Git y se conservan localmente para su publicación en Releases.
- Reglas de exclusión para artefactos, carpetas de release y salidas de compilación.
- Enlaces de descarga del README dirigidos a Releases, separados del código fuente.
- Registro del toolchain conservado como documentación para que el empaquetado
  no dependa de metadatos generados incluidos en el checkout.

## 0.1.0-beta.1 — 2026-11-13

- Versión/tag comunicado por el responsable del proyecto: `v0.1.0-beta.1`.
- Primer reporte del usuario de pruebas en PSP física: instalación,
  Recovery y suspensión funcionando.
- Actualización del estado público y de los registros de validación para
  distinguir los resultados reportados de las comprobaciones de desarrollo.
- Esta promoción a beta no introduce cambios funcionales de firmware respecto
  de la implementación de Fase 1 de `0.1.0-alpha.1`.

Estado: **beta con validación física parcial reportada por el usuario**.
Pendientes: Infinity, juegos y plugins; también las pruebas restantes de la
matriz que no se han comunicado. No se especificaron el modelo, la versión
completa del firmware, el hash ensayado, el tipo de Recovery ni el contexto de
suspensión. No se extiende el resultado a todos los modelos o configuraciones.
Véase el [reporte físico](docs/hardware-report-0.1.0-beta.1.md).

Los ZIP locales conservan el nombre y manifiesto `0.1.0-alpha.1` con el que se
generaron. Esta actualización documental no genera ni publica un paquete beta.

## 0.1.0-alpha.1 — 2026-11-13

- Base PRO-C fijada en `d9435ff4af6ba60466b46019393f6bbd1fc72ac0`.
- Identidad visible PRO-X en sistema 6.61, Installer, Fast Recovery,
  VSH, Recovery y encabezados de sus traducciones incluidas.
- Compilaciones limpias de referencia y PRO-X con toolchain fijado;
  port mínimo de scripts Python y adaptación de una opción obsoleta de GCC.
- Empaquetado explícito experimental, manifiesto de fuentes/herramientas,
  SHA-256 y comprobaciones de títulos, módulos y contratos conservados.
- Documentación de instalación condicional y matriz de pruebas físicas.

Estado: **implementación preparada para pruebas; hardware pendiente**.
No se ha validado instalación, ejecución ni arranque automático con Infinity.
Los valores HEN y las interfaces/rutas internas PRO permanecen intactos.
Se conservan componentes precompilados heredados de upstream, incluido March33.
No se incorpora WPA2, actualización en línea, almacenamiento nuevo ni cambios IPL.

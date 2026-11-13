# Reporte físico — 0.1.0-beta.1

Fecha de recepción: **2026-11-13**.
Versión/tag indicado por el responsable del proyecto: **v0.1.0-beta.1**.
Estado: **validación física parcial reportada por el usuario**.

La evidencia disponible es el reporte del usuario en esta conversación:
instalación, Recovery y suspensión funcionan; faltan Infinity, juegos y plugins.
El agente no ejecutó estas pruebas en una consola ni recibió fotos o logs del
dispositivo. Se registra el resultado comunicado sin convertirlo en una
certificación de todas las configuraciones.

| Prueba comunicada | Estado | Alcance de la evidencia |
| --- | --- | --- |
| Instalación | PASS | Reportada como funcionando por el usuario |
| Recovery | PASS | Reportado como funcionando; no se precisó si menú Recovery o Fast Recovery |
| Suspensión | PASS | Reportada como funcionando; no se precisó si en XMB o en una aplicación |
| Infinity | NOT RUN | El usuario indica que falta probarlo |
| Juegos | NOT RUN | El usuario indica que falta probarlos |
| Plugins | NOT RUN | El usuario indica que falta probarlos |

Datos todavía no comunicados:

- Modelo comercial/generación y versión completa del firmware actual.
- CFW previo, mecanismo de arranque y versión binaria de Infinity si existe.
- Nombre y SHA-256 exactos del paquete/EBOOT que se probó.
- Pasos, número de ciclos, tipo de Recovery y contexto de suspensión.
- Evidencias del dispositivo y procedimiento concreto de recuperación/salida.

La matriz detallada sigue en [install-and-validation.md](install-and-validation.md).
Las filas específicas de Fast Recovery, menús o contextos de suspensión no se
marcan PASS a partir de este reporte general. Se completarán con los resultados
concretos cuando se comuniquen.

La implementación compilada localmente sigue identificada por los manifiestos
y ZIP `0.1.0-alpha.1`. El usuario eligió el nombre beta para su release; esta
actualización registra esa decisión sin renombrar archivos, alterar hashes,
recompilar firmware, crear un tag Git ni publicar una release.

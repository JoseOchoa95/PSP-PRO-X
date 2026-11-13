# Instalación experimental y validación física

Versión declarada: **PRO-X 0.1.0-beta.1**, objetivo **6.61**. Estado:
**validación física parcial reportada por el usuario**; no es una versión estable.
Instalación, Recovery y suspensión se reportaron funcionando. Infinity, juegos
y plugins siguen pendientes. Los ZIP/manifiestos locales conservan la identidad
`0.1.0-alpha.1` de su generación; el hash exacto probado no se ha comunicado.
Véase el [reporte físico](hardware-report-0.1.0-beta.1.md).

## Ficha obligatoria antes de usar la consola

Completar una ficha por combinación; todavía no se han proporcionado estos datos:

| Dato | Valor registrado |
| --- | --- |
| Modelo comercial y generación (01g/02g/etc.) | NOT PROVIDED |
| Firmware real actual / texto completo de versión | NOT PROVIDED |
| CFW actual y revisión; configuración y plugins | NOT PROVIDED |
| Mecanismo de arranque (temporal, Infinity, otro) | NOT PROVIDED |
| Infinity binario instalado, versión y hash | NOT PROVIDED |
| Paquete PRO-X y SHA-256 | Tomar de `manifest.json` y SHA256SUMS externo |
| Operador, fecha, estado de alimentación y copia de datos/configuración | NOT PROVIDED |
| Ruta de salida/recuperación adecuada a esta consola | Pendiente de determinar |

Esta guía es un protocolo general. No prescribe desinstalar otro CFW, reflashear
OFW ni modificar IPL. Una consola con otro mecanismo permanente necesita primero
un procedimiento específico revisado para su estado. Solo se ha compilado el
objetivo 6.61; las comprobaciones de firmware heredadas no se han cambiado y no
deben interpretarse como una certificación de 6.60 o de todos los modelos.

## Preparación y prueba sin arranque automático

1. Completar la ficha, comprobar la alimentación y conservar copia de partidas,
   plugins y configuración. Conservar el instalador y revisión anteriores junto
   con su procedimiento aplicable de recuperación. No iniciar si esa ruta es
   desconocida para la consola concreta.
2. Comprobar el SHA-256 del ZIP con `Get-FileHash <archivo> -Algorithm SHA256`
   o `sha256sum`; comprobar además sus contenidos con `verify-package.py`.
   Copiar `PSP/GAME/PROUPDATE/EBOOT.PBP` y
   `PSP/GAME/FastRecovery/EBOOT.PBP` a sus rutas homónimas en el almacenamiento
   apropiado del modelo. Ambos archivos deben ser del mismo paquete.
3. Los archivos `seplugins/recovery_*.txt`, `satelite_*.txt` y `fonts/` son los
   recursos de traducción/fuente originales con encabezados actualizados. Si ya
   existen versiones personalizadas, conservarlas antes de sustituirlas; una
   traducción antigua puede seguir mostrando PRO. No reemplazar listas de plugins
   `vsh.txt`, `game.txt` o `pops.txt`: el paquete no contiene esas listas.
4. Probar primero la carga normal de PRO-X sin habilitar por primera vez su
   arranque automático. Si Infinity ya está instalado y su versión coincide con
   la documentación inspeccionada, su README describe mantener **HOME durante el
   encendido** para omitir la carga automática del CFW. Registrar esa condición
   y el estado observado antes de abrir el instalador. Infinity sigue instalado;
   esta prueba se distingue de un arranque automático por la combinación HOME,
   la versión inicial observada y el lanzamiento manual posterior. Confirmar el
   comportamiento en la versión binaria instalada antes de confiar en él.
5. Abrir **661 PRO-X Installer**. La ruta original muestra `Press X to launch
   CFW`: X ejecuta la instalación/comprobación de archivos y, si termina bien,
   solicita otra X para cargar el CFW. Registrar toda la salida y cualquier
   código de error. R es la salida ofrecida. No usar Triangle ni mantener L como
   parte de la prueba inicial: son operaciones distintas en el instalador real.
6. Comprobar `6.61 PRO-X` en Información del sistema, anotando cualquier sufijo
   añadido por Infinity. Abrir VSH y Recovery, comprobar sus encabezados y probar
   funciones básicas no destructivas. Tras un arranque sin carga automática,
   probar **661 PRO-X Fast Recovery** y volver a registrar la identidad.

PRO-X reutiliza las rutas flash de PRO-C. No constituye una instalación paralela.
Los módulos están incorporados en los EBOOT: no hay PRX sueltos que copiar a
flash manualmente. El paquete no contiene OFW Sony, Infinity ni herramientas IPL.

## Prueba separada de Infinity

Solo después de aprobar la instalación/carga manual y revisar la ficha, utilizar
la selección existente **PRO** de la versión de Infinity documentada para esa
consola. No se crea una opción PRO-X. Si Infinity no está instalado, su instalación
y adecuación al estado concreto deben resolverse antes de esta prueba; este
paquete no lo instala. Referencia: [README oficial inspeccionado](https://github.com/DaveeFTW/Infinity/blob/6a4f269f8abcf65696064cad94ba9ac8b845bd74/README.md).

Apagar completamente y encender sin HOME tres veces, distinguiéndolo de suspensión
o reinicio del XMB. En cada ciclo registrar versión, acceso a menús y cualquier
fallo. La evidencia de contrato en código no permite marcar estas filas PASS.

## Matriz actual

Fecha de registro: **2026-11-13**. Evidencia física: reporte del usuario, sin
modelo, hash del paquete ni evidencias adjuntas todavía. Las filas generales
recogen lo comunicado; no sustituyen las pruebas de sus variantes específicas.
Evidencia de desarrollo en `artifacts/verification.json` y logs de build.

| Prueba física | Estado | Evidencia / criterio pendiente |
| --- | --- | --- |
| Instalación inicial | PASS | Usuario reporta que funciona; faltan modelo, hash y foto/log del instalador |
| Recovery, reporte general | PASS | Usuario reporta que funciona; falta precisar menú Recovery o Fast Recovery |
| Suspensión, reporte general | PASS | Usuario reporta que funciona; falta precisar contexto XMB/aplicación |
| Identidad en sistema, XMB y menús | NOT RUN | Fotos de las cinco superficies y sufijo Infinity |
| Fast Recovery | NOT RUN | Carga manual desde estado de arranque documentado; misma revisión |
| Funciones básicas VSH/Recovery | NOT RUN | Acceso, navegación y retorno al XMB |
| Infinity, apagado completo/ciclo 1 | NOT RUN | Selección PRO, encendido sin HOME, PRO-X disponible |
| Infinity, apagado completo/ciclo 2 | NOT RUN | Repetir y registrar individualmente |
| Infinity, apagado completo/ciclo 3 | NOT RUN | Repetir y registrar individualmente |
| Reinicio del XMB | NOT RUN | Identidad y funcionamiento conservados |
| Suspender/reanudar en XMB | NOT RUN | Sin bloqueo ni pérdida de estado |
| Suspender/reanudar en aplicación | NOT RUN | Identificar aplicación/versión, sin regresión |
| Juego representativo | NOT RUN | Título, formato/driver y resultado frente a referencia |
| Homebrew conocido | NOT RUN | Nombre/versión y resultado frente a referencia |
| Plugins | NOT RUN | Nombres/versiones, ámbito y resultado frente a referencia |
| Persistencia de configuración | NOT RUN | Ajuste concreto, guardar, reiniciar y releer |
| Salida/recuperación de la prueba | NOT RUN | Procedimiento específico y resultado o revisión sin ejecutarlo |

Usar exclusivamente **PASS**, **FAIL**, **NOT RUN** o **BLOCKED** para resultados,
con evidencia, operador y combinación exacta. Un fallo de instalación, arranque o
recuperación impide validar esa combinación; aprobar un modelo no aprueba otros.

## Salida y recuperación: capacidades reales, aún sin validar

En esta revisión Triangle ejecuta `uninstall_cfw()`: elimina `flash1:/config.se`
y los módulos de la lista PRO. No restaura automáticamente el CFW anterior ni
elimina Infinity. Mantener L fuerza la escritura y elimina configuración; no es
una comprobación rutinaria. No ejecutar estas opciones para completar la matriz.

Con Infinity, confirmar primero una ruta que permita omitir el CFW seleccionado
(HOME está documentado por upstream), y dejar de seleccionarlo para el arranque
antes de cualquier eliminación de sus módulos. Una restauración del CFW previo,
si se acuerda para esa consola, debe usar sus instaladores reales y su revisión
conservada. El orden concreto y su resultado siguen pendientes de la ficha.
Un rollback de Git no recupera una PSP. La instalación normal tiene un reporte
PASS del usuario; no se han comunicado pruebas de reflasheo o desinstalación.

**Implementación preparada para pruebas** exige código, compilación, comprobaciones
y paquete completos. **Fase 1 validada** exige además resultados físicos PASS de
instalación y arranque Infinity para cada combinación anunciada. Actualmente hay
validación física parcial reportada; la Fase 1 completa sigue pendiente.

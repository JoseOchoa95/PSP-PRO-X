# Validación de Fase 2

Fecha: **2026-11-13**. Versión local actual: **v0.2.0**, PSP **6.61**.
Base Git: `827e407b4dc8fa10d19a1b5430138946f145320c`, más los cambios de esta
fase identificados por inventario y hash del manifiesto. No se ha publicado tag.
Estado de entrega: **experimental con conexión reportada funcionando**. Se recibió y
analizó el reporte de PSP-2010: captura y 32 rangos válidos, una firma 6.61
identificada. El usuario reporta perfil nuevo y conexión con PRO-X a una red
2.4 GHz, b/g/n, WPA2 solamente. Tras apagado completo, sin iniciar PRO-X,
la conexión falló. No se reportaron transferencia ≥5 minutos, Recovery,
suspensión/reconexión ni retorno del diagnóstico v2; siguen **NOT RUN**.
Modelo/firmware del AP y cifrado AES verificado por separado: sin registrar.
No se sustituye la falta de evidencia de esas pruebas por la autorización de publicar.
La salida con X del diagnóstico v1 fue **FAIL reportado**. Registro actualizado
de implementación y build: `context/phase-2-native-wpa2.md`.
El ensamblado de release es `pro-x-d04098657acd`, árbol
`ffd94d1bdcb8274c94aa275de23f1d89e56ab4459d2a7b574e88b262e815d305`.
Sus dos EBOOT y el diagnóstico son idénticos a los entregados con el build
`pro-x-ba6b5f9f45f2`; solo se eliminó una línea vacía final en el fuente del diagnóstico.
Compilación, suites C con relocaciones, cuatro tests Python y módulos embebidos:
**PASS**. Paquetes actuales en `builds/pro-x-0.2.0-661*.zip`.

## Registro histórico previo al reporte (0.2.0-beta.1)

## Evidencia local

Antes de modificar código se revisaron README, Fases 1 y 2, `context/`, fuentes,
logs y resultados de Fase 1. Se recompiló el estado inicial:
`artifacts/logs/pro-x-9a7e5310c41b.log`, PASS con el árbol
`1c590a0f3922b1c18f803cfa7a8c736cc82e5949c672c21b635ae2ce2ffb6a54`,
idéntico al registrado en Fase 1. Los resultados físicos anteriores no se
trasladan automáticamente al nuevo binario.

| Comprobación | Resultado | Evidencia |
| --- | --- | --- |
| Base inicial limpia 6.61 | PASS | Log y hash anteriores |
| Payload ARK frente a wpa2psp | PASS | 32 sitios coincidentes; revisiones y SHA-256 en `wpa2-upstream.json` |
| Firmas/layout originales de Sony 6.61 | BLOCKED | No se obtuvieron; cero variantes activas |
| Validador nativo C, fixtures sintéticas | PASS | `scripts/test-native.sh`; rechazo sin escrituras, límites, firma, parcial, repetición, otra base |
| Configuración privada C con I/O simulado | PASS | Ausente, deshabilitada, inválida, corta/larga, versión desconocida, error I/O, escritura/cierre/sync/rename fallidos y recuperación de backup |
| Compilación limpia final, referencia e integración 6.61 | PASS | `artifacts/logs/reference-34d0df0875e5.log`, `artifacts/logs/pro-x-268a9c1eb444.log` |
| Objetos WPA2 y SystemControl dentro del instalador | PASS | `scripts/verify.py`, `artifacts/verification.json` |
| Contratos públicos y diff acotado | PASS | Snapshot revisado `phase-2-source.json` y verificador |
| Paquete binario/fuente, manifiesto y SHA-256 | PASS | `package.py`, `verify-package.py`, ZIP diagnóstico separado |
| Reconstrucción desde el ZIP fuente | PASS | `artifacts/source-rebuild-phase2.json`: ambos EBOOT y diagnóstico idénticos |
| Tests heredados PRX/gzip/PBP | PASS | Cuatro tests Python, `artifacts/logs/phase2-python-tests.log` |

El primer build de integración compiló Recovery/SystemControl e instalador,
pero falló al enlazar el diagnóstico por definir module_start junto al CRT.
Se corrigió para usar main/CRT. Ese intento fallido está en
`artifacts/logs/pro-x-34aea701028c.log`; no se contabiliza como PASS.
No se infiere compatibilidad de red a partir de textos, compilación o fixtures.

Las dos suites C guardan su salida en `artifacts/logs/phase2-native-tests.log`.
No se introdujeron advertencias de compilación en los nuevos archivos WPA2,
configuración o diagnóstico; permanecen advertencias heredadas de la base.

## Identidad y artefactos

El árbol final de firmware es
`6cb98f4eace514dea668d70b328378dad59f46ad6e9ada80b93e9e851a11e4e2`.
Los inventarios completos, SHA-256 de los EBOOT y PRX, imagen fijada y comando
real están en `artifacts/pro-x-build.json` y
`builds/pro-x-0.2.0-beta.1-661-manifest.json`.
Fast Recovery puede conservar el hash de Fase 1: su buffer de arranque no cambia
y carga el SystemControl instalado. El instalador sí incorpora el módulo nuevo;
se comprueban sus bytes y headers, no solo su nombre.

- `builds/pro-x-0.2.0-beta.1-661.zip`: instalador y Fast Recovery.
- `builds/pro-x-0.2.0-beta.1-661-source.zip`: fuentes correspondientes.
- `builds/pro-x-0.2.0-beta.1-661.sha256`: hashes externos de esos dos ZIP.
- `builds/pro-x-0.2.0-beta.1-661-diagnostic.zip` y `.sha256`: diagnóstico optativo.

Estos paquetes se trasladaron de `artifacts/` a `builds/` sin alterar sus bytes:
se verificaron las copias y después se eliminaron los duplicados originales.
Las futuras ejecuciones del empaquetador usan `builds/` directamente; los logs
y metadatos de compilación permanecen en `artifacts/`.

Todos los artefactos permanecen ignorados por Git y se entregan localmente;
no se publicó una release. El manifiesto declara `blocked-module-identification`,
preferencia Enabled, cero variantes verificadas y parche inactivo.
La fuente puede reempaquetarse tras actualizar documentación sin cambiar los
insumos del firmware; la reconstrucción registra por separado su hash de árbol.

## Matriz física

Consola/modelo/generación, firmware completo, hash probado, método de arranque,
Infinity y plugins: **sin registrar para esta fase**. AP, firmware del AP, banda,
seguridad y evidencia RSN: **sin registrar**. Nunca registrar contraseña o PSK.

| Prueba | Estado | Criterio pendiente |
| --- | --- | --- |
| Diagnóstico previo al inicio del módulo | NOT RUN | Captura y reporte privado, dos bases/cargas y relocaciones revisadas |
| Orden de configuración XMB / GAME | NOT RUN | Almacenamiento listo y preferencia cargada antes del primer apctl |
| Recovery Enabled/Disabled, errores y persistencia | NOT RUN | Estado guardado real y efectivo tras reiniciar; conservar SEConfig |
| Primera ejecución sin archivo privado | BLOCKED | Identificar módulo antes de probar WPA2 por defecto sin plugin |
| Perfil XMB y protocolo negociado | BLOCKED | Crear/guardar perfil; AP confirma WPA2/RSN AES/CCMP |
| Autenticación y DHCP | BLOCKED | IP, puerta de enlace y DNS coherentes |
| Transferencia HTTP controlada ≥5 minutos | BLOCKED | Contenido/hash y ausencia de bloqueos |
| Contraseña incorrecta / AP ausente | BLOCKED | Fallo controlado sin reinicio |
| Tres reconexiones y perfil guardado | BLOCKED | Éxito repetible |
| Suspensión/reanudación y reconexión | BLOCKED | No exigir conservar TCP |
| Homebrew con módulo verificado | BLOCKED | WPA2 sin plugin externo |
| Juego representativo | BLOCKED | Variante conocida; diferenciar servicio cerrado de fallo Wi-Fi |
| Disabled, reinicio, WPA1; reactivar | BLOCKED | Comparar comportamiento original y parche, con AP adecuado |
| Descarga/recarga del módulo | BLOCKED | Cada instancia admitida, sin estado obsoleto |
| Fast Recovery e Infinity | NOT RUN | Arranque de esta revisión, Infinity tres ciclos si está disponible |
| XMB, Recovery, juego/homebrew y plugins | NOT RUN | Regresión básica de esta revisión |
| Red abierta y WEP | NOT RUN | Probar solo con entorno disponible |
| Segundo AP y otros modelos | NOT RUN | Registrar combinaciones, sin extrapolar |

Fase 1 mantiene su reporte parcial de instalación, Recovery y suspensión;
Infinity, juegos y plugins siguen pendientes. Véase
[hardware-report-0.1.0-beta.1.md](hardware-report-0.1.0-beta.1.md).
No hay una prueba física nueva en esta entrega.

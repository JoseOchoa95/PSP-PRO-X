# Procedencia de PRO-X 0.1.0-alpha.1

## Incorporación WPA2 de Fase 2 (0.2.0-beta.1)

Revisión PRO-X de partida: `827e407b4dc8fa10d19a1b5430138946f145320c`.
El 2026-11-13 se descargaron e inspeccionaron las fuentes exactas:

- [ARK-5 wpa2.c](https://github.com/PSP-Arkfive/ARK-5/blob/04138ec494e6e0a97000cc84e940576febbd5855/Compat/PSP/src/wpa2.c),
  revisión `04138ec494e6e0a97000cc84e940576febbd5855`.
- [wpa2psp wpa2.c](https://github.com/Kethen/wpa2psp/blob/7e6f2b967bdfff8e8fdf8645c5a9fa089262b73b/wpa2.c),
  revisión `7e6f2b967bdfff8e8fdf8645c5a9fa089262b73b`, además de main.c y README.
- [LICENSE de ARK en esa revisión](https://github.com/PSP-Arkfive/ARK-5/blob/04138ec494e6e0a97000cc84e940576febbd5855/LICENSE):
  GPL v3. El README wpa2psp atribuye el trabajo a Moment/ARK y declara que wpa2.c
  sigue la licencia ARK. Se conservan esos créditos en el port y los avisos.

Las 32 escrituras, seed, RSN y cálculo del salto coinciden entre ambos archivos;
sus diferencias son includes y el tipo de módulo SceModule/SceModule2. PRO-X usa
el SceModule2 de su interfaz de handler existente y lo adapta campo por campo a
un descriptor propio del validador. No importa estructuras de otro SDK ni instala
el handler paralelo del plugin. El payload se convierte a una tabla validada,
se integra con la cadena PRO, añade configuración privada y sincroniza cachés.

Los SHA-256 de las descargas y el inventario de destinos se registran en
[wpa2-upstream.json](wpa2-upstream.json). Ninguna referencia aporta layout y
firmas originales suficientes por sí sola. Posteriormente el reporte autorizado
de una PSP-2010 proporcionó layout, sitios originales y guardas. La firma mínima
se incorpora a `SystemControl/wpa2_variant_661.h`, con JAL relativas a la base
actual; no se incluye un módulo Sony completo ni el reporte crudo en Git/ZIP.
Estado actual: una variante identificada en v0.2.0 experimental; el propietario
reporta conexión a un AP WPA2-only. Captura independiente y transferencia
prolongada siguen sin reportarse.
La procedencia de Fase 1 que sigue se conserva sin cambiar la base de PRO-C.

Fecha de comprobación: 2026-11-13. Objetivo del paquete: firmware **6.61**.

## Base seleccionada

- Proyecto: [Team PRO / procfw](https://github.com/MrColdbird/procfw).
- Rama de procedencia: `master`; se usa un checkout detached de
  `d9435ff4af6ba60466b46019393f6bbd1fc72ac0`.
- Mensaje upstream: `Fixed popcorn issue reported in issue 596.`
- La referencia coincide con la revisión propuesta en la especificación.
- `Installer/main.c` declara `PRO-C`; `SystemControl/systemctrl.c` devuelve
  **0x1003** en `sctrlHENGetVersion()` y **0** en `sctrlHENGetMinorVersion()`.
- El Makefile raíz propaga `CONFIG_661=1`; los módulos seleccionan
  `PSP_FW_VERSION=661`, los launchers generan títulos 661 y existen tablas
  `g_661_offsets` y `g_pxe_661_offsets`. El texto de sistema original es
  `6.61 PRO-%c`. Esto acredita soporte en el código, no validación en una consola.

El repositorio PRO-X inicialmente solo contenía README y licencia. Se importaron
los 499 archivos de upstream a `firmware/`, conservando sus avisos, recursos y
documentación histórica. `upstream-files.json` contiene el SHA-256 original de
cada archivo. No se ha sustituido una base local anterior.

## Diferencias acotadas

Los únicos cambios en código C y Makefiles del firmware son textos visibles:
títulos Installer y Fast Recovery, encabezado del instalador con atribución,
versión de sistema 6.61 y encabezados de VSH/Recovery. Se cambiaron también los
14 encabezados equivalentes de las traducciones incluidas, conservando sus
codificaciones y el resto de sus bytes. La lista exacta se registra en
`artifacts/verification.json`.

Tres scripts de construcción se adaptaron a Python 3: shebang/finales LF,
`StringIO` binario sustituido por `io.BytesIO`, fecha gzip explícita y campos
PRX de tamaño escritos como enteros de 32 bits little endian en lugar del
`unsigned long` nativo del host de 64 bits. El parche independiente está en
`scripts/build-compat.patch` y se aplica también a la referencia sin marca.

`scripts/compile.sh` conserva las dependencias y su orden, incluida la
recompilación de `Common/*.o` después de Recovery. Filtra únicamente la opción
retirada `-mno-crt0`; los enlaces afectados ya usan `-nostartfiles`. No añade
supresiones de diagnósticos ni cambios de lógica para lograr la compilación.
Las herramientas IPL/cIPL y permanentes no se construyen ni empaquetan.

La cadena efectiva `6.61 PRO-X` mide 10 caracteres, igual que `6.61 PRO-C`:
11 bytes con NUL en el buffer ASCII de 30 bytes, 22 bytes con NUL en UTF-16.
No se cambia el destino en `sysconf_plugin`, las versiones HEN ni el código
que añade la minor version. Su valor en esta base sigue siendo cero.

## Dependencias heredadas

Upstream incluye bibliotecas estáticas en `libs/` y tres PRX March33 sin su
código fuente completo. El instalador 6.61 incorpora **march33_660.prx**,
SHA-256 `dbe7d55c00ece06bbd69af91e8e4526e359fa0be773f0b4c68c5239a0edbc966`.
Es una entrada heredada inalterada, no un módulo recompilado. Las otras dos
variantes generan headers pero no entran en el instalador 6.61. Las bibliotecas
systemctrl/kubridge se reconstruyen desde sus stubs antes de cada compilación.
Los demás binarios heredados necesarios y sus hashes constan en el inventario.
Esta limitación de reconstrucción de upstream se conserva y debe comunicarse
al redistribuir; no se afirma que todos los componentes procedan de fuentes C.

Toolchain fijado íntegramente mediante la imagen oficial archivada
`ghcr.io/pspdev/pspdev-docker@sha256:c4888f62d5b1d2ff905b14bab664c346af4d72e71e4bb13e815dafa3219eb18a`
(creada 2021-11-12): GCC 9.3.0, binutils 2.23.2, Make 4.2.1, Python 3.8.10,
zlib 1.2.11, newlib 1.20.0 y PSPSDK incluido en esa imagen. PSPSDK no comunica una versión
mediante `psp-config --version`; se fija por el digest de la imagen y hashes de
sus herramientas y reglas en [build-toolchain.txt](build-toolchain.txt). No se ha identificado
un SHA fuente independiente de ese SDK binario. Ver [build.md](build.md).

## Referencia de Infinity

Se inspeccionó [Infinity](https://github.com/DaveeFTW/Infinity/tree/6a4f269f8abcf65696064cad94ba9ac8b845bd74)
en `6a4f269f8abcf65696064cad94ba9ac8b845bd74` (checkout separado, no modificado
ni redistribuido). `firmware/procompat/src/main.c` carga:

- `/kd/_systemctrl.prx` antes de `/kd/init.prx`.
- `/kd/_vshctrl.prx` antes de `/kd/vshbridge.prx`.
- `/vsh/module/_recovery.prx` como sustituto de VSH en recuperación.

Inicializa `REBOOTEX_CONFIG`, `REBOOTEX_CONFIG_MAGIC`, modelo y firmware usando
el contrato PRO. PRO-X conserva `Common/rebootex_conf.h`, los offsets de boot,
imports/exports/NIDs, estructuras y rutas de instalación, incluida
`/vsh/module/_satelite.prx`. La comprobación compara todos los archivos contra
la referencia; únicamente admite las sustituciones de texto enumeradas.

La entrada existente **PRO** de Infinity es la ruta prevista para PRO-X.
Comparar fuentes acredita que estos contratos se conservaron; no demuestra
que arranquen juntos. Versión binaria de Infinity probada: **ninguna**.
Al generar los artefactos alpha no había pruebas físicas. Posteriormente el
usuario reportó instalación, Recovery y suspensión funcionando para la versión
declarada beta; el modelo/generación aún no se ha comunicado. Véase el
[reporte físico](hardware-report-0.1.0-beta.1.md). PRO-X ocupa el espacio de PRO-C;
esta versión no ofrece una instalación paralela.

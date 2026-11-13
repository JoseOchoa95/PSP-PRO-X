# Compilar y comprobar la Fase 1

Objetivo exclusivo del paquete: **6.61**, producto **0.1.0-alpha.1**.
Se necesitan Git, Docker con contenedores Linux y Python **3.12 o posterior en
el host** (solo biblioteca estándar). El compilador y Python de construcción
están dentro de la imagen fijada en `scripts/Dockerfile`; no se instala un SDK
global. La ejecución en contenedor se hace sin red. Solo la descarga inicial
de la imagen y de la referencia upstream necesita conexión.

## Comandos verificados el 2026-11-13

Desde la raíz del repositorio, en PowerShell o un shell con estos ejecutables:

```text
docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts
python scripts/build.py reference
python scripts/build.py pro-x
python -m unittest discover -s tests -v
python scripts/verify.py
python scripts/package.py
python scripts/verify-package.py artifacts/pro-x-0.1.0-alpha.1-661.zip
```

En esta máquina `python` apunta al alias de Windows Store; se usó realmente
`C:\Users\diala\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`
(Python 3.12). Sustituir `python` por la ruta del intérprete local si es necesario.
No se necesita Python 2. Las herramientas PSP no admiten todas `--version`;
se registraron versiones disponibles y hashes ejecutables en
`artifacts/toolchain.txt`.

`reference` obtiene el SHA upstream completo, valida los 499 hashes y aplica
únicamente `build-compat.patch`. Se compiló correctamente antes de cambiar la
marca. Se repitió la referencia final para reflejar exactamente el mismo orden
de objetos compartidos que upstream. `pro-x` toma una copia de `firmware/`.
Cada ejecución usa una entrada y directorio temporal nuevos, sin objetos ni
EBOOT de compilaciones anteriores. La salida se publica solo si terminan todos
los componentes. Un error devuelve un código distinto de cero y conserva el log.

Ambos modos ejecutan `make CONFIG_661=1 build_lib`, seguidos de los `make -C`
de `scripts/compile.sh`. El ejecutable PROUPDATE procede de **PXE/Launcher**;
`Installer` es un PRX incorporado por ese launcher. FastRecovery usa el buffer
live de `Rebootex_bin` de la misma ejecución. No se ejecuta `make all` del
Makefile raíz porque también construye herramientas IPL ajenas al paquete.

Las salidas quedan en `artifacts/work/<modo>-<id>/` (ignoradas por Git).
`artifacts/reference-build.json` y `pro-x-build.json` registran las fuentes,
hash de todo el árbol, HEAD del repositorio, digest local de imagen, script,
comando real, log y hashes de los dos EBOOT y módulos. El HEAD inicial por sí
solo no describe los cambios sin commit: **el inventario y su hash identifican
la revisión exacta del firmware construido**. El manifiesto del paquete conserva
esa identidad. Los logs y metadatos finales permanecen fuera de `work/`.

## Comprobaciones automáticas

`verify.py` valida ambos PBP/SFO, tamaños y formato del payload cifrado; comprueba
los textos en PRX generados, los módulos incorporados al PRX Installer y sus
headers, los PRX comprimidos que incorpora el launcher, el bootstrap PXE y que
Fast Recovery y el instalador usan el mismo buffer live. Verifica contra la
referencia todos los bytes ajenos al cambio de marca, cubriendo los valores
HEN, configuración, ABI, NIDs, offsets y rutas. No comprueba en una consola la
validez de las firmas ni el funcionamiento de los módulos.

El test del port Python verifica ida y vuelta gzip, fecha, tamaño, MD5, nombre
y atributos PRX, incluyendo el tamaño little endian de 32 bits en un host de
64 bits. Los tests de PBP rechazan cabeceras truncadas y offsets inválidos.

`package.py` exige que la revisión fuente actual y los hashes de build coincidan
con lo verificado. El paquete usa una lista explícita de contenido: dos EBOOT,
traducciones/fuentes gráficas, documentación, licencia y metadatos. No copia
indiscriminadamente el antiguo `dist/`. Genera SHA-256 internos y del ZIP.
`verify-package.py` vuelve a abrir el ZIP y comprueba hashes, inventario y títulos.
Se extrajo además el archivo fuente acompañante y se ejecutó `scripts/compile.sh`
en un contenedor limpio: produjo ambos EBOOT con los mismos SHA-256 que el build
del repositorio (`artifacts/source-rebuild.json`). El archivo fuente contiene los
insumos de los dos programas, omitiendo herramientas IPL y utilidades ajenas a
esta compilación. La comprobación completa del diff se ejecuta desde el repositorio.

## Intentos fallidos y límites

La compilación upstream intacta con PSPDEV 2026/GCC 15.2.0 falló por tipos de
APIs kernel incompatibles (`artifacts/logs/upstream-unmodified.log`). Los
intentos de combinar un SDK 2014 con ese runtime tampoco enlazaron. No son
compilaciones de referencia aprobadas; sus logs se conservaron como diagnóstico.
Se descartaron los adaptadores modernos y se fijó la imagen oficial 2021 con
sus dependencias completas. Los ajustes efectivos son únicamente los descritos
en [upstream.md](upstream.md).

Los logs exitosos contienen advertencias heredadas (punteros packed,
formatos/solapamiento de sprintf, variables sin uso, entre otras). No se
silenciaron ni se corrigieron problemas funcionales fuera de esta fase.
La base incluye bibliotecas y March33 precompilados: no se promete una
reconstrucción íntegra desde fuentes de todos sus componentes. Tampoco se
promete reproducibilidad byte a byte de los contenedores o de paquetes futuros;
se fijan entradas y herramientas y se entregan hashes de esta ejecución.

La compilación y la inspección estática no acreditan instalación, compatibilidad
de ejecución ni arranque mediante Infinity. Estas pruebas siguen **NOT RUN**.

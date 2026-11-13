# Compilar y comprobar la Fase 1

## Continuación: Fase 2

El empaquetador actual genera **v0.2.0**, objetivo **6.61**, estado
**prepared-for-hardware-test**. Los resultados alpha de las secciones
siguientes son históricos. Los mismos comandos de build usan el toolchain
fijado; además ejecutar:

```text
docker run --rm --network none --mount type=bind,source=<ruta-absoluta-del-repo>,target=/work pro-x-toolchain:phase1 sh /work/scripts/test-native.sh
python -m unittest discover -s tests -v
python scripts/verify.py
python scripts/package.py
python scripts/verify-package.py builds/pro-x-0.2.0-661.zip
```

`build.py pro-x` también construye `tools/wpa2diag` en un destino separado
`diagnostics/PSP/GAME/WPA2DIAG`; no se incorpora al instalador ni a Fast Recovery.
`package.py` produce otro ZIP diagnóstico independiente y su SHA-256. El ZIP
fuente incluye todos los insumos de ambos EBOOT y del diagnóstico.
Los ZIP finales, sus hashes y el manifiesto con nombre de versión se escriben
en `builds/`, ignorada por Git. `artifacts/` conserva logs, archivos intermedios
y metadatos internos. Las referencias a paquetes alpha en el registro histórico
son históricas; esos archivos se encuentran ahora en `builds/`, sin modificar
sus bytes. Las copias duplicadas de ZIP/hashes/manifiestos finales se eliminaron
de `artifacts/`; el empaquetador ya no las genera allí.
Las comprobaciones de Fase 2 conservan todos los archivos ajenos a marca y a la
lista explícita revisada `phase-2-source.json`. El verificador también comprueba
los objetos WPA2, su enlace y el SystemControl incorporado al instalador.
Los tests C se ejecutan con el compilador de host dentro del contenedor, sobre
las mismas fuentes del validador/configuración, con fixtures sintéticas y mock
de I/O; no ejecutan instrucciones MIPS ni un handshake de red.

El árbol completo y los hashes de binarios están en los metadatos generados.
Guías específicas: [soporte](wpa2-support.md), [validación](wpa2-validation.md).
El parche admite una firma 6.61 derivada del reporte autorizado de una PSP-2010
y sigue rechazando toda otra variante. La versión de producto se centraliza en
`scripts/product.py`; build, manifiesto y verificador deben coincidir.

## Registro histórico de Fase 1

Objetivo exclusivo del paquete documentado: **6.61**, producto **0.1.0-alpha.1**.
La versión declarada posteriormente por el responsable es **0.1.0-beta.1**, con
[validación física parcial reportada](hardware-report-0.1.0-beta.1.md).
Estos comandos/scripts conservan el empaquetado alpha original; esta actualización
de documentación no produce ni publica nuevos ZIP beta.
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
las versiones disponibles y hashes del entorno fijado se conservan en
[build-toolchain.txt](build-toolchain.txt). El empaquetador incorpora ese registro
documentado sin depender de un `artifacts/toolchain.txt` previamente versionado.

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

Todas las salidas de `artifacts/`, incluidos paquetes, logs y metadatos, se
generan localmente y están ignoradas por Git. No vienen en un checkout nuevo;
los comandos anteriores las crean. Las salidas intermedias quedan en
`artifacts/work/<modo>-<id>/`.
`artifacts/reference-build.json` y `pro-x-build.json` registran las fuentes,
hash de todo el árbol, HEAD del repositorio, digest local de imagen, script,
comando real, log y hashes de los dos EBOOT y módulos. El HEAD inicial por sí
solo no describe los cambios sin commit: **el inventario y su hash identifican
la revisión exacta del firmware construido**. El manifiesto del paquete conserva
esa identidad. Los logs y metadatos finales permanecen fuera de `work/`, pero
dentro de `artifacts/`. Los paquetes, fuentes correspondientes, manifiesto y
hashes se distribuyen como assets de release; los resúmenes de resultados se
conservan en documentación. Véase [artifacts-and-releases.md](artifacts-and-releases.md).

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

La compilación y la inspección estática no acreditan por sí mismas instalación,
compatibilidad de ejecución ni arranque mediante Infinity. Separadamente, el
usuario reportó instalación, Recovery y suspensión funcionando. Infinity,
juegos y plugins siguen **NOT RUN**; véase la [matriz](install-and-validation.md).

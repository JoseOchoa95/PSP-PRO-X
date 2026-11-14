# Créditos y avisos

## Phase 3 dependencies and references

The System Information label hook uses the PRO/ARK instruction anchor documented
in ARK-5 VSHControl, revision
`c045ddde334580dd92d2d2529df38646b451ab33`, under GPL-3.0-or-later. The pinned
reference and source hash are recorded in `scripts/updater-dependencies.json`.

The phase 3 client uses **Mbed TLS 3.6.7 LTS**, copyright the Mbed TLS
contributors, under the Apache-2.0 option. The unmodified pinned source archive
includes its original notices and licenses and accompanies the binary package.
Its SHA-256 implementation is also used by installer-side preflight; TLS stays
in the separate network client and is not added to installed firmware modules.
The **Mozilla CA extract** obtained from curl's CA extract service retains its
original header and is distributed under [MPL-2.0](https://www.mozilla.org/MPL/2.0/).
Dependency URLs and SHA-256
identities are recorded in `scripts/updater-dependencies.json`. No private keys
or GitHub tokens are included. The earlier transport prototype passed limited
owner-supplied PSP tests; the full update candidate still requires physical
installation validation.

PRO-X es un fork independiente basado en PRO-C de **Team PRO**, especialmente
Coldbird y Virtuous Flame. No es una publicación oficial de Team PRO, Davee o ARK.
Los avisos originales del árbol `firmware/` y `firmware/credit.txt` se conservan.
PRO-X y los archivos PRO que lo indican usan **GPL-3.0-or-later**; el texto
completo está en `LICENSE`. Los archivos modificados por PRO-X se identifican
en la documentación y en el manifiesto de fuentes del paquete.

- `firmware/credit.txt`: agradecimientos originales completos, distribuidos
  también en el paquete de prueba.
- PRO conserva el trabajo de Dark_AleX/M33 y otros autores, incluidos los PRX
  March33 heredados sin fuentes completas en esta base. Ver `docs/upstream.md`.
- `firmware/SystemControl/lz4.c` y `firmware/Common/lz4.h`: copyright Yann Collet y su aviso BSD
  de dos cláusulas, conservado íntegramente en estos archivos.
- `firmware/include/zlib.h`, `zconf.h` y `libs/libz.a`: zlib y sus avisos de
  Jean-loup Gailly y Mark Adler, conservados en los headers de upstream.
- Fuentes bitmap de `firmware/contrib/fonts/`: se mantiene la atribución a
  Richard J. Prinz del crédito upstream. Las traducciones conservan sus autores.
- PSPSDK y newlib aportan runtime/bibliotecas con sus licencias originales;
  sus avisos originales se conservan en `docs/licenses/` y se distribuyen en
  `licenses/` dentro del paquete de prueba.
  PrxEncrypter es una herramienta GPL de construcción, no una copia de Infinity.
- Infinity de Davee solo se inspeccionó como referencia de los contratos de
  arranque. No se modifica ni distribuye en este paquete.
- `firmware/Common/wpa2_patch.c`: payload WPA2 de **Moment (@moment0253)**,
  colaboradores de **ARK** y **wpa2psp/Kethen**. Adaptación e infraestructura de
  validación PRO-X. ARK-5 fija GPL v3 en su LICENSE; wpa2psp declara que wpa2.c
  sigue la licencia ARK. Se conservan atribución y texto GPL en el paquete.
  Revisiones, diferencias y hashes: `docs/upstream.md` y `docs/wpa2-upstream.json`.

El paquete experimental se acompaña de un archivo de fuentes para sus
componentes compilables, scripts y documentación. Los componentes precompilados
heredados se identifican por hash; su fuente completa no está disponible en
esta base y no se presenta como reconstruida. No retirar sus avisos ni anunciar
una distribución estable sin resolver también esta limitación de procedencia.

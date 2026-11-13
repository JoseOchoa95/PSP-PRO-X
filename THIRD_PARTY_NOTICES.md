# Créditos y avisos

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
  arranque. No se modifica ni distribuye en este paquete. ARK y WPA2 son
  referencias de fases futuras, sin código integrado en esta fase.

El paquete experimental se acompaña de un archivo de fuentes para sus
componentes compilables, scripts y documentación. Los componentes precompilados
heredados se identifican por hash; su fuente completa no está disponible en
esta base y no se presenta como reconstruida. No retirar sus avisos ni anunciar
una distribución estable sin resolver también esta limitación de procedencia.

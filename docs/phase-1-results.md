# Resultado de implementación de la Fase 1

Fecha: **2026-11-13**. Producto: **0.1.0-alpha.1**, objetivo **6.61**.
Estado: **implementación preparada para pruebas; no validada en PSP física**.

## Cambios entregados

Código PRO-C importado de `d9435ff4af6ba60466b46019393f6bbd1fc72ac0` y marca
PRO-X aplicada únicamente a las superficies previstas. Se conservan rutas,
exports/NIDs, estructuras, offsets, lógica del instalador, versiones HEN
`0x1003` / `0` y espacio de instalación PRO. La versión de producto se guarda en
documentación y manifiesto, separada de los campos de compatibilidad.

Se incluyeron el port Python de construcción, toolchain oficial por digest,
scripts de build/verificación/empaquetado, manifiestos, fuentes compilables y
avisos originales. No se implementaron funciones WPA2, actualización en línea,
almacenamiento, IPL ni modificaciones de Infinity.

## Registro de desarrollo

| Comprobación | Estado | Evidencia |
| --- | --- | --- |
| Fijar e inventariar upstream | PASS | 499 archivos, `upstream-files.json` |
| Compilación limpia de referencia antes de la marca | PASS | `artifacts/logs/reference-e55446004bf5.log` |
| Compilación de referencia final, orden Common de upstream | PASS | `artifacts/logs/reference-99f0c04040b0.log` |
| Compilación limpia PRO-X final | PASS | `artifacts/logs/pro-x-920d0b156d92.log` |
| Cuatro tests de port PRX y cabecera PBP | PASS | `python -m unittest discover -s tests -v` |
| Alcance del diff y contratos conservados | PASS | `scripts/verify.py`, `artifacts/verification.json` |
| Títulos reales PARAM.SFO de ambos EBOOT | PASS | `661 PRO-X Installer`, `661 PRO-X Fast Recovery` |
| Textos en PRX y encabezados de 14 traducciones | PASS | Recursos generados + comparación de bytes |
| Capacidad de cadena del sistema | PASS | Igual longitud efectiva; 22 bytes UTF-16 con NUL |
| Módulos recién compilados incorporados en Installer | PASS | Headers bin2c y bytes del PRX; hashes registrados |
| Launcher y Fast Recovery coherentes | PASS | Payload Installer/Rebootex, bootstrap PXE, buffer live compartido |
| Inventario y hashes del ZIP, fuente acompañante | PASS | `package.py` y `verify-package.py`; lista explícita sin IPL, Infinity u OFW |
| Reconstrucción con los insumos del archivo fuente | PASS | `source-rebuild.json`, log `source-rebuild.log`; ambos EBOOT idénticos byte por byte |

Comandos de build, exclusiones y limitaciones: [build.md](build.md).
Los logs de intentos con GCC 15 son fallos de diagnóstico, no resultados PASS.
Las advertencias heredadas de upstream se conservan en los logs.
La primera prueba del archivo fuente detectó la omisión de `Vshctrl`; se añadió
y se repitió con éxito la reconstrucción aislada. Los iconos y el fondo gráficos
originales de Team PRO se conservan; la identidad XMB PRO-X está en PARAM.SFO.

## Identidad exacta del firmware

- Árbol de referencia (con ajustes de herramientas):
  `f801f6d8c6b7c3fe4524c970f6d678c30cafb36c5e5d62bdc0dc2ef8d897a7c6`.
- Árbol PRO-X:
  `1c590a0f3922b1c18f803cfa7a8c736cc82e5949c672c21b635ae2ce2ffb6a54`.
- PROUPDATE/EBOOT.PBP:
  `b58b860437d1ccd9c5f64c47ead1329d4355bffdc049a97a18aac3bbcb2a8e14`.
- FastRecovery/EBOOT.PBP:
  `9e2fc87e3d275388c051b1179c9f1978c39956b363c7e2ac47e80546e9e3e56d`.

El algoritmo del hash de árbol es SHA-256 del JSON de rutas/SHA-256, ordenado y
sin espacios, definido en `scripts/build.py`. El manifiesto contiene el
inventario íntegro. El HEAD Git registrado antecede a estos cambios sin commit;
no debe usarse solo para identificar los binarios.

Artefactos: `artifacts/pro-x-0.1.0-alpha.1-661.zip`, su `-source.zip`,
`artifacts/pro-x-0.1.0-alpha.1-661.sha256` y `artifacts/manifest.json`.
El hash del ZIP se publica externamente para evitar una referencia circular.

## Pruebas que requieren PSP física

**NOT RUN**: instalación, identidad en pantalla, Fast Recovery, funciones
VSH/Recovery, arranque Infinity en tres ciclos de apagado completo, reinicio,
suspender/reanudar en XMB y aplicación, juego/homebrew, plugins, persistencia
de configuración y salida/recuperación específica.

No se dispone de modelo/generación, firmware actual ni versión binaria de
Infinity de una consola probada. La [ficha y matriz física](install-and-validation.md)
deben completarse antes de una prueba real. Infinity se inspeccionó en
`6a4f269f8abcf65696064cad94ba9ac8b845bd74`; se conserva su contrato PRO y se
prevé usar su selección **PRO**. Esto no acredita compatibilidad en ejecución.
No se usó un emulador como evidencia de instalación o Infinity.

# WPA2 integrado: estado y límites

Fase 2, **v0.2.0 experimental**, PSP **6.61**. Estado: **conexión reportada funcionando**.
La preferencia está habilitada por defecto y se admite una variante identificada
con el reporte autorizado de una PSP-2010. Las firmas se aplican por contenido
y layout, sin asociar el parche a una consola concreta. El usuario reporta conexión
en una red configurada solo con WPA2, 2.4 GHz y b/g/n, y fallo sin activar PRO-X.
No se aportó captura independiente del AP ni transferencia prolongada. La matriz
completa, Recovery y salida del diagnóstico v2 conservan sus pendientes.

## Implementación

`SystemControl/wpa2.c` usa la cadena existente de `syspatch.c`, antes de la
salida de sistema arrancado, también para cargas posteriores. No agrega exports,
NIDs, plugins ni cambia `SEConfig`, HEN o las rutas PRO/Infinity. Conserva el
handler anterior y su resultado. Solo sincroniza cachés cuando hay escrituras.
No modifica el módulo Sony en flash.

La preferencia se carga una vez al recibir `sceDisplay_Service` (punto que PRO
ya usa después de lflash), con `sceMediaSync` como segundo punto de inicialización
para GAME. No se lee flash en cada carga de red. Si apctl llegase antes, se
devuelve `WPA2_CONFIG_NOT_READY`: esa instancia queda intacta y no se intenta
parchearla después de arrancar. El orden real en XMB y GAME sigue pendiente
de observación física; no se afirma que esos puntos cubran todos los títulos.

`Common/wpa2_patch.c` adapta las 32 escrituras de Moment/ARK/wpa2psp. Los dos
destinos de datos (`0x11880`, 32 bytes, y `0x11da8`, 20 bytes) se comprueban
contra segmentos; el código se comprueba además contra `text_size`. El destino
del salto `text + 0xf464` se valida por separado, y sus instrucciones LUI/ORI
se calculan con la base de cada instancia. Se comprueban tamaño, firmware,
nombre, layout, alineación, límites, firmas originales y guardas antes de la
primera escritura. Solo se admite el conjunto enteramente original o enteramente
parcheado; una mezcla se rechaza sin escribir. No existe un booleano que impida
recargar otra instancia. Las fixtures de host son sintéticas, sin código Sony.
Cinco JAL originales se reconstruyen con destinos relativos al módulo, comprobando
la región MIPS: no se reutiliza la base del reporte de la PSP.

Resultados internos: `0 APPLIED`, `1 ALREADY_APPLIED`, `2 DISABLED`, `3 REJECTED`,
`4 CONFIG_NOT_READY`. Los mensajes `printk` están disponibles en un build DEBUG;
el build normal no guarda logs ni escribe archivos desde el callback.

## Variante identificada y evidencia

ARK y wpa2psp aportaron los reemplazos. El reporte autorizado, conservado en
artifacts/diagnostics y excluido de los ZIP/Git, permitió identificar originales
y layout. SHA-256 del reporte:
`43d1d38733d68b8dda59b3e728833964628774a5ac7150cdb0aafcbae76abffc`.
La firma `apctl-661-11e9c-160-6a60` exige text/data/bss `0x11e9c/0x160/0x6a60`,
segmentos relativos `0/0x11ec0` con tamaños `0x11e9c/0x6bc0`, todos los sitios
originales y tres guardas de 32 bytes en `0x100/0x200/0xeef0`.
Las llamadas JAL apuntan a `text+0x10e20` o `text+0xd600`; se excluye la guarda
candidata de `0xf440` por contener llamadas absolutas reubicadas.
El SHA-1 completo del texto ya reubicado no se utiliza para identificar variantes.
Las otras variantes permanecen sin parchear. Procedencia en
[upstream.md](upstream.md) y [wpa2-upstream.json](wpa2-upstream.json).
El orden de inicialización XMB/GAME y el comportamiento real de red requieren
todavía la matriz física; identificar bytes no demuestra un handshake WPA2.

## Recovery y persistencia

En **Configuration**, `WPA2 Support (restart required)` muestra la preferencia
Enabled/Disabled con los textos de estado del idioma actual. La nueva etiqueta
y los mensajes auxiliares usan inglés; las traducciones existentes y sus índices
permanecen intactos. Enabled expresa intención, no compatibilidad comprobada.
Cambiar la opción requiere **Reset device / reinicio completo del CFW**; salir de
Recovery o Reset VSH no garantiza recargar SystemControl. No se deshace un parche
en una conexión activa.

El archivo privado es `flash1:/prox_network.cfg`: 12 bytes, magic ASCII `PXNW`,
versión little endian de 32 bits `1`, byte Enabled `0` o `1` en posición 8, y
tres bytes reservados cero. Ausente: Enabled, sin crear archivo. Inválido, versión
desconocida o error de lectura: Enabled y diagnóstico de estado. Solo se guarda
al cambiar explícitamente la opción. No se modifican listas de plugins.

El guardado escribe `.tmp`, comprueba escritura/cierre/sync, mueve el archivo
anterior a `.bak` y confirma mediante rename. Si falla, mantiene el valor mostrado
y trata de restaurar el archivo anterior. Si esa restauración también falla,
la lectura recupera `.bak` cuando falta el archivo principal. El backup no se
usa para ocultar un archivo principal inválido. No se promete atomicidad frente
a pérdida de energía ni persistencia física hasta probar flash1 en una consola.

## Diagnóstico voluntario en PSP

El ZIP `pro-x-0.2.0-661-diagnostic.zip` es independiente del paquete de
instalación. Su programa **PRO-X WPA2 identification (read only)** carga los
módulos comunes/INET sin escanear ni conectar. Usa temporalmente el handler de
PRO para capturar datos antes del inicio y después restaura la cadena. Guarda
únicamente `PSP/GAME/WPA2DIAG/report.txt`, en ms0 o ef0. No escribe flash.

1. Registrar modelo/generación, firmware completo, versión y hash del build,
   método de arranque, Infinity y plugins. Arrancar una sesión nueva de PRO-X.
2. Desactivar manualmente cualquier plugin WPA2 externo antes del reinicio.
   No realizar una conexión antes del diagnóstico.
3. Copiar `PSP/GAME/WPA2DIAG` del ZIP diagnóstico y ejecutar el programa.
   Esperar el mensaje de guardado y pulsar X para volver, o esperar 15 segundos.
   v2 usa la interfaz kernel del mando y la salida de PRO, con lectura no bloqueante.
4. Conservar el reporte privado y repetir tras otro arranque/carga para comparar
   bases y relocaciones. Registrar por separado XMB y una aplicación compatible.

El reporte contiene firmware/HEN, layout y base de carga, SHA-1 del texto ya
relocado, extractos acotados de los 32 sitios y cuatro ventanas candidatas de
32 bytes para revisar guardas/relocaciones. Estas ventanas no son firmas aprobadas.
SHA-1 aquí es solo una referencia
diagnóstica; no es la firma de aceptación del parche ni un hash de seguridad del
paquete. No contiene perfiles, contraseñas, PSK, SSID ni datos de conexiones.
No publicar volcados completos ni incorporar binarios Sony al repositorio.
Si no captura el módulo o falla el guardado, registrar el fallo y no autorizar
otra variante. La captura v1 fue correcta en PSP-2010; su salida con X falló.
La corrección v2 y su retorno automático tienen prueba física pendiente.
v2 informa coincidencias del payload después del handler anterior: 32/32 es
evidencia de bytes modificados, no de negociación WPA2 o transferencia.

## Alcance de red cuando se valide

Objetivo: WPA2-Personal AES/CCMP en un AP de prueba de **2.4 GHz**. El parche de
referencia reemplaza WPA1; Disabled y reinicio recuperan el comportamiento original.
La interfaz puede conservar una etiqueta WPA existente: no se ha observado todavía
el texto real en PRO-X. No se promete compatibilidad universal con juegos que
incluyan otro módulo, routers, red abierta/WEP o modelos no probados.
Street/E1000 carece de Wi-Fi. WPA3, Enterprise/802.1X, 5 GHz, PMF obligatorio,
TLS moderno y actualización desde GitHub quedan fuera de esta fase.

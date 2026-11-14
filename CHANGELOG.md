# Changelog

## Unreleased

### v0.3.0-beta.1 — experimental prerelease

This prerelease distribution preserves the tested v0.3.0 firmware build
`pro-x-af71b32d7aaf` and its matching package filenames. The beta suffix is the
distribution tag; the executables still report 0.3.0. This preparation does not
establish completion of pending hardware acceptance tests.

- Recreate net/Inet/APCTL/resolver between eligible pre-TLS retries, waiting for
  disconnection before reinitializing and reconnecting the same saved profile.
  Track partial cleanup/init ownership; retain the original deadline and stop
  on unsafe cleanup or cancellation. Reuse PSP's last valid network-profile ID
  instead of asking again; show the saved-profile selector if unavailable.
  Firmware drivers, the physical WLAN switch and stored profiles are unchanged.
  Fourteen supplied PSP reports from this candidate complete authenticated catalog
  checks on attempt one, with automatic profile reuse and checked teardown.
  Fresh-session recovery, cancellation and broader profile cases still need
  physical validation; these runs do not establish the DNS root cause.
- Retry pre-TLS DNS/TCP connection failures up to five attempts, with a one-second
  cancellable pause and four extra attempts shared across the updater run.
  Keep the original request deadline. Stop on success, cancellation, unsafe
  cleanup or internal errors; do not retry certificate, HTTP/rate-limit, metadata
  or package-verification failures. The earlier retry build has eight successful
  PSP catalog checks in nine supplied runs, including one recovery on attempt
  two and one failure exhausting all five attempts; reliability remains partial.
- Distinguish PSP DNS worker wait errors, cancellation and deadlines, and record
  checked stop/join results plus the APCTL state. Await disconnection and check
  every network termination/unload before installer handoff; retain dependencies
  and block handoff on failure. DNS settings, request limits and HTTPS checks are
  unchanged. This hardening is not a proven fix of intermittent DNS failures.
- **v0.3.0:** open the PRO-X updater from XMB System Update -> Update via Internet.
  Remove the inherited PRO HTTP update destination without a Sony fallback.
- Select numerically newer, compatible stable GitHub releases using explicit
  draft/prerelease flags and exact same-release update assets. Reject downgrades,
  ambiguous metadata and unsupported firmware/model/client contracts.
- Stream and verify package/file sizes and SHA-256 in private staging. Reject
  unsafe paths, links, duplicates, unexpected files, malformed ZIP/PBP structures,
  truncated downloads and unsupported compression before installation.
- Hand verified packages to the existing PRO installer after network shutdown.
  Recheck the manifest/files and power; preserve settings, plugins and PRO/Infinity
  contracts. OTA disables uninstall/forced-reset controls and avoids overwriting
  the executing client. Recover storage renames with exact backups when possible;
  flash writes retain the base installer's nontransactional failure limitations.
- Generate matching manual/source/OTA ZIPs, the schema-1 update manifest, build
  identity and hashes locally. Initial updater installation requires the manual
  package. The current OTA model declaration is limited to the observed model ID 1.
- Fix updater X/directional input, live firmware/model detection, SHA-384 CA-chain
  support and GitHub's long response headers while retaining mandatory certificate,
  hostname, date and HTTPS redirect validation. Add bounded private reports.
- Show `6.61 PRO-X 0.3.0` in System Information using a separately sized UTF-16
  buffer and corrected guarded hook. Rendering works on the owner's PSP;
  Sony/HEN compatibility values remain unchanged.
- Three owner-supplied prototype reports confirm authenticated API access and
  redirected asset size/SHA-256 verification. Normal manual installation/start
  and full catalog checks are also owner-reported, including three first-attempt
  successes with the retry-capable client. Earlier builds still have DNS failures;
  successful checked teardown is observed in all six latest supplied reports.
  Fresh-session retry recovery, positive official stable installation and second update remain
  pending/blocked; no resolved DNS cause, universal compatibility or completed
  hardware acceptance is claimed.
- Existing releases and published assets are preserved.

- Add a standalone PSP network diagnostic with private connection/DNS/HTTP logs,
  gateway checks, Cloudflare/Quad9 comparisons and bounded waits. It does not
  modify firmware or network profiles.
- Repeated owner reports from the same diagnostic build confirm intermittent
  data connectivity after IP readiness: successful gateway/DNS/Sony probes in
  one run and gateway timeouts in others. The XMB Internet test has both failed
  and succeeded; the cause remains open. Sustained transfer, cancellation and
  XMB return remain unvalidated.
- Add supervised Sony HTTP-library probes alongside raw HTTP, recording setup,
  send/read errors and network errno. Both HTTP 1.0 and 1.1 probes complete with
  the expected response in one owner report; other runs return `80410410` while
  gateway/raw probes also time out. This does not reproduce or fix the XMB test.
- Record a later owner-supplied successful XMB Internet test with UPnP enabled
  and NAT type 2. A focused review found no confirmed PRO-X defect explaining
  the intermittent failures; PSP/router compatibility remains an unconfirmed
  working hypothesis. No firmware fix or universal compatibility is claimed.

- Require English for all GitHub-bound documentation and publication text, including READMEs, changelogs, commits, pull requests, release notes and artifact documentation.

## 0.2.0 — 2026-11-13

- El usuario reporta conexión satisfactoria en PSP-2010 con una red 2.4 GHz,
  b/g/n y WPA2 solamente, y fallo de esa conexión al arrancar sin activar PRO-X.
  Validación física parcial reportada; transferencia prolongada, Recovery,
  suspensión/reconexión, Infinity y otros AP/modelos siguen pendientes.

- Organización local: entregas finales en `builds/`, registros y temporales en
  `artifacts/`; exclusiones de Git ampliadas y política de artefactos documentada
  en `AGENTS.md`. Se eliminan paquetes/hashes/manifiestos finales duplicados de
  `artifacts/` después de verificar sus copias. Sin cambios en firmware ni versión.

- Fase 2, **v0.2.0 experimental**: soporte WPA2 de Moment/ARK/wpa2psp integrado
  y habilitado por defecto para una variante 6.61 identificada con el reporte
  de una PSP-2010. Se comprueban contenido, límites y direcciones relativas
  antes de modificar el módulo; las variantes desconocidas se omiten.
- Requisitos de la red de prueba documentados: 2.4 GHz, compatibilidad 802.11b
  habilitada en el AP y WPA2-Personal AES/CCMP; canal/ancho y parámetros del
  router se registran por separado del resultado de conexión.
- Preferencia privada habilitada por defecto y opción de Recovery con efecto
  tras reiniciar, sin modificar SEConfig, exports/NIDs o contratos PRO/Infinity.
- Diagnóstico PSP voluntario separado del instalador; pruebas C sintéticas,
  compilación, procedencia fijada, manifiesto y documentación de pendientes.
- Diagnóstico: lectura del mando mediante la interfaz kernel, espera limitada
  a 15 segundos y salida por la API de PRO. Corrección de salida pendiente
  de comprobación física; reporte ampliado con coincidencias del parche.
- Se conserva el alcance limitado del reporte físico; no se anuncia compatibilidad
  universal. Infinity y los demás pendientes físicos de Fase 1 se conservan.

- Fix: paquetes ZIP, logs, manifiestos y demás salidas generadas de `artifacts/`
  dejan de seguirse en Git y se conservan localmente para su publicación en Releases.
- Reglas de exclusión para artefactos, carpetas de release y salidas de compilación.
- Enlaces de descarga del README dirigidos a Releases, separados del código fuente.
- Registro del toolchain conservado como documentación para que el empaquetado
  no dependa de metadatos generados incluidos en el checkout.

## 0.1.0-beta.1 — 2026-11-13

- Versión/tag comunicado por el responsable del proyecto: `v0.1.0-beta.1`.
- Primer reporte del usuario de pruebas en PSP física: instalación,
  Recovery y suspensión funcionando.
- Actualización del estado público y de los registros de validación para
  distinguir los resultados reportados de las comprobaciones de desarrollo.
- Esta promoción a beta no introduce cambios funcionales de firmware respecto
  de la implementación de Fase 1 de `0.1.0-alpha.1`.

Estado: **beta con validación física parcial reportada por el usuario**.
Pendientes: Infinity, juegos y plugins; también las pruebas restantes de la
matriz que no se han comunicado. No se especificaron el modelo, la versión
completa del firmware, el hash ensayado, el tipo de Recovery ni el contexto de
suspensión. No se extiende el resultado a todos los modelos o configuraciones.
Véase el [reporte físico](docs/hardware-report-0.1.0-beta.1.md).

Los ZIP locales conservan el nombre y manifiesto `0.1.0-alpha.1` con el que se
generaron. Esta actualización documental no genera ni publica un paquete beta.

## 0.1.0-alpha.1 — 2026-11-13

- Base PRO-C fijada en `d9435ff4af6ba60466b46019393f6bbd1fc72ac0`.
- Identidad visible PRO-X en sistema 6.61, Installer, Fast Recovery,
  VSH, Recovery y encabezados de sus traducciones incluidas.
- Compilaciones limpias de referencia y PRO-X con toolchain fijado;
  port mínimo de scripts Python y adaptación de una opción obsoleta de GCC.
- Empaquetado explícito experimental, manifiesto de fuentes/herramientas,
  SHA-256 y comprobaciones de títulos, módulos y contratos conservados.
- Documentación de instalación condicional y matriz de pruebas físicas.

Estado: **implementación preparada para pruebas; hardware pendiente**.
No se ha validado instalación, ejecución ni arranque automático con Infinity.
Los valores HEN y las interfaces/rutas internas PRO permanecen intactos.
Se conservan componentes precompilados heredados de upstream, incluido March33.
No se incorpora WPA2, actualización en línea, almacenamiento nuevo ni cambios IPL.

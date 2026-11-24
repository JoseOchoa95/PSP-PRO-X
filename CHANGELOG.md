# Changelog

## Unreleased

### v0.4.0 — self-contained native update development

- Add a local query-only PSP diagnostic that measures worker startup, free kernel and user memory before and after load, and authenticated query duration. It uses the current card-based worker and blocks installation from System Update.
- Add a PSP-2000 6.61 resident query test kit with matching Installer and Fast Recovery. The Installer provisions a build-specific flash0 worker. System Update verifies that worker's SHA-256 and uses its embedded CA bundle without a PROXUPDATE folder.
- Remove the charger and minimum battery charge requirements from the resident diagnostic's manual Installer.
- Check actual flash0 free space before manual resident installation. Account for the worker, every target module and directory growth in allocation units, and stop before writing if space is insufficient or the measurement fails.
- Check flash0 space for staged firmware, destination files and backups before the first write of a background Internet update. The calculation passed on one PSP-2000 using a simulated offer. A real Internet installation with the new check remains pending.
- Generate a dedicated data-only update EBOOT from the same build as the manual Installer and Fast Recovery. The owner reported that the read-only PSP tool verified the package and all twelve components.
- Add a resident System Update preview for the distinct dedicated EBOOT contract. Verification of the full staged package and removal of the temporary files passed on one PSP-2000 with a synthetic offer.
- Add dedicated EBOOT writer support for the matching flash0 worker and its allocation costs. Isolated tests use the compiled EBOOT to check backup verification, rollback and cleanup gated on the target build. The PSP installation path remains pending.
- Prepare all twelve dedicated EBOOT components as a private install job after consent. Reopen each extracted file and require its size and digest to match before handing off the job. The Settings preview cleans up the test stage after verification.
- Recheck a dedicated job's EBOOT and all staged components at writer entry. Test persisting the committed card inventory and retrying exact file cleanup after observing the target build. Preserve changed or unrelated files and keep the installed worker.
- Exercise the original Settings handoff with the resident EBOOT preview. Writer revalidation of the consented job and stage removal passed on one PSP-2000 with a synthetic offer.
- Allow the resident test Installer to import the twelve original completion phrases from an exact owner-supplied 6.61 resource. Budget and verify the compact internal data before using it. The background completion path can read the installed phrases without the original file on the card. Physical provisioning and rendering remain pending.
- Add a local writable dedicated-update candidate that binds the consented package and resident worker to the native install job. Require verified original completion data and flash0 capacity before replacement. Keep backups and the card stage available for restart verification.
- After restarting into the verified target, use the first authenticated System Update check to retire the update's flash0 sidecars and remove the temporary EBOOT and stage from the Memory Stick. Preserve changed files and retry deferred cleanup on a later check. Physical validation remains pending.
- Verified one physical authenticated query using the internal worker with the Memory Stick inserted and the PROXUPDATE folder absent. The owner reported the expected no-newer-version result on the first query without the Memory Stick. The original completion resource, legacy upgrade path and cleanup after restarting into the new firmware remain pending.
- Add a standalone read-only flash0 capacity diagnostic that uses the USB driver's kernel-mode geometry query and counts free allocation units.
- Return to the PSP menu from the kernel-mode capacity diagnostic when X is pressed or the inactivity timer expires.

## v0.3.4 — native background installation

- Install verified GitHub updates in the background after one acceptance on the original PSP update screen, preserving system language/theme and avoiding the target installer's text console and additional installation prompts.
- Show original localized completion after verified firmware/storage success, hide Back on that page and wait for a fresh X to restart; no automatic restart timer.
- Replace unsupported firmware renames with bounded verified copies, exact original backups and checked native flush/restoration. Retain recovery evidence on failure and never report an incomplete update as successful.
- Preserve configuration, plugins, PRO/HEN/Infinity contracts and the existing HTTPS, redirect, stable-release, numeric-version, compatibility and post-offer storage/power checks. Keep manual Installer controls unchanged.
- Verify the authenticated target as signed data and prepare its matching modules/rebooter before replacement; correct compressed signer-template selection and retain precise failure logs.
- Limited owner PSP acceptance: the temporary 0.3.2 build updated to unchanged stable 0.3.3; final 0.3.4 displayed the correct version, opened the SELECT menu and reported no newer update. Successful-run raw logs were not supplied; broader hardware/router and Infinity validation remain pending.
- The exact original completion RCO remains a separate runtime requirement and is not bundled. Firmware copies are not atomic against power loss; powered-on card reinsertion and the self-contained update redesign remain deferred. Older installed workers retain their existing update behavior.

### v0.3.3 — native GitHub System Update

- Integrate authenticated GitHub checking, compatible stable offers and verified download progress with the original PSP System Update screens, preserving the installed language and theme.
- Reuse the connection selected in XMB and renew the same profile between bounded DNS/TCP retries without tearing down shared network libraries.
- Check update eligibility before requiring download storage or installation power; use the original localized storage/start-error messages after a real offer and download acceptance.
- Prepare the matching worker and pinned public CA bundle in RAM for checking after storage removal from an initialized CFW session. Correct the native 6.61 buffer-loader ABI and preserve legitimate PAF variable imports.
- Bind consent to the exact compatible manifest and verify HTTPS certificates, redirects, versions, sizes, SHA-256 and staged files before handing off to the existing PRO-X installer. Reject drafts, prereleases, downgrades and unsafe packages without Sony fallback.
- Make the real native flow the normal build and generate matching manual/source packages, OTA manifest/update ZIP and hashes. Keep temporary versions and simulated/query-only diagnostics excluded from stable distribution.
- Record limited owner-reported success for missing-power/storage errors, a real temporary 0.3.0 -> 0.3.1 update, and the final 0.3.3 System Information label/no-update result. Retain the existing text installer and X prompts; native installer/silent restart, Infinity and powered-on storage-reinsertion routing remain outside this release.
- Require unique version assignments across changes and release descriptions that do not repeat their title.

### v0.3.2 — original System Update UI diagnostic checkpoint

- Add an opt-in diagnostic that preserves the original PSP update UI, entry point and installed language/theme resources, returning an explicitly simulated latest-version result without HTTP requests, package downloads or installation through that query.
- Validate all ten inspected provider imports and register the diagnostic syscalls separately; reject incomplete/unknown layouts before patching. Keep the existing updater as the default and preserve Installer, Fast Recovery, configuration, plugins and PRO/Infinity contracts.
- Prepare matching local Installer/Fast Recovery packages, source, manifest and hashes. The owner reports that the isolated build passed its installation/native-query check; the earlier prototype's native message/language/theme behavior also passed owner testing. Real GitHub/native installer integration is outside this checkpoint.
- Require continuous Markdown prose paragraphs in GitHub publication text, preserving intentional paragraph breaks, lists, tables, code blocks and formatting. No release or new product-version increment is authorized by this documentation rule or PR.

### v0.3.1 — version and release-packaging chore

- Require owner acceptance and merge of the corresponding PR before publishing
  any release or exposing its assets to automatic updates; separate release
  authorization and validation requirements still apply.
- Advance the product build to v0.3.1 across Installer, Fast Recovery, updater,
  System Information (`6.61 PRO-X 0.3.1`) and matching package metadata.
- Prepare the same-release update manifest and dedicated OTA ZIP for a normal
  release, allowing an existing 0.3.0 client to discover the newer version.
- Require those OTA assets and an up-to-date PSP System Information label in
  agent release instructions; verify actual generated packages with an
  actual-version native installer preflight/storage fixture, without flash.
- Keep the updater/network behavior and PRO/Infinity compatibility contracts
  unchanged. New-version console installation, restart and follow-up query
  remain pending; the earlier 14 successful catalog checks belong to 0.3.0.

- Require the exact update manifest and dedicated OTA ZIP on every new stable
  firmware release, with same-build compatibility/hash checks before publication.
  Manual-test prereleases remain excluded from stable automatic updates.

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

# PRO-X

A community-driven PSP custom firmware project based on PRO-C, targeting system software 6.61.

**AI-assisted development:** PRO-X is developed with assistance from AI, including Codex. Original PRO-C code and third-party components retain their authorship, credits and licenses.

## System Update

Product v0.3.3 uses **Settings -> System Update -> Update via Internet** to check public [PRO-X GitHub releases](https://github.com/JoseOchoa95/PSP-PRO-X/releases), offer a newer compatible stable product version and download/verify its dedicated update package through the original PSP interface. Installed system language and theme resources remain in use. Drafts, prereleases, equal/older versions, insecure destinations and incompatible or ambiguous contracts are rejected. HTTPS certificate, hostname, date, redirect and digest checks remain mandatory; Sony update servers are not a fallback.

The query reuses the XMB connection. Installation storage, free space, AC and battery readiness are required after an eligible offer and download acceptance, with rechecks before handoff. After successful CFW initialization with the matching local client available, a RAM-prepared worker and its compiled public CA bundle permit checking without writable removable storage. This does not provide cold boot or installation without storage. If the client cannot be prepared, a failed query remains an error.

The download shows original native progress and launches only the verified PRO-X job. The destination's existing installer still uses its text screen and X confirmations. A new native installer UI, single-confirmation installation and automatic restart are not included in this version. Configuration, plugin paths and existing PRO/HEN/Infinity contracts are retained; Infinity startup and powered-on storage-reinsertion routing are not validated by this change.

Published v0.3.4 adds a matched background writer for Internet updates after the original offer is accepted, without a text console or additional installation prompts. Original native completion waits for a fresh X to restart. The owner reports a successful temporary 0.3.2 -> stable 0.3.3 background update and correct version/menu/no-newer checks on final 0.3.4. This is limited owner acceptance. The owner-supplied original completion resource remains a separate runtime dependency and is excluded from the package. Initial manual setup and updates initiated by older workers retain their existing controls.

PRO-X v0.4.0 adds a resident updater for PSP-2000 on 6.61. Its manual Installer measures internal free space before writing and provisions a verified internal worker. The manual Installer can run without an AC or minimum battery charge requirement. System Update uses the original screens for authenticated catalog queries. A previous diagnostic on one PSP-2000 authenticated to GitHub with the Memory Stick inserted and the PROXUPDATE folder absent. The Internet update writer checks space for staged firmware and backups before opening flash0 for writing. Full Internet installation on the v0.4.0 build remains pending on PSP.

PRO-X v0.4.1 includes completion messages for the twelve PSP system languages in the resident updater. The manual ZIP supplies the files for first installation and Internet updates. After activation, the user can remove PROUPDATE and keep Fast Recovery on the Memory Stick. The owner confirmed the `6.61 PRO-X 0.4.1` label on physical hardware. Fast Recovery reactivation and a complete Internet update remain pending.

## Validation and compatibility

The local v0.4.0 dedicated update test package contains a data-only EBOOT built from the same firmware components as its manual installation kit. The owner reported that the read-only app verified the package and all twelve components on one PSP-2000. With a synthetic offer on that model, System Update retained the verified installer job until the original Settings handoff, passed the shared writer's read-only verification and removed its temporary card stage. Host tests use the compiled EBOOT to check recovery and show that a committed record supports retryable cleanup after observing the target build. Installation through System Update and cleanup after a physical PSP restart remain pending.

The v0.4.0 resident Installer can import the original 6.61 completion phrases once from a verified owner-supplied resource and retain 522 bytes internally for later language selection. The dedicated updater uses this verified text and measures flash0 before replacing modules. On startup in the XMB, the updater can verify the installed build, retire the update's flash backups, and remove the staged EBOOT from the Memory Stick without an Internet check. Physical dedicated installation, native rendering and post-restart cleanup remain pending.

The owner reports that the matching temporary v0.3.0 build correctly found the existing v0.3.1 target, displayed native errors for missing AC/storage after the offer, and completed the update with those conditions satisfied. The final v0.3.3 build's `6.61 PRO-X 0.3.3` label and no-newer-update result also passed owner testing. These are limited owner reports, not a universal hardware guarantee. The OTA contract currently restricts eligible models to model ID 1; do not infer support for every PRO-C model or router from a single console.

The existing WPA2 integration has a limited owner-reported connection result on PSP-2010. Use a 2.4 GHz network allowing legacy 802.11b clients; g/n-only or n-only modes are insufficient. The tested report used mixed b/g/n and WPA2-only security. Broader router/cipher/model coverage remains unvalidated.

## Install and activate

1. Use a PSP already running system software **6.61**. Download the matching manual binary ZIP and verify its SHA-256. See [Releases](https://github.com/JoseOchoa95/PSP-PRO-X/releases) for published downloads.
2. Extract the ZIP's `PSP` folder to the Memory Stick root. The v0.4.1 manual kit supplies `PROUPDATE` and `FastRecovery`.
3. Open PROUPDATE from Games. Press X to install, wait for completion and press X to start PRO-X. Check **Settings -> System Settings -> System Information** for the installed version, such as `6.61 PRO-X 0.4.1`. After activating v0.4.1, you can remove `PSP/GAME/PROUPDATE`.
4. After a full shutdown, use the matching Fast Recovery to activate PRO-X. For an Internet update, use System Update with a Memory Stick and meet the displayed storage and power prerequisites.

Use the manual v0.4.1 kit if you are on official 6.61, on a pre-v0.4.0 PRO-X version or on v0.4.0 without its previously installed completion resource. The dedicated Internet update path requires a resident client and a newer compatible target.

PRO-X writes firmware modules and reuses PRO-C paths; it does not install alongside PRO-C. It does not install, remove or configure Infinity/cIPL. Do not uninstall existing CFW or reflash official firmware as a routine update step. Preserve previous working packages and follow the installer result instead of assuming a successful reboot after an error.

Resident releases starting with v0.4.0 use the matching `pro-x-dedicated-update.json` and data-only `EBOOT.PBP`. The earlier published releases retain their original ZIP and manifest. Stable publication requires the matching source and binary archives, manifests, hashes and hardware validation after an accepted merge and explicit owner authorization.

## Build and contribute

Build with the pinned Docker toolchain; generated firmware and packages stay in ignored `builds/`, with intermediate trees/logs in `artifacts/`.

```text
docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts
python scripts/build.py reference
python scripts/build.py pro-x --resident-dedicated-ota
python scripts/package.py
python scripts/verify-package.py builds/resident-release-<build-id>/pro-x-0.4.1-661.zip
```

Use the writable resident build when staging a v0.4.0 or later release. The query-only, simulated-offer and incomplete-progress options remain available as diagnostics. Build and packaging commands work locally. Follow [AGENTS.md](AGENTS.md) for validation, versioning and Git/release authorization.

## Credits and references

PRO-X is an independent fork and is not an official release from Team PRO, the ARK team, or the Infinity developer.

- **Team PRO**, including Coldbird and Virtuous Flame, for the original PRO custom firmware. [PRO source](https://github.com/MrColdbird/procfw)
- **Davee** for Infinity. [Infinity](https://github.com/DaveeFTW/Infinity)
- **ARK contributors**, **Moment** and **wpa2psp contributors** for the WPA2 payload adapted in phase 2. [ARK-5](https://github.com/PSP-Arkfive/ARK-5) · [wpa2psp](https://github.com/Kethen/wpa2psp)

Original copyright notices, license notices, and contributor acknowledgments must be retained.

## License

PRO-X is licensed under the **GNU General Public License, version 3 or, at your option, any later version** (`GPL-3.0-or-later`), consistent with the PRO source notices.

Include the full license text in `LICENSE`. Third-party components retain their applicable licenses and notices. Distributed binaries must be accompanied by access to their corresponding source in accordance with the GPL.

See the [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html) for the full terms. The software is provided without warranty, as described in the license.


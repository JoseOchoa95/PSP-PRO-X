# PRO-X

A community-driven PSP custom firmware project based on PRO-C, targeting system software 6.61.

**AI-assisted development:** PRO-X is developed with assistance from AI, including Codex. Original PRO-C code and third-party components retain their authorship, credits and licenses.

## System Update

Product v0.3.3 uses **Settings -> System Update -> Update via Internet** to check public [PRO-X GitHub releases](https://github.com/JoseOchoa95/PSP-PRO-X/releases), offer a newer compatible stable product version and download/verify its dedicated update package through the original PSP interface. Installed system language and theme resources remain in use. Drafts, prereleases, equal/older versions, insecure destinations and incompatible or ambiguous contracts are rejected. HTTPS certificate, hostname, date, redirect and digest checks remain mandatory; Sony update servers are not a fallback.

The query reuses the XMB connection. Installation storage, free space, AC and battery readiness are required after an eligible offer and download acceptance, with rechecks before handoff. After successful CFW initialization with the matching local client available, a RAM-prepared worker and its compiled public CA bundle permit checking without writable removable storage. This does not provide cold boot or installation without storage. If the client cannot be prepared, a failed query remains an error.

The download shows original native progress and launches only the verified PRO-X job. The destination's existing installer still uses its text screen and X confirmations. A new native installer UI, single-confirmation installation and automatic restart are not included in this version. Configuration, plugin paths and existing PRO/HEN/Infinity contracts are retained; Infinity startup and powered-on storage-reinsertion routing are not validated by this change.

## Validation and compatibility

The owner reports that the matching temporary v0.3.0 build correctly found the existing v0.3.1 target, displayed native errors for missing AC/storage after the offer, and completed the update with those conditions satisfied. The final v0.3.3 build's `6.61 PRO-X 0.3.3` label and no-newer-update result also passed owner testing. These are limited owner reports, not a universal hardware guarantee. The OTA contract currently restricts eligible models to model ID 1; do not infer support for every PRO-C model or router from a single console.

The existing WPA2 integration has a limited owner-reported connection result on PSP-2010. Use a 2.4 GHz network allowing legacy 802.11b clients; g/n-only or n-only modes are insufficient. The tested report used mixed b/g/n and WPA2-only security. Broader router/cipher/model coverage remains unvalidated.

## Install and activate

1. Use a PSP already running system software **6.61**. Download the matching binary ZIP and SHA-256 list from [Releases](https://github.com/JoseOchoa95/PSP-PRO-X/releases); source ZIPs are for building. Keep storage and AC connected with at least 50 percent battery during installation.
2. Copy all three matching folders into `PSP/GAME`: `PROUPDATE`, `FastRecovery` and `PROXUPDATE`, including `PROXUPDATE/cacert.pem`. Preserve configuration, plugin lists, games and reports.
3. Open the matching PRO-X Installer from Games. Press X to install, wait for completion and press X to start PRO-X. Check **Settings -> System Settings -> System Information** for `6.61 PRO-X 0.3.3`.
4. After a full shutdown, use the matching Fast Recovery when PRO-X is not already active. For Internet updates, use the Settings route above. Leave the matching package available while CFW starts and allow ten seconds in inactive XMB for RAM preparation.

PRO-X writes firmware modules and reuses PRO-C paths; it does not install alongside PRO-C. It does not install, remove or configure Infinity/cIPL. Do not uninstall existing CFW or reflash official firmware as a routine update step. Preserve previous working packages and follow the installer result instead of assuming a successful reboot after an error.

Future stable Internet-update releases must attach the matching `pro-x-update.json` and dedicated `pro-x-<version>-661-update.zip` to the same non-draft, non-prerelease release, together with the manual binary/source ZIPs, build manifests and hashes. A manual/source/diagnostic ZIP is not an OTA substitute. Publication follows an accepted merged PR and explicit owner authorization.

## Build and contribute

Build with the pinned Docker toolchain; generated firmware and packages stay in ignored `builds/`, with intermediate trees/logs in `artifacts/`.

```text
docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts
python scripts/build.py reference
python scripts/build.py pro-x
python scripts/package.py
python scripts/verify-package.py builds/pro-x-0.3.3-661.zip
```

The normal PRO-X build enables the real native updater. Explicit query-only, simulated-offer and incomplete-progress options remain separate diagnostic builds and cannot generate stable OTA assets. No build or packaging command publishes a release. Follow [AGENTS.md](AGENTS.md) for validation, versioning and Git/release authorization. Contributions should preserve upstream credits, existing compatibility contracts and exact-build evidence.

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


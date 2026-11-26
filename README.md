# PRO-X

A community-driven custom firmware based on PRO-C for PSP-2000 consoles running Sony system software 6.61. [Download the latest stable release](https://github.com/JoseOchoa95/PSP-PRO-X/releases/latest)

**AI-assisted development:** PRO-X is developed with assistance from AI, including Codex. Original PRO-C code and third-party components retain their authorship, credits and licenses.

## Features

- **Integrated WPA2 Wi-Fi.** Connect to WPA2-Personal networks on the supported 6.61 Wi-Fi module variant. The Recovery menu lets you change the WPA2 setting. Restart the PSP to apply it.
- **Internet updates from the PSP menu.** **Settings → System Update → Update via Internet** checks GitHub for a newer compatible stable release. The updater uses HTTPS and verifies the downloaded package before installing it in the background. Progress and restart prompts use the PSP interface, with completion text for all twelve system languages.
- **Storage checks and update cleanup.** The update offer shows the Memory Stick space needed for staging. The installer checks free space in internal storage before writing. The startup cleaner verifies the installed build before removing temporary firmware files, the downloaded EBOOT and the updater's `PROXDIAG` files from the card.
- **PRO-C game and plugin features.** Launch PSP ISO and CSO images from the Memory Stick with selectable ISO drivers. Manage XMB, game and POPS plugins through the existing `seplugins` configuration.
- **VSH and Recovery menus.** Adjust clock speeds and USB device selection, with optional translations and fonts.

## Installation guide

### First installation

1. On a PSP-2000 running Sony system software **6.61**, download the manual binary ZIP named `pro-x-<version>-661.zip` and verify its SHA-256. [Releases](https://github.com/JoseOchoa95/PSP-PRO-X/releases)
2. Copy `PSP` from the ZIP's `release/` directory to the Memory Stick root. Copy `SEPLUGINS` from the same directory for optional menu translations and fonts.
3. Open **PROUPDATE** from Games. Press X to install, wait for completion and press X to start PRO-X. Check **Settings → System Settings → System Information** for the installed version.
4. After activation, you can remove `PSP/GAME/PROUPDATE`. Keep the matching **Fast Recovery** to reactivate PRO-X after a full shutdown.

### Update an existing installation

With a compatible resident version of PRO-X active, open **Settings → System Update → Update via Internet**. Use a Memory Stick with the displayed free space, charge the battery to at least 50% and connect DC IN power. Keep the card and charger connected through the restart. Use the manual installation steps above if you are on an earlier version than v0.4.0 or if your v0.4.0 installation has no completion resource.

PRO-X writes firmware modules and reuses PRO-C paths; it does not install alongside PRO-C. It does not install, remove or configure Infinity/cIPL. Do not uninstall existing CFW or reflash official firmware as a routine update step. Preserve previous working packages and follow the installer result instead of assuming a successful reboot after an error.

## Validation and compatibility

On one PSP-2000, the Internet update from v0.4.2 installed the published v0.4.3 build. Read-only checks after restart confirmed that `PROXDIAG` and the temporary firmware files were removed. [Version history](CHANGELOG.md)

WPA2 was tested on a PSP-2010 with a 2.4 GHz WPA2-only access point allowing 802.11b clients. Other router configurations and Wi-Fi module variants need further testing. Infinity startup and broader game and plugin compatibility testing also remain pending. [WPA2 support](docs/wpa2-support.md)

## Build and contribute

Build with the pinned Docker toolchain; generated firmware and packages stay in ignored `builds/`, with intermediate trees/logs in `artifacts/`.

The standalone [MSDIAG utility](firmware/tools/msdiag/README.md) estimates Memory Stick capacity in read-only mode on the supported PSP 6.61 configuration. The owner reported exact matches with direct-reader measurements on the tested 256 GB and 512 GB card and adapter combinations. To build the utility locally, run `python scripts/build-msdiag.py` with the pinned toolchain image.

The resident packager stages six release uploads. The manual ZIP contains the detailed build manifest. A companion source ZIP, compact build JSON, one checksum file and the two Internet-update files complete the inventory.

```text
docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts
python scripts/build.py reference
python scripts/build.py pro-x --resident-dedicated-ota
python scripts/package.py
python scripts/verify-package.py --release-assets builds/resident-release-<build-id>/pro-x-<version>-661.zip
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


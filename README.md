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

The standalone Memory Stick trials were tested on one backed-up 256 GB card and adapter. The PSP formatted three primary FAT32 volumes and automatically mounted the first as `ms0:` in one run. A direct card reader independently checked the layout and the initial contents of all three volumes. Formatting other card and adapter combinations and integration with PSP Settings remain pending. [Partition switch trial](firmware/tools/msswitch/README.md) · [Whole-card trial](firmware/tools/msswitch/WHOLE-CARD-TRIAL.md)

MSAUTO is a read-only diagnostic that computes a capacity candidate and partition preview using the inserted card. It reports ambiguous samples separately. The owner verified ordinary format routing on an 8 GB card and the three-volume preview on the prepared 256 GB card without reference files. [MSAUTO diagnostic](firmware/tools/msdiag/AUTO-PREVIEW.md)

MSAUTOFMT is a standalone 6.61 test app that uses the live card to plan and format FAT32 volumes and restores the first volume as `ms0:`. It requires an exact boundary and distinct live sector samples before it offers formatting. The owner verified automatic formatting and first-volume mounting on a 256 GB card, and a direct USB reader confirmed all three FAT32 volumes. On the first 512 GB card, verification stopped at the second volume. On the second 512 GB card, a direct USB reader verified all six volumes. The owner verified an automatic first-volume mount after formatting from Sony's whole-device fallback and confirmed that the XMB recognized the card without reinsertion. On the tested 256 GB card, the PSP converted GPT to MBR during formatting and mounted the first volume. A direct USB reader confirmed all three FAT32 volumes and the removal of both GPT copies. [MSAUTOFMT trial](firmware/tools/msswitch/AUTO-FORMAT-TRIAL.md)

[MSATTR](firmware/tools/msdiag/ATTRIBUTE-PROBE.md) lists the live Memory Stick PRO attributes provided by the adapter. A physical scan on the 512 GB trial card confirmed that the raw System Info matches the Sony driver's reported capacity. Native SD CSD access remains unverified.

[MSBLANK](firmware/tools/msswitch/BLANK-MEDIA-WRITE-TRIAL.md) tests temporary sector writes and restoration on the backed-up 256 GB card. The owner reported successful restoration on PSP, and a direct USB reader confirmed the MBR and all ten sampled sectors against their original bytes. The trial uses an existing card identity. Qualification of completely blank cards remains pending.

[MSID](firmware/tools/msdiag/IDENTITY-PROBE.md) compares Memory Stick PRO serial and PRO ID fingerprints in read-only mode. Two physical microSD cards in the same adapter returned the same fingerprints for both fields. The native microSD identity remains unverified.

[MSCHANGE](firmware/tools/msdiag/MEDIA-CHANGE-PROBE.md) checks Memory Stick changes in read-only mode. The owner confirmed on a PSP that removing and reinserting a card invalidated the previous review, including with a card that prompted the console to format it.

The current MSAUTOFMT test build applies this check to its format plan. The owner confirmed that removing the card discarded the displayed plan and that the format completed after a fresh scan.

The zero-filled sampling path in MSAUTOFMT uses separately authorized temporary writes and verifies restoration before offering an extended format. On a prepared 256 GB card, the direct USB reader confirmed restoration and verified the three FAT32 volumes and their initial contents after formatting. The owner confirmed that the first volume mounted on the PSP. The planning limit is 1 TB, with physical capacity testing through 512 GB.

MSAUTOFMT assigns FAT32 labels such as `PART 1` to newly formatted volumes. Its read-only preview identifies which volume Sony has mounted using the stored serial and extent and displays its current label. The owner verified that the second 512 GB card mounted as `PART 1` after a repeat format. A direct USB reader confirmed all six labels.

[MSWRPATH](firmware/tools/msswitch/WRITE-PATH-TRIAL.md) is a standalone one-sector write trial for comparing adapters on the backed-up 512 GB card. The first physical run reached the write and readback. The write reported success and the original data remained at the tested address. The failure also occurred through a direct USB reader, which passed a lower-sector write and restore on the same microSD. The second 512 GB card retained and restored data at the same sector through that reader.

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
- **PSPDEV contributors** for the SDK and API documentation, the library NID catalog used to verify Sony's mutex imports, and the PRXTool kernel error reference. [PSPSDK](https://github.com/pspdev/pspsdk) · [PSPLibDoc](https://github.com/pspdev/psplibdoc) · [PRXTool](https://github.com/pspdev/prxtool)
- **PSP Homebrew Community Wiki contributors** for the PSP error-code reference used during the MSDIRS investigation. [PSP Homebrew Community Wiki](https://github.com/psp-homebrew-community/wiki)
- **PPSSPP contributors** for the PSP ELF loader used as a reference for packed relocations. [PPSSPP ELF reader](https://github.com/hrydgard/ppsspp/blob/master/Core/ELF/ElfReader.cpp)
- **uOFW documentation contributors** for archiving the Memory Stick PRO specification. [upspd](https://github.com/uofw/upspd)
- **Linux Memory Stick maintainers** for the Memory Stick PRO attribute directory and System Info identity field reference. [Linux Memory Stick PRO driver](https://github.com/torvalds/linux/blob/master/drivers/memstick/core/mspro_block.c)
- **Unicorn Engine and dosfstools contributors** for host replay and independent FAT32 validation. [Unicorn Engine](https://github.com/unicorn-engine/unicorn) · [dosfstools](https://github.com/dosfstools/dosfstools)

Original copyright notices, license notices, and contributor acknowledgments must be retained.

## License

PRO-X is licensed under the **GNU General Public License, version 3 or, at your option, any later version** (`GPL-3.0-or-later`), consistent with the PRO source notices.

Include the full license text in `LICENSE`. Third-party components retain their applicable licenses and notices. Distributed binaries must be accompanied by access to their corresponding source in accordance with the GPL.

See the [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html) for the full terms. The software is provided without warranty, as described in the license.


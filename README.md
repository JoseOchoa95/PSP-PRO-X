# PRO-X

A community-driven custom firmware project for the PlayStation Portable, based on PRO-C.

**AI-assisted development:** PRO-X is being developed with assistance from artificial intelligence (AI), including Codex, for code changes, build tooling and documentation. The original PRO-C code and third-party components retain their authorship, credits and licenses.

PRO-X aims to continue the PRO lineage through regular updates, bug fixes, and new features, prioritizing stability and compatibility with Infinity and the existing PRO ecosystem.

## Project status

**0.1.0-alpha.1 — implementation prepared for testing; physical hardware validation pending.**

The repository now contains a PRO-C-based experimental build for PSP firmware **6.61**, with PRO-X text in the installer, Fast Recovery, system version, VSH and Recovery menus, including bundled translations. Clean reference and branded builds passed development checks. No PRO-X build has been validated on physical hardware yet. Installation and Infinity startup remain unverified.

## Goals

- Continue development with maintainable, incremental updates.
- Preserve compatibility with the PRO ecosystem wherever possible.
- Support persistent startup through Infinity.
- Introduce new features in separate, reviewable milestones.
- Document build requirements, changes, and hardware test results.

## Roadmap

| Milestone | Scope | Status |
| --- | --- | --- |
| **0.1.0 — Initial foundation** | Select and pin the PRO-C baseline, establish the build process, update visible branding, and validate installation and Infinity startup on real hardware. | Implementation and test package complete; hardware pending |
| **WPA2 integration** | Integrate WPA2 support, enabled by default, using the ARK implementation as a technical reference. | Planned |
| **PRO-X updates** | Adapt the XMB system update entry to retrieve compatible stable releases from the PRO-X repository and verify packages before installation. | Planned |

Roadmap items are development targets and do not describe features currently available in PRO-X.

## Initial milestone

The experimental release uses PRO-C revision `d9435ff4af6ba60466b46019393f6bbd1fc72ac0`. The imported source and original credits are in `firmware/`; provenance and inherited binary limitations are documented in [docs/upstream.md](docs/upstream.md).

Internal module paths, exported interfaces, HEN values and boot contracts remain unchanged. Infinity is expected to use its existing **PRO** selection to start PRO-X; this behavior must be confirmed on hardware. PRO-X shares PRO-C's installation paths and does not provide a parallel installation.

Acceptance requires checking installation, full shutdown and startup through Infinity, suspend and resume, menu operation, and a representative set of games, homebrew, and plugins. Test results will identify the exact console model, firmware, Infinity version, and build tested.

## Building and installation

See [docs/build.md](docs/build.md) for the pinned Docker toolchain, actual commands and checks, and [docs/install-and-validation.md](docs/install-and-validation.md) for the console information required before installation and the physical test matrix.

The experimental package is [artifacts/pro-x-0.1.0-alpha.1-661.zip](artifacts/pro-x-0.1.0-alpha.1-661.zip), with a companion source archive and external SHA-256 list in `artifacts/`. It includes `PSP/GAME/PROUPDATE/EBOOT.PBP` and `PSP/GAME/FastRecovery/EBOOT.PBP` from the same build, translations, notices and a build manifest. It excludes Infinity, Sony firmware and IPL/cIPL tools. [Development results](docs/phase-1-results.md) distinguish completed checks from pending hardware tests.

PRO-X packages should be treated as experimental until the relevant build and console configuration have passed hardware validation. A successful compilation alone does not establish that a build is ready to install.

### Installation requirements

- A PSP running **firmware 6.61**. Check the complete software version in **Settings → System Settings → System Information**. This package is not a firmware update and has not been validated for 6.60 or other firmware versions.
- Access to the PSP's storage through USB or a card reader, and the **binary ZIP** linked above. The `-source.zip` archive is for building, not installation.
- A charged battery, the charger connected, and a backup of saved data, plugins and existing configuration. Keep the previous CFW installer and its recovery instructions if applicable.
- A record of the current model, firmware, CFW and startup method for the test report. The PRO-C base contains support for PSP-1000, 2000, 3000, Go and Street; this PRO-X build has not been validated on any physical model yet.

The procedure below describes temporary activation from official 6.61 without automatic CFW loading. If another CFW, Infinity or cIPL is already present, review the [installation and validation guide](docs/install-and-validation.md) for that existing state before applying these steps. Do not assume that removing the existing CFW or reflashing official firmware is required.

**PRO-X writes to flash and reuses PRO-C's module paths. It does not install alongside PRO-C. This package does not install, remove or configure Infinity or cIPL, and successful development checks do not guarantee a brick-free installation.**

### Installation steps

1. Download and extract [pro-x-0.1.0-alpha.1-661.zip](artifacts/pro-x-0.1.0-alpha.1-661.zip). Check its SHA-256 against [the accompanying hash list](artifacts/pro-x-0.1.0-alpha.1-661.sha256). For example, from the repository root in PowerShell:

   ```powershell
   Get-FileHash .\artifacts\pro-x-0.1.0-alpha.1-661.zip -Algorithm SHA256
   ```

2. Connect the PSP's storage to the computer. Copy the extracted `PROUPDATE` and `FastRecovery` folders into `PSP/GAME` on that storage. Merge the folders with the existing directory; do not delete other applications. The resulting paths must be:

   ```text
   PSP/GAME/PROUPDATE/EBOOT.PBP
   PSP/GAME/FastRecovery/EBOOT.PBP
   ```

3. Disconnect safely and leave USB mode. In the PSP's **Game** menu, open **661 PRO-X Installer** from the storage where you copied it.
4. Press **X** at `Press X to launch CFW`. Allow installation to finish without interrupting power or removing storage. When it reports `Completed. Press X to start CFW.`, press **X** to activate PRO-X. If an error appears, stop and record its exact text/code.
5. Check **System Information** for **6.61 PRO-X**. Record the full displayed version, installer result, console model and package revision in the physical test report.
6. If using translated menus, first back up existing translations/fonts, then copy the bundled `seplugins/recovery_*.txt`, `satelite_*.txt` and `fonts/` resources into the corresponding `seplugins` paths. Existing custom translations can retain the old PRO heading. Keep plugin lists such as `vsh.txt`, `game.txt` and `pops.txt`; the package does not contain replacement lists.

Use only the normal **X** installation path for the initial test. **Triangle** uninstalls CFW modules and configuration; holding **L** forces reinstallation and removes configuration. These are separate operations, not required installation steps.

### After a full shutdown

Without a configured permanent startup mechanism, the expected behavior is that a **full shutdown** followed by power-on starts official firmware. To activate the installed PRO-X again, open **661 PRO-X Fast Recovery**; reinstalling is not normally needed. Suspending and resuming keeps the current session active.

The installed PRO-X files remain in flash after shutdown. If Infinity is already configured to load **PRO**, or another permanent mechanism is active, it may load the CFW automatically instead. This package does not disable that existing behavior. Installation, Fast Recovery and automatic startup still require the physical tests recorded in [the validation matrix](docs/install-and-validation.md).

## Contributing

Contributions should focus on small, clearly scoped changes. Include the purpose of the change, relevant compatibility considerations, and the checks performed.

For hardware reports, include the console model, base firmware, PRO-X revision, Infinity version when applicable, enabled plugins, reproduction steps, and observed behavior. Clearly distinguish completed tests from pending validation.

## Credits and references

PRO-X is an independent fork and is not an official release from Team PRO, the ARK team, or the Infinity developer.

- **Team PRO**, including Coldbird and Virtuous Flame, for the original PRO custom firmware. [PRO source](https://github.com/MrColdbird/procfw)
- **Davee** for Infinity. [Infinity](https://github.com/DaveeFTW/Infinity)
- **ARK contributors** and **Moment** for the WPA2 work informing the planned networking milestone. [ARK-5](https://github.com/PSP-Arkfive/ARK-5) · [wpa2psp](https://github.com/Kethen/wpa2psp)

Original copyright notices, license notices, and contributor acknowledgments must be retained.

## License

PRO-X is licensed under the **GNU General Public License, version 3 or, at your option, any later version** (`GPL-3.0-or-later`), consistent with the PRO source notices.

Include the full license text in `LICENSE`. Third-party components retain their applicable licenses and notices. Distributed binaries must be accompanied by access to their corresponding source in accordance with the GPL.

See the [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html) for the full terms. The software is provided without warranty, as described in the license.


# PRO-X

A community-driven custom firmware project for the PlayStation Portable, based on PRO-C.

PRO-X aims to continue the PRO lineage through regular updates, bug fixes, and new features, prioritizing stability and compatibility with Infinity and the existing PRO ecosystem.

## Project status

**Early development — initial milestone: a PRO-C-based build with PRO-X branding.**

The first milestone is to produce an installable build for PSP firmware **6.61**, update its user-facing branding, and validate automatic startup through Infinity. No PRO-X build has been validated on physical hardware yet. Infinity compatibility is a requirement to verify, not a completed feature.

## Goals

- Continue development with maintainable, incremental updates.
- Preserve compatibility with the PRO ecosystem wherever possible.
- Support persistent startup through Infinity.
- Introduce new features in separate, reviewable milestones.
- Document build requirements, changes, and hardware test results.

## Roadmap

| Milestone | Scope | Status |
| --- | --- | --- |
| **0.1.0 — Initial foundation** | Select and pin the PRO-C baseline, establish the build process, update visible branding, and validate installation and Infinity startup on real hardware. | Planned |
| **WPA2 integration** | Integrate WPA2 support, enabled by default, using the ARK implementation as a technical reference. | Planned |
| **PRO-X updates** | Adapt the XMB system update entry to retrieve compatible stable releases from the PRO-X repository and verify packages before installation. | Planned |

Roadmap items are development targets and do not describe features currently available in PRO-X.

## Initial milestone

The first release will focus on an installable PRO-C-based build with PRO-X branding. It will cover the installer, Fast Recovery, system version display, VSH menu, and Recovery menu where applicable.

Internal module paths, exported interfaces, and boot contracts required by PRO and Infinity will be preserved. Initially, Infinity is expected to use its existing **PRO** selection to start PRO-X; this behavior must be confirmed on hardware.

Acceptance requires checking installation, full shutdown and startup through Infinity, suspend and resume, menu operation, and a representative set of games, homebrew, and plugins. Test results will identify the exact console model, firmware, Infinity version, and build tested.

## Building and installation

Reproducible build instructions and installation steps will be added after the source baseline and toolchain have been verified.

PRO-X packages should be treated as experimental until the relevant build and console configuration have passed hardware validation. A successful compilation alone does not establish that a build is ready to install.

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


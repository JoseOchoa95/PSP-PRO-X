# PRO-X Memory Stick Whole Card Trial

This PSP 6.61 diagnostic recreates the three FAT32 volumes on the prepared, fully backed-up 256 GB microSD from the MSDIRCARD trial. It uses the original Memory Stick adapter and a matching `MSPART.BIN`. The app previews the measured layout, checks the media again under the stock I/O lock, stages the 17 initial folders and two indicator files with Sony's directory routine, and verifies every volume's metadata on readback. It publishes the MBR after all volumes verify. It then verifies the stock first-partition block device and attempts to mount it as `ms0:`.

1. Copy `release/PSP` to the root of the working launch card. Put the original `MSPART.BIN` beside `EBOOT.PBP` in `PSP/GAME/MSMULTI/`.
2. Launch **PRO-X Memory Stick Whole Card Trial** and press **X** to check the private RAM device. When the access light goes out, insert the backed-up 256 GB trial card with its original adapter and press **X** to preview the layout.
3. Check that the preview lists three volumes starting at LBAs 2048, 230688768 and 461375488. Circle cancels at this point. Hold **L + R** and press **X** to confirm. This replaces the contents of all three volumes. Leave the card inserted until the result displays. Verification can take several minutes.
4. Record the result screen before pressing Circle. `Whole card trial: PASS` requires three verified volume images and successful readback of the first volume using `ms0:`. `Mount status` and `Result code` help distinguish a mount issue from a formatting issue.
5. With the card in the direct microSD slot of the Windows reader, run `python verify-whole-card.py DISK_NUMBER path/to/MSPART.BIN` from an elevated terminal. The command opens the disk read-only and checks all three volumes.

For a card already formatted by MSMULTI, follow steps 1 and 2 through the card swap and press **Square** at the card prompt. This verifies the existing MBR and boot records, runs Sony's post-format media refresh, and reports each call used to restore `ms0:`. It reports `Partition 1 mount trial: PASS` if the first volume is accessible. Pressing **X** on the recognized formatted card prompts for **Square**. The full format also reports elapsed time and sector writes for each volume.

To repeat the complete test on the same backed-up 256 GB card, follow steps 1 and 2 through the swap, then press **Start**. The app verifies the already formatted layout and displays a new preview. After the **L + R + X** confirmation it detaches the stock mount, checks Sony's native FAT mount state, and repeats the full format, verification and `ms0:` mount. The trial replaces the contents of all three volumes again. If it stops before writing, the `FAT before`, `FAT after` and `Preflight` lines identify the failed access check.

## Credits

- Thanks to the PSPDEV contributors for the SDK and Sony import catalog. [PSPSDK](https://github.com/pspdev/pspsdk) · [PSPLibDoc](https://github.com/pspdev/psplibdoc)
- Thanks to the PPSSPP, Unicorn Engine and dosfstools contributors for relocation reference, host replay and independent FAT32 verification. [PPSSPP ELF reader](https://github.com/hrydgard/ppsspp/blob/master/Core/ELF/ElfReader.cpp) · [Unicorn Engine](https://github.com/unicorn-engine/unicorn) · [dosfstools](https://github.com/dosfstools/dosfstools)
- Thanks to the authors and archivists of 64kformat and CXMB for examples of restoring the conventional Memory Stick assignment. The refresh control was traced independently in Sony 6.61. [64kformat](https://github.com/PSP-Archive/64kformat) · [CXMB](https://github.com/PSP-Archive/CXMB)

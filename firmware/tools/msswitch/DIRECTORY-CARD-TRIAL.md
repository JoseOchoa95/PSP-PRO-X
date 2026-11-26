# PRO-X FAT32 Physical Directory Trial

This trial uses the original Sony FAT mkdir callback to prepare the fourteen Sony directories plus `ISO`, `ISO/VIDEO` and `SEPLUGINS` in a private FAT32 volume in RAM. The last three match the PRO-X menu hook for creating `PSP/GAME`. The staged FAT32 root also contains the two empty, hidden, read-only indicator files observed on a card formatted from the PSP menu, `MEMSTICK.IND` and `MSTK_PRO.IND`. The trial applies the verified image to partition 3 of the same backed-up 256 GB card and replaces that partition's existing filesystem contents, including any entries added by Windows.

1. Copy `release/PSP` to the root of your working launch card. Place the original `MSPART.BIN` beside `EBOOT.PBP` in `PSP/GAME/MSDIRCARD/`.
2. Launch **PRO-X FAT32 Physical Directory Trial** on the tested PSP-2000 running PRO-X and system software 6.61. Press **X** to prepare the RAM volume.
3. Wait for the Memory Stick light to go out. Insert the same backed-up 256 GB trial card with its original Memory Stick adapter. Press **X** to preview its partition and metadata ranges.
4. Confirm the selected partition is partition 3 (LBA 461375488 through 500695039). Hold **L + R** and press **X**. Keep the trial card inserted until the test and readback finish. Circle exits.
5. Connect the card to the direct microSD slot on the USB reader. From an elevated Windows terminal run `python verify-directory-trial.py DISK_NUMBER path/to/MSPART.BIN`. This read-only check verifies the MBR, controls, FAT32 geometry, 17 directories and both indicator files.

## Credits

- Thanks to the **PSPDEV contributors** for the SDK and Sony library catalog, and the **PSP Homebrew Community Wiki contributors** for the error reference. [PSPSDK](https://github.com/pspdev/pspsdk) · [PSPLibDoc](https://github.com/pspdev/psplibdoc) · [PSP Homebrew Community Wiki](https://github.com/psp-homebrew-community/wiki)
- Thanks to the **PPSSPP contributors** for the packed relocation reference, the **Unicorn Engine contributors** for host emulation, and the **dosfstools contributors** for independent filesystem validation. [PPSSPP ELF reader](https://github.com/hrydgard/ppsspp/blob/master/Core/ELF/ElfReader.cpp) · [Unicorn Engine](https://github.com/unicorn-engine/unicorn) · [dosfstools](https://github.com/dosfstools/dosfstools)

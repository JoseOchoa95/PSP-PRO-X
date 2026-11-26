# PRO-X FAT32 RAM Directory Trial

This diagnostic runs Sony 6.61's FAT directory creation against a private FAT32 device held entirely in RAM. It uses the fourteen paths traced from the Sony Settings post-format routine, the three PRO-X menu folders and the two empty indicator files observed after formatting. It uses the same geometry as the prepared third partition from the 256 GB card trial.

1. Copy the `PSP` folder inside `release/` to the root of a working Memory Stick.
2. Launch **PRO-X FAT32 RAM Directory Trial** on the tested PSP-2000 configuration running PRO-X on system software 6.61. Leave the launch card inserted. The test runs without a card reference or swap.
3. Press **X** and record the result screen. The checks cover the original Sony FAT profile, 17 directories, two indicator files, the FAT32 free-cluster summary, writes at the partition boundary, a deliberately denied write and cleanup. Press **Circle** to exit.

The separate MSDIRCARD diagnostic exercises the same initial contents on the physical third partition. Integration into the Settings flow remains pending.

## Credits

- Thanks to the **PSPDEV contributors** for the build SDK, the catalog of Sony library NIDs and the PRXTool kernel error reference. [PSPSDK](https://github.com/pspdev/pspsdk) · [PSPLibDoc](https://github.com/pspdev/psplibdoc) · [PRXTool](https://github.com/pspdev/prxtool)
- Thanks to the **PSP Homebrew Community Wiki contributors** for the PSP error-code reference. [PSP Homebrew Community Wiki](https://github.com/psp-homebrew-community/wiki)
- Thanks to the **PPSSPP contributors** for the ELF loader reference, the **Unicorn Engine contributors** for host emulation, and the **dosfstools contributors** for the independent FAT32 check. [PPSSPP ELF reader](https://github.com/hrydgard/ppsspp/blob/master/Core/ELF/ElfReader.cpp) · [Unicorn Engine](https://github.com/unicorn-engine/unicorn) · [dosfstools](https://github.com/dosfstools/dosfstools)

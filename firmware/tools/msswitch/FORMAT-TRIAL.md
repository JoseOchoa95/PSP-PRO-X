# PRO-X FAT32 Partition Format Trial

This diagnostic replaces the FAT32 metadata of partition 3 on the prepared, backed-up 256 GB microSD. The resulting volume is empty. The separate MSDIRCARD diagnostic exercises the initial folders and files. The app checks the saved MBR, card geometry, and on-card file and sector witnesses before writing. It verifies the FAT32 boot record describes the exact partition and compares the live preview's boot record again after confirmation and under the write lock.

1. Copy the `PSP` folder from `release/` to the root of the boot Memory Stick. Place the original `MSPART.BIN` in `PSP/GAME/MSFORMAT/` alongside `EBOOT.PBP`.
2. Launch the trial on the same PSP running PRO-X on system software 6.61. Insert the prepared microSD with its original Memory Stick adapter while the activity light is off. Press X to check the card.
3. Press Select to preview. Confirm partition 3 and the displayed range, then hold L and R and press X once. The test file becomes inaccessible when its FAT32 metadata is replaced. Keep the microSD inserted until the result displays.
4. Capture the result and press Circle to exit. Insert the microSD in the USB reader's direct microSD slot. Dismiss any prompt to initialize, repair, or format.
5. From an elevated Windows terminal run `python verify-format-trial.py DISK_NUMBER path/to/MSPART.BIN` using the disk number of the USB reader. The verification opens the disk for reading and checks the MBR, five unchanged sectors and the full FAT32 metadata area of partition 3.

The other partition entries stay in place. The trial uses a private Sony block context and the native format dispatcher. The bounded FAT32 initializer supplies the writes for that context. The trial package leaves the installed firmware and the XMB formatting flow unchanged.

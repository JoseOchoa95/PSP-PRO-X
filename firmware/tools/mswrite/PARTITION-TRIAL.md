# PRO-X Memory Stick Partition Trial

This standalone 6.61 diagnostic is for the backed-up 256 GB microSD and the original adapter after the high-sector write trial. Confirming the test replaces sector zero, including the partition entries for the current exFAT volume. That volume becomes inaccessible through its old partition entry. Use a card whose contents you can afford to lose.

The test creates three primary partition entries for future FAT32 volumes. Their lengths are 230,686,720, 230,686,720 and 39,319,552 sectors. The preview shows each exact start and length. The trial writes only the MBR sector and does not initialize any filesystem.

1. Keep the original MBR backup on the PC. Use the prepared `MSPART.BIN` or prepare it from the direct microSD reader with administrator PowerShell by running `py -3 .\mswrite-reference.py prepare-partitions N "<path-to-MSWTEST.BIN>"`, where `N` is the disk number from `Get-Disk`. This reads the card, verifies the earlier high-sector trial and saves `MSPART.BIN` and `ORIGINAL-MBR.BIN` in a temporary directory.
2. Copy `release/PSP` to the root of a card the PSP can launch apps from. Place `MSPART.BIN` beside `EBOOT.PBP` in `PSP/GAME/MSPART/`. Start **PRO-X Memory Stick Partition Trial**. When the Memory Stick activity light goes out, insert the backed-up trial card in its original adapter.
3. Press **X** for the preview. Verify that it shows three primary partitions at starts 2048, 230688768 and 461375488. **L + R + X** confirms replacing the partition table. **O** cancels. Photograph the result, including the write, readback and close results.
4. Reconnect the trial card directly to the PC. Dismiss any prompt to initialize or format it. Independent verification reads the table and witness sectors with `py -3 .\mswrite-reference.py verify-partitions N "<path-to-MSPART.BIN>"` using the current disk number. Keep both backup files until validation is complete.

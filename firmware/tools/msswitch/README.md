# PRO-X Partition Switch Trial

This standalone PSP 6.61 diagnostic exercises read-only block I/O for three primary partitions, private Sony FAT block switching and RAM-only format dispatch. It uses the same Memory Stick adapter as the partition creation trial and compares a different sector in each partition against the existing direct-reader reference. The diagnostic registers a temporary block driver for the session. XMB Memory Stick assignment is a separate integration step.

1. Copy the `PSP` folder inside `release/` to the root of a working Memory Stick. Copy the existing `MSPART.BIN` PC reference for the trial card into `PSP/GAME/MSSWITCH/` beside `EBOOT.PBP`.
2. Launch **PRO-X Partition Switch Trial**. When it shows **PC reference loaded**, wait for the Memory Stick light to turn off and insert the backed-up trial microSD with its original adapter. Keep it inserted through the test.
3. Press **X**. Record the first screen. Press **Right** twice to test partitions two and three, then press **Right** once more to return to partition one. Record each screen. A successful screen shows **PC witness: MATCH** and **Partition test: PASS**.
4. Press **Circle** and check that the app returns to the PSP menu.

The second and third witnesses are above the capacity reported by the adapter. Each trial also checks the partition length, end-of-device behavior, rejection of out-of-range seeks and rejection of a write-mode open on the temporary driver.

## FAT32 file read trial

On partition 3, press **Triangle**. The diagnostic first checks the partition's actual boot sector and looks for a 512-byte `PXPROBE.BIN` if the partition contains FAT32. The original trial card can show **no FAT32 boot sector** because its existing test layout contains only MBR entries.

On the prepared FAT32 test partition, **On-card file witness: MATCH** confirms that the file read from its root contains the exact sector sample in `MSPART.BIN`.

The second check uses an in-memory FAT32 boot sector, allocation table and root directory. The file data comes from LBA 500694016 on the real card through the selected partition. The expected results are **PXPROBE.BIN read**, **Card data reads: 1** and **PC file witness: MATCH**. Press **Circle** to return to the PSP menu. This test reads through the temporary block driver and an independent FAT32 reader. Sony filesystem mounting and formatting remain separate validation steps.

## Sony block switch trial

After pressing **X** to check the prepared 256 GB card, press **Square** for the automatic 1 → 3 → 1 test. It uses Sony 6.61's lower FAT mount, sector-read and unmount routines on a private context with a read-only descriptor. The same temporary device name is opened for each partition. The trial compares each witness and requires the read past each partition to produce zero sector transfers. On partition 3 it also reads the prepared on-card FAT32 file through this sector route and compares its PC witness.

Record the screen showing **Sony block switch: PASS** or its error. Press **Circle** to return to the PSP menu. The test runs within the application. System-wide `ms0:` selection, native FAT file operations and formatting are separate validation steps.

## Sony format dispatch trial

Press **Start** after the card check. The app selects partition 3 and verifies its known file. It then passes Sony's format request to the 6.61 dispatch routine in a separate RAM context. The dispatch calls a partition-bounded FAT32 initializer with a RAM-only writer and verifies the resulting boot sectors and allocation tables. The hardware partition is opened for reads and is checked again after the RAM test.

Record the screen showing **RAM format trial: PASS** or its error. The FAT32 in RAM is empty. Separate physical diagnostics exercise the bounded FAT32 format and initialization of the files and folders on partition 3. PSP Settings integration remains pending.

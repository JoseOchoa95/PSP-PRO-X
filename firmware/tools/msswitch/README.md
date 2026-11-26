# PRO-X Partition Switch Trial

This standalone PSP 6.61 diagnostic exercises read-only block I/O for three primary partitions on the verified 256 GB trial card. It uses the same Memory Stick adapter as the partition creation trial and compares a different sector in each partition against the existing direct-reader reference. The diagnostic registers a temporary block driver for the session. FAT mounting, Sony formatting and the XMB Memory Stick assignment are outside this test.

1. Copy the `PSP` folder inside `release/` to the root of a working Memory Stick. Copy the existing `MSPART.BIN` PC reference for the trial card into `PSP/GAME/MSSWITCH/` beside `EBOOT.PBP`.
2. Launch **PRO-X Partition Switch Trial**. When it shows **PC reference loaded**, wait for the Memory Stick light to turn off and insert the backed-up trial microSD with its original adapter. Keep it inserted through the test.
3. Press **X**. Record the first screen. Press **Right** twice to test partitions two and three, then press **Right** once more to return to partition one. Record each screen. A successful screen shows **PC witness: MATCH** and **Partition test: PASS**.
4. Press **Circle** and check that the app returns to the PSP menu.

The second and third witnesses are above the capacity reported by the adapter. Each trial also checks the partition length, end-of-device behavior, rejection of out-of-range seeks and rejection of a write-mode open on the temporary driver.

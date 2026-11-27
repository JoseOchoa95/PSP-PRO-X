# PRO-X Memory Stick Write Path Trial

This PSP 6.61 diagnostic tests the write path on the backed-up 512 GB trial card. It scans the card locally and targets LBA 243271680, the second volume's start in the failed trial. Use the same card and LBA with each adapter to isolate the change.

Copy the PSP directory from release onto a working Memory Stick. Start the trial, wait until the Memory Stick light goes out, and insert the backed-up 512 GB card in the original adapter. Press X to scan. Check the displayed target and hold L and R while pressing X to run the one-sector write and readback. Record the results, exit with O, then repeat with the same card in the second adapter. Start the app from the working card again before swapping if necessary.

The trial requires a backup because an adapter can redirect a write. The app issues writes only to the displayed LBA, checks the original MBR and sampled control sectors, and attempts to restore the original target bytes. It verifies the restored bytes after reopening the device. It performs no filesystem format or partition-table publication. The tested geometry is limited to the measured 500 GiB card. GPT conversion remains a separate task.

Thanks to the PSPDEV maintainers for the toolchain and SDK, and the uOFW contributors for the PSP storage documentation. [PSPDEV](https://github.com/pspdev) · [uOFW](https://github.com/uofw/upspd)

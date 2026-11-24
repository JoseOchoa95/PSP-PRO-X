# PRO-X flash0 capacity diagnostic

This standalone tool measures the free bytes, usable total bytes and allocation unit of flash0 on PSP 6.61 with PRO-X active. It opens the flash0 logical volume for reading and uses the same kernel-mode geometry query as the USB driver to obtain the sector count before reading the FAT boot sector and allocation tables. The only file the tool writes is a report on the Memory Stick. A report contains only results and counters.

1. Copy `PSP/GAME/FCAPDIAG` to the Memory Stick.
2. With PRO-X active, open **PRO-X Flash0 Capacity** in Games. It displays the result and tries to save a new `PSP/GAME/FCAPDIAG/report-*.txt` on the card.
3. Send the new report or a photo of the screen. After the report, press X to return to XMB or wait 30 seconds. The PRO-X Installer and Fast Recovery are not needed for this measurement.

A zero result is a measurement. A nonzero result means that the volume could not be read or its geometry/allocation tables could not be verified. This protects the later installer preflight from relying on an unverified free-space estimate.

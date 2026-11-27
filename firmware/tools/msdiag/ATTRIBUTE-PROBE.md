# PRO-X Memory Stick Attribute Probe

This standalone read-only PSP 6.61 diagnostic uses Sony's Memory Stick PRO attribute-read command to list records supplied by the adapter. It compares the live System Info record with the one cached by the Sony driver. The CSD register belongs to the SD protocol. Any unrecognized attribute is exploratory evidence and requires further identification before it can serve as a capacity source.

Copy the PSP directory from release to the working Memory Stick. Launch the app and wait until the Memory Stick light goes out before inserting the target with its adapter. Press X, record the screen, and press O to exit. This test can be run using the same adapter with each test microSD.

Thanks to PSPDEV for the toolchain and SDK, uOFW for the archived PSP storage documentation, and the Linux Memory Stick maintainers for the attribute directory reference. The SD Association specification defines the SD CSD command and capacity field. [PSPDEV](https://github.com/pspdev) · [uOFW](https://github.com/uofw/upspd) · [Linux Memory Stick PRO driver](https://github.com/torvalds/linux/blob/master/drivers/memstick/core/mspro_block.c) · [SD Association specification](https://www.sdcard.org/downloads/pls/)

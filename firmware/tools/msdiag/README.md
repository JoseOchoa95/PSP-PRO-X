# PRO-X Memory Stick Diagnostic

MSDIAG runs on a PSP with active custom firmware targeting system software 6.61. It displays the size of the mounted volume, the extent reported by the raw Memory Stick driver, and results from individual 512-byte reads. It opens the Memory Stick and the optional PC reference for reading and keeps results in RAM. There is no firmware installation or format step.

To build the standalone utility, create the pinned Docker image with `docker build -t pro-x-toolchain:phase1 -f scripts/Dockerfile scripts`, then run `python scripts/build-msdiag.py` from the repository root. The script puts the binary ZIP, matching source ZIP and checksums in `builds/<build-id>/`. The binary ZIP contains `release/PSP/GAME/MSDIAG/EBOOT.PBP`.

## Quick measurement

1. Copy the `PSP` folder inside `release/` to the root of a Memory Stick that the PSP can already read.
2. In Game → Memory Stick, start **PRO-X Memory Stick Diagnostic**. If the target is already inserted, leave it in place. If the PSP cannot launch an app from the target, start from a working card. At the app prompt, wait for the Memory Stick activity light to go out and insert the target card and adapter.
3. Press **X** for the automatic estimate or **Square** for the PC comparison. Leave the target inserted while the scan runs. Use **Right** to see each sector result, **Triangle** to jump to the last page and **O** to exit.

The mounted volume can cover only part of the device. Each driver size is a reported extent. The Memory Stick attributes show the geometry presented to the original driver, and the MBR entry shows the declared end of the last primary partition for supported layouts. These values can differ from the physical microSD size. If the program has no PC reference, a successful read shows that the driver returned 512 bytes for the requested address. The content at that address is still unverified.

## Automatic capacity estimate

Press **X** and then **Triangle** after scanning to see page 10. This test uses the active card and the recognized Sony 6.61 driver. It works without `MSDREF.BIN` or a partition table. With the same 4096-sector allocation block geometry observed on the test cards, the diagnostic probes boundaries at the reported size plus multiples of 128 GiB. It tentatively selects a candidate when the reads immediately inside it succeed and two outside addresses return the transport failure previously observed on hardware. The report includes the requested addresses and both return codes.

A transport failure can have other causes, and a success can be an address alias. A candidate remains an estimate. Results for unsupported geometry, inconsistent responses or a read at the 1 TB check can leave the true size unconfirmed. This test issues no write or format operation. For hardware validation, capture pages **1** and **10** and identify the card and adapter used.

The owner confirmed that `msdiag-f310299ee3fd` matched the direct-reader capacities for the tested 256 GB card and adapter (256,355,860,480 bytes in 15.2 seconds) and the tested 512 GB card and adapter (536,870,912,000 bytes in 16.7 seconds). Broader adapter compatibility, blank media and 1 TB cards require physical validation.

## Reference from the direct microSD reader

1. Put the target microSD into a PC reader directly, bypassing the PSP and its Memory Stick adapter. If Windows offers to format or initialize it, dismiss the prompt.
2. From an Administrator PowerShell, identify the target by unplugging and reconnecting the reader and comparing the output of `Get-Disk | Format-Table Number,FriendlyName,BusType,Size,LogicalSectorSize`. Record the reader, card, adapter and PSP models. The capture tool supports media with 512-byte logical sectors and requires Python 3.10 or later.
3. From the extracted diagnostic ZIP on the PC, run `py -3 .\capture-reference.py N` with the direct reader's disk number in place of `N`. The script reads the physical disk and writes `MSDREF.BIN` in a temporary PC directory, printing its location. It reads existing sectors and stores only their addresses and SHA-1 fingerprints, along with the device geometry.
4. Copy `MSDREF.BIN` beside `EBOOT.PBP` on the working launch card. Start the app and confirm that the reference loaded. If the target is a separate card, swap at the prompt as above. Press **Square** to compare with the reference. Photograph the results and share the summary, the two control pages and the sector pages.

The comparison checks two low-address controls and samples below and above 128 GB (128,000,000,000 bytes) and 128 GiB (137,438,953,472 bytes), near the card's end, and at its last sector. It also checks corresponding lower addresses for common wrap periods of 4 GiB, 128 GB and 128 GiB. A distinct PC match supports access to that sampled address. Uniform data, missing lower reads, mismatched controls or identical higher and lower data can leave a result inconclusive. None of these samples establishes contiguous read access, successful writes or formatting support.

## Stock 6.61 transfer test

Page 10 is an experimental read-only test for PSP model ID 1 with the recognized Sony 6.61 Memory Stick driver. With the matching direct-reader reference loaded, the diagnostic checks the driver code and reported geometry, calls Sony's serialized read worker for the low control, and attempts one high sample near 128 GiB only after that control agrees with the reference. It passes a local byte position to the worker and uses its existing I/O mutex. The test makes no firmware changes and issues no storage write commands.

Page 11 shows the boundary test. After a matching nonuniform high sample, the app compares the final sector in the direct-reader reference, attempts the first sector beyond it and repeats the high sample to check recovery. It shows the storage worker's result, the first and last transport results and the number of dispatch calls for each address. These codes describe the test conditions. A generic I/O error cannot establish the true end on a card of unknown size.

For the 256 GB validation card, load its existing `MSDREF.BIN` beside the EBOOT on the launch card. Press **Square** and photograph pages **1**, **10** and **11**, including any skipped-test status or error code. If the app stops advancing, note the last line on screen. The owner confirmed the distinct high-sector read, the observed boundary response and recovery on the original card and adapter.

# PRO-X Memory Stick Auto Format trial

This standalone 6.61 test app measures the inserted microSD through its Memory Stick adapter, presents the FAT32 plan, and requires a separate confirmation to erase and format it. Use the backed-up 256 GB card for the first console trial. The app derives capacity and two live sector witnesses from the card. The evidence must establish an exact boundary and distinguish the sampled address periods. The 1 TB cap applies to planning. The app directs cards in the ordinary capacity range to Sony's Format Memory Stick setting. GPT conversion is pending.

The formatter reads contiguous FAT32 metadata in batches of up to eight sectors and falls back to individual reads if Sony's driver rejects a batch. It verifies every metadata sector after writing. Keep the `Metadata read batches` line and the per-volume times when reporting a test.

Copy the PSP directory from release to the root of a working Memory Stick. On the PSP, launch the app and press X for a formatter check in RAM. When the app asks and the Memory Stick light is off, insert the backed-up trial card with its adapter. Press X to scan. Review the card size and all partition sizes on the preview. Hold L and R and press X to confirm. Leave the card inserted while each partition formats and verifies. When the result appears, take a photo and press O to return to the XMB. Check that Memory Stick opens and that the PSP/GAME folder is accessible.

The app writes the partition layout and FAT32 metadata. Each formatted volume receives the initial PSP folders and indicator files. The app restores partition 1 as ms0: and checks the mounted filesystem. The app package does not install firmware.

After confirmation, the formatter reads, writes and restores one high metadata sector when the planned layout writes above the adapter's reported range. It verifies the original target bytes and sampled controls before proceeding. If the write check fails, it stops before bulk formatting and reports that it could not verify writes to some sectors.

The 256 GB console test passed and an independent direct USB read confirmed all three volumes. On the second 512 GB card, the PSP and direct USB reader verified three primary and three logical FAT32 volumes. An earlier build reported result code 8 when Sony's driver rejected the final `ms0:` mount. The owner reported that reinserting the formatted card let the XMB recognize the first volume. This build asks Sony's FAT detector to close the whole-card handle and reopen by partition if the fallback is active. It rechecks the first volume and verifies `ms0:`. If the fallback ran, `Sony FAT reopen: 0` means the detector reopened the partition. Keep the card inserted until the final result. GPT remains a preview-only case.

For independent verification after formatting, run the repository's `verify-auto-format.py` script from an administrator terminal and pass the Windows disk number, full sector count from the preview, and adapter sector count. The script opens the direct USB reader read-only. It verifies the MBR or EBR chain, FAT32 geometry and the initial directory and indicator entries on every volume.

A repeat format on the already formatted second 512 GB card passed with six verified volumes and an automatic first-volume mount. To exercise the recovery path, start with the backed-up card using a single exFAT partition on MBR and check that `FAT before` reports state `0x00000013`. After the final `PASS`, press O and open Memory Stick in the XMB with the card still inserted.

The original fallback was reproduced on the second 512 GB card. All six volumes verified, and `Sony FAT reopen: 1` indicated that a precondition blocked the reopen. This diagnostic shows the FAT state, media query and detector status when you press X to scan. Capture those lines and press O to exit for a read-only check. The same lines appear if a later format reaches the recovery path and the reopen fails.

Newly formatted volumes receive standard FAT32 labels such as `PART 1` and `PART 2`. MSAUTOFMT reads the label, start and serial from the backing volume Sony currently has mounted. Existing unlabeled volumes display `PARTITION`. On the 512 GB test, `Mounted P1: PART 1   LBA 2048` after formatting verifies the first labeled volume. The preview can identify the corresponding unlabeled volume before formatting. Windows can edit a FAT32 label. The numeric identity uses the stored serial and extent, independent of the displayed label.

Thanks to the maintainers of PSPDEV for the toolchain and SDK, PPSSPP and uOFW for PSP platform references, Unicorn for replaying the hardware-specific call paths, and dosfstools for independent FAT32 validation.

- [PSPDEV](https://github.com/pspdev)
- [PPSSPP](https://github.com/hrydgard/ppsspp)
- [uOFW](https://github.com/uofw/uofw)
- [Unicorn](https://github.com/unicorn-engine/unicorn)
- [dosfstools](https://github.com/dosfstools/dosfstools)

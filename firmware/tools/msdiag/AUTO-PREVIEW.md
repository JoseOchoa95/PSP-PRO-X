# PRO-X Memory Stick Auto Preview

MSAUTO measures the inserted microSD on the profiled PSP-2000 and Sony 6.61 Memory Stick PRO path. It uses the PSP's reads and creates a layout preview without a reference file. The executable is read-only.

1. Copy `release/PSP` to the root of the working launch card and open **PRO-X Memory Stick Auto Preview**.
2. Wait for the Memory Stick light to go out. Insert the microSD to test with its adapter and press **X**. Keep it inserted during the scan.
3. Record the adapter sector count, the boundary candidate or cap sample, the distinct sample count and the proposed layout. When the diagnostic can identify the live card from two distinct sectors outside the proposed write ranges, it also reports their LBAs. Use **Left** and **Right** for additional volumes. Press **Circle** to exit.

Small verified candidates use the ordinary Sony format path. The preview for larger candidates rounds the adapter extent down to whole GiB, aligns the volumes and shows the final remainder. Layouts with more than four volumes include logical partitions. The planning cap is 1 TB decimal. Equal sector samples can leave address aliasing inconclusive, including on blank cards. GPT is detected for display. The [MSAUTOFMT trial](../msswitch/AUTO-FORMAT-TRIAL.md) handles GPT-to-MBR conversion during confirmed formatting.

## Credits

- Thanks to the PSPDEV contributors for the SDK and Sony import catalog. [PSPSDK](https://github.com/pspdev/pspsdk) · [PSPLibDoc](https://github.com/pspdev/psplibdoc)
- Thanks to the uOFW documentation contributors for the Memory Stick PRO protocol reference. [upspd](https://github.com/uofw/upspd)

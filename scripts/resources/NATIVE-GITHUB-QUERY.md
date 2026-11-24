# Phase 3-B resident updater baseline

This local PRO-X v@PRODUCT_VERSION@ build profiles the existing card-based worker in the original PSP System Update UI. It makes an authenticated query to GitHub and records worker startup, kernel/user memory and TLS allocation measurements. System Update blocks download and installation in this build. The resident updater is pending.

1. Keep a known working manual package and preserve the card's settings, plugins and reports. Copy the matching `PROUPDATE`, `FastRecovery` and `PROXUPDATE` folders into `PSP/GAME`, including `PROXUPDATE/cacert.pem`.
2. With the charger and card connected, open PRO-X Installer from Games. Press X to install, wait for completion, then press X to start PRO-X. Check **Settings → System Settings → System Information** for `6.61 PRO-X @PRODUCT_VERSION@`.
3. Set the PSP clock correctly. Open **Settings → System Update → Update via Internet**, select your usual saved Wi-Fi connection and let the check finish. Repeat twice without restarting. If cancelling one extra check, run a new check afterwards to verify recovery. Leave the card inserted for these measurements. Use Settings for this test.
4. Save the new `PSP/GAME/PROXUPDATE/native-report-NNN.txt` files and `dispatch.txt` and note the result displayed on the PSP. Match the reports to the build ID in the manifest. In `dispatch.txt`, `profile` values are hexadecimal. In `native-report-NNN.txt`, timing and memory values are decimal. Free-memory samples are snapshots. TLS peak measures only allocations tracked by Mbed TLS.

## Measure internal free space

With PRO-X active, use the Recovery Menu to verify **Protect Flash in USB Device Mount** is enabled before selecting **Flash 0** as the USB Device. Use **Settings → USB Connection** to expose the device to the PC. Read the volume's total space and free space from Properties, recording the values in bytes. Close USB Connection and restore the previous USB Device selection. Skip the measurement if flash protection cannot be confirmed. The installed USB driver uses this protection setting to expose the selected flash device read-only.

The diagnostic requires the matching files on the card. Keep the matching Fast Recovery available on a card for activation after full shutdown. The package has no stable OTA assets. Physical results are pending.

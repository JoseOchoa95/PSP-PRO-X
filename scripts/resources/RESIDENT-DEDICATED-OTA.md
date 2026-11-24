# PRO-X v@PRODUCT_VERSION@ resident dedicated update candidate

This PSP-2000 6.61 kit contains matching PROUPDATE and Fast Recovery applications. For first installation, copy the ZIP's `@CARD_ROOT@` folder to the root of the Memory Stick. Launch PROUPDATE from Games, press X to install, then press X to activate. Confirm `6.61 PRO-X @PRODUCT_VERSION@` in System Information. You can then remove `PSP/GAME/PROUPDATE` and keep Fast Recovery for reactivation. The resident worker contains the update trust bundle and localized PRO-X completion messages.

System Update offers a newer compatible stable dedicated release. Installation requires the Memory Stick, sufficient flash0 space and the update power conditions. It uses the PSP's completion screen with the PRO-X message and a fresh press of X to restart.

Keep the same Memory Stick and installation power available when the original completion screen restarts into the installed target. When the XMB starts, the resident worker verifies the committed record and installed files without opening System Update or connecting to the Internet. It retires the update's verified flash0 sidecars before removing its temporary EBOOT and stage from the card. From v0.4.2 onward, verified cleanup also removes the updater reports and removes `PROXDIAG` when only updater files were present. A later startup or authenticated System Update check can retry deferred cleanup.

Use the manual kit with official 6.61, PRO-X versions earlier than v0.4.0, or a v0.4.0 installation without a previously stored completion resource.

Physical first-install and Internet-update validation for this build are pending. The dedicated EBOOT is a data package for System Update.

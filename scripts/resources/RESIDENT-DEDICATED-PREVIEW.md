# PRO-X v@PRODUCT_VERSION@ resident dedicated transfer preview

This kit is a PSP-2000 test for 6.61. It exercises the dedicated release selector, package transfer and verification from the Settings menu. When the validated public catalog has no newer dedicated update, the test offers a pinned synthetic EBOOT from the card. The synthetic version exists only in that data file. The firmware version stays v@PRODUCT_VERSION@.

1. Save the working resident installation kit. Copy PSP/GAME/PROUPDATE, PSP/GAME/FastRecovery and the PROXDIAG folder from this ZIP to the Memory Stick. Keep the data filename and directory as provided.
2. Run the matching PROUPDATE from Games, press X to install, wait for completion and press X to activate. The manual Installer writes the diagnostic worker and matching PRO-X modules in the ordinary installation path.
3. With the card inserted, charger connected and sufficient battery, select Settings → System Update → Update via Internet and accept the diagnostic offer. The PSP stages the sample and all twelve parts in an owned PROXDIAG subdirectory. The original updater hands off the verified stage to the shared writer check, which reopens the EBOOT and every component and verifies them again. The original error screen ends this test before any firmware update or restart.
4. Preserve PROXDIAG/dispatch.txt and the new PROXDIAG/native-download-NNN.txt. Look for Dedicated EBOOT verification and job preparation: PASS, Dedicated Settings writer handoff: PASS, and Dedicated diagnostic stage removed. The original PROX-DEDICATED-TEST.PBP remains available for a repeat test.

The Settings test writes the removable stage and reports. The local sample does not exercise an HTTPS package response. A real public dedicated package is eligible only with a valid manifest and the usual stable version, HTTPS and hash checks. This preview stops after the read-only installer handoff for both sources.

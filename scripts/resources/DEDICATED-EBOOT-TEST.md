# Dedicated EBOOT read-only test

Run the included PRO-X Update File Check from Games on a PSP-2000 with active CFW and system software 6.61. Copy only the included PROXEBOOT folder into PSP/GAME. Its EBOOT.PBP is the diagnostic application. UPDATE.PBP is a copy of the matching dedicated EBOOT data file. The diagnostic trusts its build-time pin and verifies the format, complete-file SHA-256, target identity, exact part inventory and every part hash. Expected result is PASS with 12 verified parts. Press X to return to XMB.

The diagnostic uses a fixed test baseline for its version comparison. Running either the resident kit or the installed PRO-X release is sufficient for this read-only check. The application reads the supplied file from the Memory Stick and does not update or write firmware. No external completion resource is needed for the check. Clearing the PROXEBOOT directory after the test removes both files from the card.

The dedicated PBP has an empty executable section and carries an indexed uncompressed payload in DATA.PSAR. Its local JSON identifies the exact compiled build and package digest. Internet offers, install/migration, and post-restart cleanup remain separate from this diagnostic.

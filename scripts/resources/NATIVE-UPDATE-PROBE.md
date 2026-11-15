# Original PSP update UI â€” isolated firmware diagnostic

This is a **firmware diagnostic**, not the completed v0.3.2 Internet updater. Manual installation replaces PRO-X modules using the existing installer. Once installed, Internet Update uses the original PSP UI and a synthetic no-update provider. That query cannot download or install firmware and does not contact GitHub or Sony. A displayed latest-version message is simulated, not evidence that the installed product is current. Do not publish this build or OTA assets.

The original update_plugin entry and installed RCO/language/theme resources are preserved. All ten inspected sceLibUpdateDL imports are intercepted together; an unknown/incomplete layout is refused. No replacement screens are drawn. This probe tests the native frontend lifecycle before connecting the verified GitHub backend. Installation UI reuse and the final consent flow remain pending.

1. Keep the memory card inserted, connect external power and charge the battery to at least 50%. Keep existing configuration/plugins and update-client files.

2. Copy the matching PSP/GAME/PROUPDATE/EBOOT.PBP and PSP/GAME/FastRecovery/EBOOT.PBP from this package onto the card. Do not combine executables from different builds. This package contains no new PROXUPDATE.

3. Run PRO-X Installer from Games. Press X to install, then X to start CFW using the existing manual flow. Do not choose uninstall or configuration reset.

4. Check System Information: expected `6.61 PRO-X 0.3.2`; SELECT should open the PRO-X VSH menu. Then use Settings -> System Update -> Update via Internet.

5. The PSP's own selection/checking/result UI should appear, ending in the original latest-version message. This response is deliberately simulated. An access point may be selected/connected through the original XMB network UI; the provider makes no HTTP/catalog/asset requests and never starts an installer.

6. Exit/cancel, repeat the query twice, change the system language and theme and repeat. Report which original screens appeared, whether navigation returned correctly and whether language/theme changes were reflected. Provide PSP/GAME/PROXUPDATE/dispatch.txt if that existing directory is available.

This isolated checkpoint excludes the rejected panel, collector and installer/consent changes. The prior prototype's language/theme behavior passed owner testing; this clean rebuild needs its own installation, label and native-query check. The normal updater remains the default; the simulation requires `PROX_NATIVE_UPDATE_FLAGS=-DPROX_NATIVE_UI_PROBE=1`.

The owner reports that clean build pro-x-0306568d2fc4 passed the requested installation/native-query check. The earlier prototype also passed owner language/theme testing. These results do not establish real native GitHub OTA, broader WPA2/model compatibility or Infinity startup. Infinity and powered-on card reinsertion remain deferred. For normal OTA behavior, restore the previously tested v0.3.1 manual package through its manual installer; this diagnostic cannot update itself from GitHub. Keep its matching source ZIP and SHA-256 list.

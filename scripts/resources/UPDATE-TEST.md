# PRO-X v0.3.0 — GitHub system-update test candidate

The earlier `pro-x-c41ee2ff7b5e` transport prototype passed three owner-supplied
PSP API/redirected-asset tests (reports 006/007/008). The current af71 candidate
completes all fourteen catalog/profile-reuse checks in reports 024–037 on attempt
one. Positive OTA installation and fresh-session retry recovery remain unvalidated.
The beta distribution is for manual testing. Its prerelease classification keeps
it outside stable automatic discovery; see the release notes for matching assets.

The owner has since confirmed normal X installation/start of
`pro-x-226d4f7fe3ac`. Its report 009 completes the real catalog check; reports
010/011 stop in DNS before TLS. For that already-installed console, this
fresh-session retry iteration changes only the client. Leave it for XMB, replace
`PROXUPDATE/EBOOT.PBP` plus the matching `cacert.pem`, preserve reports and test
the Internet entry twice. Installer/Fast Recovery/CFW bytes are unchanged; no
manual reinstall is needed for this iteration. The initial bootstrap below
still applies when the required firmware/bridge is not already active.

The owner-reported legacy System Update route after storage reinsertion remains
unresolved. If the expected PRO-X client does not open, cancel the legacy updater.

## Manual bootstrap and current PSP test

Copy the matching `PSP/GAME/PROUPDATE`, `FastRecovery` and `PROXUPDATE` folders
from the manual ZIP into `PSP/GAME`. Keep `cacert.pem` beside the client's EBOOT.
Merge folders and preserve reports, saves, network profiles, plugin lists,
translations and existing preferences. Keep the previous recovery package.

Open the Installer from Games. Use its normal X installation/start path, then
press X again at `Completed. Press X to start CFW.`. Do not use Triangle or L.
Check `6.61 PRO-X 0.3.0` in System Information and the PRO-X VSH menu. Existing
PRO/Infinity startup contracts are retained; this does not establish a new
Infinity hardware result. No Sony firmware, Infinity or IPL tool is included.

Open Settings -> System Update -> Update via Internet. The client reads PSP's
last network-profile ID using read-only sceUtility NID 0x4FED24D8 and validates
that it still exists. When available it connects that profile automatically,
avoiding the second selector after XMB. This uses the last stored ID, not a live
connection handed across loadexec. The owner reports selection working and the
fourteen logs confirm profile id 1; broader profile cases remain pending.
If the ID is missing/invalid/deleted, choose
a saved profile with Up/Down and X. It does not edit any profile.
Create a connection in XMB first if needed. Circle or HOME cancels the client.
The PSP clock must be correct for certificate validation.

DNS reports now separate the SDK worker's exact wait status from cancellation,
the 12-second lookup watchdog and the 90-second request budget. Worker entry/
completion, stop/join/release returns and a post-join APCTL state are recorded.
`worker_after_stop` may be induced by stopping the operation; it does not prove
the original DNS cause. GOT_IP by itself does not prove data connectivity.
Teardown waits up to five seconds for DISCONNECTED and checks each term/unload.
Failure preserves dependencies and prevents installer handoff. Before TLS, a
failed DNS lookup or TCP connection can retry, up to five attempts to establish
the connection. Four extra attempts are shared across the whole updater run,
including catalog pages, assets and redirects. Each retry releases the joined
resolver worker/socket first and pauses one second, polling cancellation every
20 ms, then closes the user-mode net/Inet/APCTL/resolver session, observes
DISCONNECTED and recreates those services before reconnecting the same profile.
Network modules remain loaded for valid imports; firmware/WLAN drivers and the
physical Wi-Fi switch are untouched. Partial close/init flags are retained so
final cleanup does not terminate an already-closed service twice. Each new
association waits up to 45 seconds for IP within the original request budget.
Internal/cleanup errors, cancellation and the original 90-second request
deadline stop retries; the deadline is never reset. A joined/stopped DNS watchdog
may retry only after successful stop and release. The SDK still uses its existing
five-second/one-retry lookup settings. Connection success immediately proceeds
to mandatory TLS validation. Certificate/handshake errors, HTTP status including
403/429, invalid metadata and failed downloads/hashes do not restart the request.
There is no alternate DNS server, cache/IP override or relaxed TLS check.
The previous retry build's report 020 recovered on attempt two, while 023
exhausted five DNS attempts without recreating its session. Fresh-session
recovery and automatic profile reuse are NOT RUN on PSP in this delivery.
The screen and one private report show all attempts without asking for a profile
again during retry. No saved settings are changed.

The current v0.2.0 stable release is older than the running v0.3.0 and must not
be offered. Expect `No newer compatible stable update found in the checked
release catalog` when the current catalog contains no eligible newer contract.
An API, DNS, certificate or rate-limit error is a failed check, not confirmation
that the firmware is current. No continuous retry or Sony/legacy PRO fallback.

Return with Circle, preserve the next `report-NNN.txt`, and check preferences,
plugins, Fast Recovery and the storage-update method without starting a Sony
installation. Report Infinity behavior only when Infinity is actually present.
Manual-install regression and these physical cases remain pending this candidate.

## Future compatible stable update

The client reads the public repository's releases in pages of ten (five pages
maximum). It requires boolean `draft=false` and `prerelease=false`, a strict
stable product tag, and a numerically newer version. Names containing or lacking
"beta" do not determine eligibility. Duplicate product versions, malformed API
data and an incomplete bounded catalog cause an error, not an update decision.

Only the exact `pro-x-update.json` and `pro-x-<version>-661-update.zip` assets
from the same release are considered. Source/manual/diagnostic ZIPs cannot be
selected instead. The highest compatible newer stable contract is chosen.

X first confirms downloading the displayed version/size/summary. Free space
must cover the ZIP, extracted files, adjacent replacement copies and reserve.
Downloads stream into a freshly created private `.stage-...` directory on the
same storage device. Size/SHA-256, ZIP structure, every allowed file hash and
PBP headers are checked before offering installation. Failures/cancellation
before handoff remove only that newly created stage; installed files remain.

X then opens the verified staged Installer after network shutdown. The launcher
forwards a copied manifest hash and the just-validated source-version snapshot.
The installer independently checks firmware/model, target product, version order,
minimum client, manifest hash and all staged files before flash setup. It has
no new private SystemControl import, preserving manual bootstrap from official
firmware. The live source version is checked by the client immediately before
handoff; a restart ends that session. Arguments are not a cryptographic identity
for arbitrary locally launched code, which is outside the network-update trust
boundary and can already run the manual installer.

The Installer requires a fresh X to install; R exits before writing. Triangle
and forced L reinstallation are disabled for OTA requests. AC power, a present
battery and at least 50% charge are required by the OTA path and rechecked before
writing. This is a conservative new OTA policy; the inherited manual installer
does not provide this same power gate. R can cancel preflight file verification.
Cancellation is disabled once firmware/storage commit begins.

After the updater exits, the Installer prepares verified `.prox-new` storage
copies, uses the existing PRO smart installer for flash, and replaces the
matching Installer/Fast Recovery/client/CA files. Exact `.prox-old` backups are
used for storage rename recovery and removed after success. A storage failure
attempts restoration and preserves evidence/backups when restoration fails.
Flash writes retain the base installer's nontransactional behavior: there is
no guarantee of rollback after power loss or a partial flash-write failure.

Press X to start CFW after successful completion. `install.txt` records the
private installer result. `installed.pending` distinguishes prepared from
committed attempts. On the next client launch, the running product version must
match a committed target before `installed.observed` records the post-restart
version. This is version observation, not attestation of every loaded module's
build hash. A prepared/failed attempt is not marked successful. Completed stages
are retained for recovery; do not delete unknown backups to bypass a blocked
preflight. No settings/plugin/network configuration is replaced by this flow.

## Schema 1 and generated assets

`pro-x-update.json` contains schema/product/repository, version/tag/stable channel,
firmware target, supported CrossFW model IDs, minimum updater version, build ID,
bounded summary, and exact package/file sizes and SHA-256 values. GitHub's asset
digest is also checked when present. HTTPS authenticates the GitHub origin;
there is no invented project signature key or token embedded in the console.

The local package declares model ID **1 only**, based on the owner's existing
6.61 installation/transport evidence. Positive OTA installation still needs
validation; no other model or router is inferred compatible from that result.
Minimum schema-1 updater version is 0.3.0. The first updater installation must
use the manual bootstrap; older firmware does not gain a client from a release
classification change.

The stored-only ZIP has exactly eight regular files:

```
PROUPDATE/EBOOT.PBP
FastRecovery/EBOOT.PBP
PROXUPDATE/EBOOT.PBP
PROXUPDATE/cacert.pem
LICENSE
THIRD_PARTY_NOTICES.md
SOURCE.md
licenses/mbedtls/LICENSE
```

The last four are stored under PROXUPDATE when installing. All paths are fixed
allowlist entries; absolute/traversal paths, links, duplicates, extra files,
directory records, compression, ZIP64, extras/comments, conflicting local/central
records, invalid CRC/size/hash and oversized expansion are rejected. Streaming
extraction never writes to flash or over the executing client.

Generate assets with `python scripts/package.py` after a successful pinned build
and verification. It writes the manual ZIP, corresponding source ZIP, stored
OTA ZIP, update JSON, detailed/compact manifests and SHA-256 list to `builds/`.
All executable/CA bytes match across manual, OTA and loose delivered files.
Publishing all corresponding assets requires separate explicit authorization.

## Limits, local checks and pending evidence

TLS 1.2 requires certificate chain, hostname, dates, SNI and checked KIRK RNG.
Every redirect must use HTTPS and an allowed GitHub asset host. Limits remain:
four redirects; 90 seconds per request including redirects; supervised DNS
12 seconds per lookup; TCP connect 15 seconds per attempt; up to four extra
DNS/TCP attempts per client invocation within the original request deadline;
16 KiB total headers including status;
8 KiB line buffer (8191 wire bytes plus capacity for termination); 2 KiB URLs;
256 KiB API/page; 8192 JSON tokens; nesting depth 16; 32 KiB update manifest;
16 MiB package/aggregate file cap; 4 MiB per file; 2 MiB Mbed TLS allocations.
The client uses an 8 MiB heap and 128 KiB main-thread stack; the installer uses
a 1 MiB heap and 64 KiB main-thread stack. Mbed TLS report peaks exclude other
heap/stack/system memory. Whole-client peak use still needs physical measurement.

The space-query layout follows the
[PSPSDK SCE_PR_GETDEV structure](https://pspdev.github.io/pspsdk/pspiofilemgr__devctl_8h_source.html).
The pinned older SDK lacks that header; the private five-field layout is checked
at compile time and tested with mocked SDK returns and 64-bit multiplication.

Run `sh scripts/test-native.sh` in the pinned Docker image. Run
`PROX_RUN_TLS_TESTS=1 python3 tests/test_update_https.py` and then
`PROX_RUN_CONTRACT_TESTS=1 python3 tests/test_update_contract.py` with
`--network none`. Native fixtures have no flash-installation capability and do
not replace physical module loading or an official stable update test. Production
builds have no fixture endpoint, keys, arbitrary URL override or rename fault.

After packaging, run `python3 scripts/verify-native-update.py` in that image to
check the real generated ZIP/JSON with the production C consumer. Its locally
constructed release/older-version input is a container-validation fixture, not
evidence of an official release or an installation from older firmware. An
optional `--catalog <saved-api-json>` checks the current phase-3 catalog using
native adapters without contacting the network.

The official newer-stable positive update, its restart and a second update are
BLOCKED/NOT RUN until suitable authorized release assets and hardware evidence
exist. The three successful diagnostic transfers do not establish these results.
No commit, push, PR, tag, release or release mutation is authorized by tests alone.

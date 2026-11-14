# PRO-X Network Diagnostic 1.0

Standalone diagnostic for PSP firmware 6.61. Physical execution is **NOT RUN**
for the new native-HTTP comparisons. Earlier owner reports first showed DNS/TCP
timeouts, then successful gateway HTTP, DNS and the historical Sony HTTP response
through raw sockets. The owner still sees an XMB Internet-test error with unchanged
settings; the photo confirms that field but contains no error code. The cause is
unresolved. This is a homebrew application, not a firmware
installer or a correction of Sony's Internet connection test. It does not change
the installed PRO-X version. Use the existing PRO-X v0.2.0 firmware for this test.

Copy the `NETDIAG` directory to `PSP/GAME/` on the Memory Stick or PSP Go internal
storage. Activate PRO-X, enable the WLAN switch, then launch **PRO-X Network
Diagnostic 1.0** from Game. Choose your existing connection with UP/DOWN and
press X. No network password needs to be entered in this app. To compare with
another CFW, run the same binary with the same profile and retain both reports.
The tool does not independently verify which CFW is active.

The report is written incrementally to `PSP/GAME/NETDIAG/report-001.txt`, then
`report-002.txt`, etc. Existing reports are preserved (up to 999 per storage).
It is a **private report**: it includes local IP, gateway, DNS, profile ID, channel
and signal strength, but no profile name, SSID, MAC/BSSID, password or key.
Review it before sharing. HTTP response headers and bodies are not recorded.
If writing fails, the screen reports the failure; an incomplete file may remain.

Tests:

- Loads and initializes the standard networking modules. It applies no patches.
- Connects using a saved profile (IDs 1 through 99), logs state changes and raw
  AP control event/error codes, and waits up to 45 seconds for IP readiness.
  IP readiness is not proof of Internet access or WPA2 negotiation.
- Reads assigned IPv4, mask, gateway and DNS, plus channel/signal/security API
  code. The security code is not independent evidence of WPA2/AES negotiation.
- Connects to `1.1.1.1:80` and sends a small HTTP request as a DNS-independent
  control. A timeout is inconclusive; the destination or network may block it.
- Tests the assigned gateway with HTTP on port 80, a TCP-only connection on
  port 443 (no TLS handshake), and a DNS query on UDP port 53. Admin ports can
  be closed/filtered and a router may not provide a DNS proxy, so failure of
  these probes alone is inconclusive. No login credentials are sent.
- Tests TCP connectivity to `9.9.9.9:53` as a second provider's literal-IP
  control; this is not a full TCP DNS transaction or an HTTP/HTTPS test.
- Uses the PSP native resolver for Sony and NeverSSL. Each call requests a
  three-second timeout and one retry, supervised by a twelve-second watchdog.
  A return code following a forced stop is labeled as cancellation output,
  not the original DNS failure code.
- Separately queries the configured primary/secondary DNS and public DNS
  `1.1.1.1` and `9.9.9.9` over UDP port 53 for the historical Sony hostname
  (three seconds each). Duplicate configured Cloudflare queries are reused.
  The public query is diagnostic only; no DNS/profile setting is changed.
  UDP can be filtered, redirected or intercepted. DNS replies use a bounded
  parser; RCODE, malformed/truncated responses and missing A answers are logged.
  No DNS-over-TCP retry or DNSSEC validation is performed.
- Requests `http://fj00.psp.update.playstation.org/networktest/trial.txt`, using
  the native result or an explicitly logged direct-DNS fallback (including the
  gateway or Quad9). The test checks
  for status 200 and the historical one-byte `p` response, without claiming that
  this is the exact destination/logic of Sony firmware 6.61.
- Requests `http://neverssl.com/online.html` when native resolution succeeds.
- After the existing raw Sony probe, loads the stock URI/HTTP parsers and HTTP
  module and tests the same historical Sony URL through `sceHttp`, using HTTP
  1.0 and 1.1 templates separately. It logs module/setup returns, template,
  connection and request creation, send/read errors, underlying network errno,
  status, length, body byte count and whether the response is exactly `p`.
  Body contents, headers and authentication data are not written to the report.
  These are homebrew API comparisons; the exact XMB URL/options are unverified.
- Native HTTP uses direct connections, no redirects, cookies, authentication
  or keep-alive. Resolve/connect/send/receive deadlines are 3/5/3/5 seconds,
  with one DNS retry. If a required setting fails, that probe stops before Send.
  Send/Read run in a worker supervised for 25/6.5 seconds; cancellation requests
  `sceHttpAbortRequest` and waits one second. If the worker cannot stop, the app
  closes the report and exits without unloading live libraries. Post-abort codes
  are labeled cancellation output. This behavior remains physically unvalidated.
  Only two response-body bytes are read, sufficient to check the one-byte target;
  this is not a sustained download. No HTTPS initialization or cookie storage
  APIs are used, and module/request cleanup is logged.
- TCP connection, send and receive have separate deadlines of 5/3/5 seconds.
  Reads are limited to 4096 bytes, redirects are not followed, and HTTPS is not
  tested. Partial/large/chunked responses are not full-page validation.

Press Circle to cancel an active test, or HOME to request exit. At completion,
the report is closed before X returns to XMB, with an automatic return after
20 seconds. Profile selection times out after two minutes. If the native DNS
worker fails to stop within one second of its watchdog/cancellation request,
the report is closed and the app exits rather than unloading live libraries.
Cancellation, automatic return and all network behavior still require physical
validation. No flash writes, profile edits, proxy changes or firmware installation
are performed; the probes connect directly even if the saved profile uses a proxy.

Build using the project's pinned local Docker toolchain:

```text
python scripts/build-netdiag.py
```

Run the command from the repository root, or the extracted ZIP's `source/`
directory, with the pinned `pro-x-toolchain:phase1` Docker image already installed.
This builds only the diagnostic. Deliverables go to `builds/`, logs and temporary
compiler output to `artifacts/`. The ZIP includes the diagnostic source and GPL
license. It preserves the firmware installer, Fast Recovery and prior WPA2
identification packages. No product-version increment is consumed by this
standalone investigative tool.

Each new diagnostic ZIP/manifest/hash filename includes its build ID so earlier
delivered packages remain available. The loose `PSP/GAME/NETDIAG` directory is
updated to the latest executable; replace its EBOOT without deleting your logs.
Reports for this iteration identify `format=3`, `tool=1.0` and the unique build.
The pinned SDK's eight-byte `timeval` ABI is checked during compilation.

The owner reports other devices browsing successfully on the same Wi-Fi and
Cloudflare DNS configured on the router. This motivates the gateway and second
provider comparisons; it does not establish a Cloudflare, router or WPA2 defect.
Service reference: [Quad9 services and ports](https://docs.quad9.net/services/).

API reference: [PSPSDK networking](https://pspdev.github.io/pspsdk/group__Net.html).
Native HTTP reference: [PSPSDK psphttp.h](https://pspdev.github.io/pspsdk/psphttp_8h.html).
The networking API declarations and samples in the pinned toolchain were
inspected before implementation; no upstream diagnostic code was copied.

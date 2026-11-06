# Printer-hosted dashboard

Static browser UI and ARMv5 C backend; no Node/JavaScript runtime runs on the
printer. Open `http://PRINTER_IP:8081/`. The stock GUI/TCP8899 service still owns
printer control; the vendor USB MJPEG server uses 8080.

The dashboard provides native GX previews/metadata, bounded G-code conversion,
file listing/deletion, deliberate print prompts, camera/live status, identity
footer and dark/light/system appearance. Orca uses the OctoPrint-compatible
upload surface. See [usage](../docs/usage.md) and [architecture](../docs/architecture.md).

| Route | Purpose |
| --- | --- |
| GET /api/version | OctoPrint-compatible identity |
| GET /api/session | LAN browser/slicer request key |
| GET /api/status | M119/M105/M27 status and progress |
| GET /api/files or /api/files/local | Root-level GX/G-code file list, storage, identities and metadata |
| GET /api/preview/NAME | Native GX BMP preview |
| POST /api/files/local or /upload | Multipart file upload; explicit optional print request |
| DELETE /api/files/local/NAME | Request key + current If-Match required |
| GET /api/printer | Live M115 identity |
| GET /api/camera/stream or /api/camera/snapshot | Fixed local camera proxy |
| GET /api/notifications | Latest non-autostart upload |
| POST /api/notifications/dismiss | Key + notification identity |
| POST /api/print/NAME | Key + current identity + GX validation + readiness |

G-code input is anonymous temporary storage. After metadata/thumbnail conversion
and output validation, the source is closed before the GX is atomically published.
Original G-code bytes remain intact within GX. Invalid/interrupted replacement
retains the existing GX and cleans private staging.

Only regular root-level print files are exposed. Symlinks, hidden files, unrelated
temporary data, subdirectories and logs are not a file-management API. Uploads
and deletes require both machine and movement READY. File identity is rechecked.

Limits: 100 MiB upload, 1024-pixel thumbnail dimension, 2 MiB encoded thumbnail,
12 MiB decoder budget, four workers, 180-second worker lifetime, 2048 listed files.
Free-space reserve is twice request bytes plus 16 MiB. Camera: one proxied live
viewer, bounded reconnect/health checks, no frames saved to flash.

`GET /api/session` intentionally returns the key to LAN clients. This is a trusted
LAN/CSRF guard, not multi-user authentication; there is no TLS or user account
system. Never publicly expose dashboard, camera, SSH or stock control ports.

Build: `python tools/build_web_service.py`. Linux integration fixtures are in
`tests/integration.py`; [development](../docs/development.md) gives the commands.
Physical upload-and-print acceptance remains separate from fake-printer tests.

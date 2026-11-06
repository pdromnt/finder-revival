# Dashboard, Orca and camera

Open **`http://PRINTER_IP:8081/`** on your trusted LAN. Printer information in
the footer comes from the target's live M115 reply, including its own serial.
No PC bridge, Node runtime or external conversion server is needed.

## Files and Orca

The repository includes [Finder 1 / 2 / Plus Orca presets and bed assets](../slicer/orca/README.md).
Enable the **Flashforge Guider 2s 0.4 nozzle** system preset before importing.
The included PLA preset is for **AllMetal-modified printers** and additionally
inherits `Generic PLA @Flashforge G3U`. Follow that guide for import, parent
availability, local bed-asset paths and separate model licensing.

Configure an OctoPrint-compatible physical-printer connection in OrcaSlicer
with the printer's dashboard URL and its own displayed API/request key. Exact
Orca UI labels can vary. Test **upload without print** first.

Orca `.gcode` is parsed on the printer into `.gx`. Supported metadata and the
first supported PNG/JPEG thumbnail become a native header/80×60 BMP. Original
G-code bytes are retained inside GX. The source is an anonymous temporary file:
it is closed/discarded before the validated GX is atomically published. No
extra source `.gcode` is kept as a user-visible file after conversion.

Uploads produce a deliberate Print now / Keep for later notification, including
external slicer uploads. Merely uploading must not start a job. Explicit API
`print=true` is a physical-action request and needs separate supervised testing.

The local dashboard supplies file listing and guarded deletion missing from the
stock TCP interface. Delete/print requests require a current file identity and
positive machine/movement readiness. Unsupported filenames are normalized for
new uploads; existing Unicode file names can still be listed/deleted.

Limits include 100 MiB uploads, 1024-pixel thumbnail dimensions, 2 MiB encoded
thumbnail input, a 12 MiB decoder allocation budget, four request workers,
2048 listed files, and free-space checks. See [architecture](architecture.md).

## Camera

A compatible UVC/MJPEG USB camera is hotplug-detected. The tested C270 produced
640×480 JPEGs. The vendor capture process uses port 8080; the dashboard proxies
it through `/api/camera/stream` and `/api/camera/snapshot`.

One dashboard stream at a time is supported. Snapshots remain available. Hidden
tabs release their stream, and bounded reconnects handle the old camera server.
Frames are not saved to flash. Do not expose the unauthenticated vendor camera
port to an untrusted network.

## Status is polling

The printer-facing interface is raw TCP, not WebSocket. The dashboard currently
queries M119/M105/M27 every five seconds and shares serialized command access.
M27's numerator out of 1000 means tenths of a percent on this firmware. Unknown
filename/ETA/elapsed values are not invented. There is no redundant Refresh button.

The touchscreen retains native printing/maintenance controls with a modernized
home, subscreens/dialogs, neutral homing copy and native GX previews. Local-only
policy removes cloud/update navigation. This is not a new motion planner.

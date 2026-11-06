# Toolkit parity

The native dashboard retains the earlier toolkit's upload/conversion, preview,
USB camera, status/progress, explicit after-upload print decision, external Orca
notification and appearance controls. It adds direct listing and guarded deletion
on printer storage without requiring a PC bridge.

The identity footer reads live M115, including the actual target serial. Dashboard
8081 and camera 8080 are distinct. The redundant Overview/Files pane bar and manual
refresh button were removed. Current status sampling is five-second request/response
polling, not an implemented WebSocket/state-push bridge.

One camera stream is supported; a C270 returned a validated 640x480 JPEG after
hotplug. Camera frames are never saved. Missing telemetry is unknown, not invented.

Eighteen fake-printer groups cover protocol framing, readiness/movement guards,
mutations, replacement/cancellation, camera exclusion, print acknowledgement and
cross-client upload notifications. A live READY reply is not real-print acceptance.
See ../docs/validation.md for exact evidence boundaries.

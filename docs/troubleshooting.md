# Troubleshooting

## USB is not detected / bootstrap does not run

Use a **USB drive of at least 16 GB formatted as FAT32**. FAT32 is mandatory
for the supported install, backup and recovery workflow; exFAT and NTFS are
not supported. Verify the actual format and free space before investigating
the model marker or startup hook. Back up existing stick contents before any
reformatting; copying the release ZIP onto an incompatible filesystem is
insufficient.

## Old-looking menu

Read `/run/revival-ui-installed/failed`, `integrity.log` and `ui.log`. Hash/ABI
guards may have selected stock fallback. Check that exactly one UI runs and the
correct launcher owns it. Never restart a GUI during a job. Stop its launcher
and supervisor too; otherwise fallback can leave a second stock UI running.

## After Factory Reset

See the [reset audit](factory-reset.md). The stock action changes the display
name, registration, language and hotspot name; it does not directly remove
Revival program/key paths. Recheck the printer's address/hotspot and reload the
dashboard if access or displayed identity changed. A lost network connection is
not by itself evidence of an uninstall. Live reset/next-boot qualification is
still pending; do not trigger another reset to diagnose a connection problem.

## SSH is missing

Check the printer's current address and port 2222. Verify your public key was
installed, host-key identity is the expected target, and the init script ran.
Do not disable host verification or enable a shared root password to bypass it.
USB temporary access and UART are separate recovery options.

## Camera unavailable

Look for `/dev/video0`, then the camera watcher and MJPEG process. Another
process may own the camera/8080, or one dashboard stream may already be active.
Reconnect the USB device and allow the watcher to detect it. No video device
means live capture cannot be validated; an active watcher alone is not a frame.

## Status or uploads fail

If Orca's embedded Device tab reports **Malformed or oversized request** while
the ordinary browser works, reload the Device tab after updating the dashboard.
Older dashboard builds used a different API-key header spelling from Orca's
injected header. Browser normalization combined the two values and the server
rejected the resulting request. The corrected dashboard uses the same spelling;
authentication and request limits remain intact. A successful upload may already
have produced its GX file, so inspect the list before uploading it again.

A generic file icon can remain after an individual thumbnail request fails.
Refresh the file list to retry; if selecting the file shows its preview, the GX
contains the thumbnail and does not need uploading again.

Check raw TCP 8899 and vendor GUI readiness. Another long-held client can occupy
the listener. The dashboard is HTTP 8081, not the original toolkit's PC bridge.
Do not forward these ports publicly. File mutations require both machine and
movement READY and the current file identity.

## Garbled log names or ext4 errors

Stop installation/cleanup and follow [storage repair](storage-repair.md).
Do not run fsck on mounted partitions or delete corrupted names blindly.

## Black screen / boot failure

Follow [UART recovery](recovery.md). The first bad splash failed in the ARM
decompressor even though ordinary CRCs were correct. A kernel booting in RAM
does not prove the installed rootfs/UI is healthy. Restore only this printer's
verified files; do not flash somebody else's entire eMMC image.

## WAN blocked / routed VLAN client unreachable

Read [security](security.md). The local-only policy is deliberate. On-link LAN
routes are retained; remote management from a different routed subnet needs an
explicit reviewed network exception. Never solve this by enabling vendor OTA.

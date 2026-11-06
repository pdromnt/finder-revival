# Factory Reset and Finder Revival

**Stock 2.8.4.366 F2.12 20210823: code-audited and reset/power-cycle tested on
the development printer (2026-11-06).**
The touchscreen Factory Reset action is a settings operation. It does not
reinstall the stock firmware or intentionally uninstall Revival. A reset is
not part of installation or a substitute for the [restore guide](restore.md).
It does not reboot automatically; the operator power-cycled the printer after
the immediate checks to validate the next startup separately.

## Audited touchscreen path

The supported stock executable's `Setting::itemDoClicked(int)` is at
`0x156eb0`; Factory Reset is action ID **16**. Only after its confirmation dialog
returns Yes does it perform the following operations:

| Area | Observed stock action |
| --- | --- |
| Display name | Set to `My 3D Printer` through the machine-name file helper |
| Flashforge Cloud | Stop its client if active; set its switch OFF |
| Registration | Remove the registration-code file, including `/data/RegistrationCode.txt` |
| Hotspot | Rewrite `ssid=` in `/etc/rtl_hostapd_2G.conf` to `finder`; restart the AP if enabled |
| Old core files | Remove `/data/log/core*` |
| Vendor checksum flags | Remove `/opt/md5*` |
| Polar Cloud | Unregister/stop if its switch was ON, then set the switch OFF |
| Language | Replace the language setting with English and reload translations |

The file-writing helpers were also inspected: the relevant setters operate on
`machine-name`, `cloud-ff`, `cloud-username-pin` and `language-config` under the
vendor settings directory. `Tool::myexec` passes the named commands to its
process helper. The initial code audit was read-only; the later operator-run
hardware test is recorded below.

The binary also contains a different `PickSettingsConf::resetFactory()` routine
for `/opt/Printer` and `/etc/Printer`. That is not the handler used by the
touchscreen action above; matching a function name alone is insufficient.

## Effect on Revival

The audited touchscreen handler does **not directly target**:

* `/PROGRAM/revival-ui`, `revival-web`, `revival-logging` or `revival-network`.
* `/PROGRAM/software/2.8.4/finder-revival.sh` or `/PROGRAM/ffstartup.cfg`.
* `/etc/init.d/S02revival-logs`, `S03revival-local-only`, `S50revival-sshd`,
  `S51revival-web` or `S52revival-camera`.
* `/etc/revival` owner authorization/SSH host keys or `/etc/revival-web/key`.
* The active/cached kernel, stock GUI, serial-number/calibration files or GX jobs.

The live reset and power-cycle checks confirmed that the installed theme,
services, keys, logging mitigation and outbound policy survived on this target.
Cloud switches stayed OFF, consistent with Revival's local-only policy. The removed
`/opt/md5*` names do not overlap Revival's SHA256 manifests under `/PROGRAM`.

The result applies to this printer and installed baseline. Conditional paths
with active cloud clients or an active hotspot were not exercised; the test
used cloud switches OFF and Wi-Fi client mode. It is not a guarantee for another
firmware/board.

Display name and registration changes are deliberate vendor behavior, unlike
the identity preservation promised for installing/updating Revival. Network
access can be interrupted when an active hotspot restarts; re-establish the
connection and reload dashboard information after a reset.

## Completed hardware check

Both the immediate post-reset state and a subsequent operator power cycle passed:

| Check | Observed result |
| --- | --- |
| Reset settings | Name `My 3D Printer`, English, hotspot SSID `finder`, registration file absent, cloud switches OFF |
| Revival | Launcher retained; theme, logger and network guards loaded; UI/web integrity checks passed |
| Owner access | Existing SSH host identity and owner authorization worked; dashboard key matched the saved pre-reset hash |
| Serial/calibration | Serial, extruder/servo calibration and Z offset matched the archived PC baseline; retained after power cycle |
| Next boot | A new boot was observed and the operator confirmed the normal menu; protected file hashes matched the immediate post-reset state |
| Vendor checksum flags | `/opt/md5*` remained absent after startup; on-disk control and library bytes matched across boot |
| Network/camera | LAN dashboard and control status worked; WAN reject routes and cloud/update guards were active; valid 640×480 camera JPEG |
| Logging/storage | Stock log sink remained `/dev/null`; 256 KiB RAM cap active; root/data error counts zero; valid empty persistent log directory |

Storage checks were read-only online observations, not another offline fsck.
Keep the [offline consistency-check distinction](storage-repair.md). The two
earlier GX test jobs had been deliberately removed by the owner before the reset;
the remaining job listing and file identity stayed unchanged across boot.
No byte-for-byte pre-reset job-content comparison was made. The code audit
found no job deletion in the reset handler.

The control/library comparison checks on-disk files, not a motion-controller
flash readback.

Do not factory-reset the development printer merely to satisfy a documentation
checkbox. A future reset test needs operator authorization and externally saved
settings/registration/calibration/keys, with UART recovery available.

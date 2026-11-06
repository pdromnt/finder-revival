# Factory Reset and Finder Revival

**Code-audited against stock 2.8.4.366 F2.12 20210823; not live-reset qualified.**
The touchscreen Factory Reset action is a settings operation. It does not
reinstall the stock firmware or intentionally uninstall Revival. A reset is
not part of installation or a substitute for the [restore guide](restore.md).

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
process helper. No reset routine was invoked on the printer during this audit.

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

Thus file-level inspection indicates that the installed theme, services, keys,
logging mitigation and outbound policy survive this settings reset. The cloud
switches are set OFF, consistent with Revival's local-only policy. The removed
`/opt/md5*` names do not overlap Revival's SHA256 manifests under `/PROGRAM`.

This is an inference from the reset's code/file footprint. We have not executed
the reset, exercised every conditional cloud/AP branch, or qualified the next
boot after deleting vendor checksum flags. Do not describe it as a completed
hardware reset test or a guarantee for another firmware/board.

Display name and registration changes are deliberate vendor behavior, unlike
the identity preservation promised for installing/updating Revival. Network
access can be interrupted when an active hotspot restarts; re-establish the
connection and reload dashboard information after a reset.

## Controlled qualification still required

A future live reset test requires explicit operator authorization because it
changes identity/network settings. Before it, preserve this printer's current
settings, registration, calibration and keys externally and verify UART recovery.
After confirmation, check both immediately and after a normal reboot:

1. The launcher still selects Revival; UI/service integrity checks pass.
2. SSH host identity, owner authorization and dashboard key match the saved values.
3. Cloud switches remain OFF, update guards load and outbound routes are active.
4. Bounded RAM logging remains active; storage error counters/log directory
   show no recurrence. Keep the [offline consistency-check distinction](storage-repair.md).
5. Serial/calibration and protected print files retain their bytes; changed
   display name/registration/hotspot/language agree with the stock reset action.
6. Review vendor reinitialization after `/opt/md5*` removal before claiming
   unchanged controller/library behavior.

Do not factory-reset the development printer merely to satisfy a documentation
checkbox. This guide records the audited behavior and the remaining acceptance.

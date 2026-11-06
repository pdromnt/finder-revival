# Finder Revival USB quickstart

Experimental firmware overlay for the **Linux/eMMC Finder Plus / V2** running
**2.8.4.366 F2.12, build 20210823**. Check the full version in About and read
[compatibility](compatibility.md). The package verifies exact binary hashes;
another firmware or board must not be forced through its guards.

Use a **16 GB minimum USB drive formatted as FAT32**. exFAT and NTFS are not
supported. Back up any existing stick contents before formatting it.
Routine installation does not require UART, soldering or pre-existing SSH.

## 1. Prepare the stick

Verify the downloaded USB ZIP against the supplied `SHA256SUMS`, then extract
it to the stick's root. It must contain `0007`, `flashforge_init.sh` and the
`revival` directory directly, not inside another enclosing folder.

Keep the large `runtime-sources.zip` on the PC; do not extract it onto the stick
or printer. It contains matching source/relink materials, not an installer.
The included `slicer/orca/` files are PC-side references and are not installed
on the printer. Keep the presets/bed assets in a stable PC folder for Orca.

First-time SSH setup defaults to **root / `flashforge_revival`**, port **2222**;
change it with `passwd` after login. For key-only setup instead, add your own
ordinary public key as `revival/authorized_keys`. Never copy a private key there.
Existing SSH configuration, passwords and owner keys are preserved on upgrades.
See [identity](identity.md) and [security](security.md).

## 2. Run CHECK first

Create `revival/REQUEST.txt` containing exactly:

```text
CHECK
```

With the printer off, insert the stick and power on. Keep power and USB connected
until **Checks passed / Remove USB, then power-cycle**. The operation checks both
ext4 filesystems and boot FAT while unmounted, and installs nothing.

Remove the safely unmounted stick, switch the printer off for ten seconds and
boot normally without it. Return the stick to the PC and inspect the newest
`revival/reports/usb-*/` directory: require `CHECK-PASSED.txt` and `FINISHED.txt`,
with no `FAILED.txt`. Each request is consumed once; it is not automatically retried.

If a check stops, inspect its report before proceeding. Do not blindly delete
corrupt log entries or repair a mounted filesystem; follow [storage repair](storage-repair.md).

## 3. Back up this printer

Before installing, preserve your own serial/calibration/settings and obtain a
complete backup using [the backup guide](backup.md). The USB **BACKUP** request
uses the same off/insert/power-on sequence and needs at least 8 GiB free.
It can take an hour or more. Wait for **Backup complete** and its removal instructions.

Copy the whole backup directory to the PC and independently verify all ten
image hashes and lengths as documented. Keep the original kernel and startup
configuration accessible, alongside the [recovery guide](recovery.md).
Backups contain private owner data; do not share them with other printers.

## 4. Install

Replace the consumed request with a new `revival/REQUEST.txt` containing:

```text
INSTALL
```

Repeat the off/insert/power-on sequence. Keep power and USB connected until
**Installation complete / Remove USB, then power-cycle**. Then remove the stick,
switch off for ten seconds and boot normally without it.

Copy the whole new report directory to the PC. Require the run's `FINISHED.txt`
and `install/COMPLETE.txt`, with no `FAILED.txt`. Preserve `install/rollback-private.tar`
and its inventories/checksums, `kernel-boot-before.img` and `kernel-cache-before.img`:
they belong to this printer and enable rollback. Keep the whole report together.

**Installation incomplete / Do not reboot yet:** leave power and USB connected,
inspect the report and follow [restore after failure](restore.md).

The installer preserves identity, calibration, network settings, print files and
existing keys. First-time password SSH setup intentionally initializes the root
password and saves its prior record privately on USB. It disables legacy cloud
switches while retaining account data.
One INSTALL supplies the touchscreen, SSH, dashboard and boot splash. The splash
is prepared from your own verified kernel on USB, then both named internal kernel
files are staged, checked and replaced. The motion controller remains untouched.

## 5. Check normal operation

Confirm the native menu and your own serial/calibration/settings, then open
`http://PRINTER_IP:8081/` on a trusted LAN. Use **Slicer setup** for the printer's
OctoPrint-compatible Orca URL/key. Uploads convert `.gcode` to `.gx` locally;
uploading alone does not start a print. See [usage](usage.md).

## Rollback and reports

To undo this installation, keep its original report on the same stick and create
a new `REQUEST.txt` containing **ROLLBACK**. Follow [USB rollback](usb-maintenance.md#rollback),
waiting for **Rollback complete** before removing USB and power-cycling. Owner
keys are retained and cloud switches stay OFF. If stock startup returns, block
Internet access at the router before its next normal boot.

For a boot/kernel failure, use [UART recovery](recovery.md): **3.3 V TTL, VCC
disconnected**. USB carries recovery files; UART provides console control.

Report the board/version, operation, final screen and relevant redacted log
excerpts to the maintainer. Do not post a complete backup, private keys, Wi-Fi
credentials, registration details or serial-bearing reports publicly.

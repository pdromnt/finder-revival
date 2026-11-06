# Experimental installer components

**Stock firmware must already be 2.8.4.366 F2.12 20210823**, with the exact GUI
hash in [compatibility](../docs/compatibility.md). Another build is unsupported;
the starter/installer refuses it rather than trying to upgrade it.

**Required USB drive: at least 16 GB, formatted as FAT32.** FAT32 is mandatory
for installation, backup and recovery; exFAT and NTFS are not supported. The
small payload size does not remove the need for backup/rollback space. See
[backup requirements](../docs/backup.md).

`flashforge_init.sh` is the RAM-only USB starter for the verified `0007` hook.
Without a one-shot request it does not install anything. Explicit CHECK, BACKUP,
INSTALL and ROLLBACK requests use the new RAM-only BusyBox init handoff. See
[USB-only maintenance](../docs/usb-maintenance.md); the generic ZIP is not armed. The owner adds their own ordinary
public key as `revival/authorized_keys` for key-only setup. Without a key or
existing SSH configuration, first SSH setup initializes root / `flashforge_revival`,
port 2222. Existing owner credentials/configuration are preserved. Generic packages
contain no private keys; `SSH-ACCESS.txt` records the chosen setup in the install report.

`maintenance-enter.sh` is executed as PID 1 from a one-shot `init=/bin/sh` boot.
It copies the target's own maintenance binaries/dependencies into capped RAM,
pivots away from eMMC root, and leaves both repair targets unmounted. No repair
is automatic. The small `sparse_stream.c` is read-only source-side transport.

`install-overlay.sh --apply` only operates from RAM with both ext4 targets
unmounted and clean offline checks. It verifies the stock ELF/geometry, saves
the target's own originals externally, installs named Revival files, turns off
the two cloud switches, preserves/generates per-target keys, changes the launcher
field, and installs the boot splash into both verified kernel files. It saves
both originals on USB and checks unmounted boot/root/data before completion.
It does not flash the motion controller or rewrite the bootloader/partition table.

`kernel-logo` is a freestanding ARMv5 converter built from original code and
unchanged zlib 1.3.1 sources. A fixed 512 KiB allocation arena and streamed USB
scratch files keep memory bounded. Input, artwork and output hashes are pinned;
the package contains project artwork, not a vendor kernel. `kernel-install.sh`
stages both files, verifies them, commits cached copy first and boot image last,
and implements matching two-file rollback using that installation's USB report.

During the explicit offline install, static touchscreen screens show checking,
package verification, external rollback backup, installation, verification and
completion. Failures distinguish checks before install writes from an incomplete
installation: the latter says **Do not reboot yet**. Detailed errors remain in
the console; `FAILED.txt` is saved when the USB report directory is available.
Success requires unmount, final offline check and a flushed `COMPLETE.txt` report.
There are no touch buttons, timer, invented percentage or automatic reboot.

`status-screen.sh` streams one 307,200-byte RGB565 frame directly from USB to
`/dev/fb0`, with geometry, depth, stride, pan and frame-size guards. It uses no
image decoder, resident display process or RAM frame cache. Missing/incompatible
display support falls back to the console and does not change install results.
Without a maintenance request, the USB starter returns to normal startup and
does not imply an installation happened. Explicit USB requests show maintenance
stages and hold for manual power cycling. Generate PC previews with
`python tools/build_install_screens.py`; the release builder packages raw frames
and their hashes automatically, outside the persistent overlay.

USB-only CHECK, packaged upgrade/reinstall and rollback passed on the development
printer, including normal power cycles and protected-file comparisons.
See [installation](../docs/installation.md),
[identity](../docs/identity.md) and [validation](../docs/validation.md).

Reports/rollback archives are private and belong on external media or the PC.
The installer does not provide an atomic whole-install power-loss transaction.
Retain UART recovery and do not interrupt writes. It deliberately refuses the
normal live root instead of attempting unsafe in-place fsck.

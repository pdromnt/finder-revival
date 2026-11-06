# Restore after a failure

Use the smallest restore that addresses the failure. All copies must come from
**this printer's own backup**. Never use another owner's root/data dump.

**Use a USB drive of at least 16 GB, formatted as FAT32.** FAT32 is mandatory
for the supported restore workflow; exFAT and NTFS are not supported. Keep
verified backups on the PC as well. See [backup](backup.md) for free-space needs.

| Failure | Recovery path | Evidence |
| --- | --- | --- |
| Theme/layout regression; Linux and SSH work | Theme fallback, retaining local-only/logging guards | Existing launcher behavior; not a complete uninstall |
| Overlay/startup failure; U-Boot and root readable | Offline stock-UI rescue below | Generic procedure not yet rehearsed end to end |
| Bad splash/kernel; U-Boot still works | RAM-boot original kernel, restore both kernel files | Demonstrated on the development board |
| ext4 corruption | Preserve current state, rehearse offline repair | Demonstrated on the development board |
| Lost partition table/root, failed eMMC or U-Boot | Board-specific raw recovery | No qualified whole-device restore procedure yet |

A full dump is valuable, but it is **not a guarantee of recovery**. The USB
maintenance scripts need readable binaries from root; a RAM-loaded kernel alone
does not supply a replacement root filesystem. If root is unreadable, this
particular maintenance path may be unavailable.

## Before changing anything

1. Stop printing/heating and preserve any useful RAM logs before restarting.
2. Verify the exact [board/profile](compatibility.md), your backup hashes and
   [3.3 V UART access](recovery.md). Keep originals unchanged on the PC.
3. Save the current failed state externally, including configuration and kernel
   files when readable. Do not overwrite your only good backup with failed state.
4. For offline work, enter [RAM maintenance](recovery.md). Confirm root/data are
   unmounted, and the USB stick is at `/usb`. Do not proceed after a failed pivot.
5. Run `e2fsck -fn` on the two unmounted ext4 partitions. If either fails, follow
   [storage repair](storage-repair.md) before file restoration. Do not format.

## Theme-only fallback

On an otherwise working installation, the launcher recognizes
`/PROGRAM/revival-ui/DISABLED`. Creating that empty marker selects stock
presentation on the **next controlled application start**, retaining the logging
and update guards. Removing it enables the theme on a later start.

This does not recover an invalid stock ELF, missing guard library, damaged
filesystem or bad kernel. Do not kill/relaunch the GUI during a print. A plain
vendor executable launched outside the Revival wrapper loses these guards.

## USB-only managed-file rollback

New USB installs save inventories, checksums and a device binding under
`revival/reports/usb-*/install/`. The explicit ROLLBACK request in
[USB-only maintenance](usb-maintenance.md) restores the prior managed state,
preserves owner keys and the current root password, and keeps cloud switches OFF. It refuses edited managed
files and another printer’s archive. The physical upgrade/rollback round trip and
following normal boots passed on the development printer. See [validation](validation.md).

Current installation reports also contain `kernel-boot-before.img` and
`kernel-cache-before.img`; rollback verifies and restores both named kernel files.
Keep the entire report, including its checksum inventories, together. The
`rollback-private.tar` alone contains overlay files, not the kernel backups.

The older stock-UI rescue below is separate and requires an archive whose
startup configuration actually selects stock.

## Offline stock-UI rescue

The installer writes a private archive to the USB under
`revival/reports/install-*/rollback-private.tar`, with `rollback.sha256` beside it.
Keep those reports on the PC too. A first-install archive contains the original
stock startup configuration; an upgrade archive may instead select Revival.

Verify the archive on the PC using its recorded SHA256. The checksum file may
contain the printer's original absolute path: compare its hash with the copied
archive, rather than assuming `sha256sum -c` works after moving it. Inspect
`tar -tf` and choose the **pre-install stock** archive for stock-UI rescue.
Only use an archive you created and have reviewed; it may contain private keys.

In RAM maintenance, extract only the startup configuration to RAM. Replace the
example report directory with the one you verified:

```sh
sh <<'SH'
set -eu
export PATH=/bin:/usr/sbin:/usr/bin:/sbin
REPORT=/usb/revival/reports/REPLACE_WITH_VERIFIED_INSTALL_DIRECTORY
[ -f "$REPORT/rollback-private.tar" ]
grep -q ' / tmpfs ' /proc/mounts
! grep -q 'mmcblk0p[23]\|/dev/root ' /proc/mounts
awk '$2=="/usb" && $3=="vfat" {ok=1} END {exit !ok}' /proc/mounts
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
WORK=/run/stock-rescue
[ ! -e "$WORK" ]; mkdir -p "$WORK/saved" "$WORK/root"
tar -xf "$REPORT/rollback-private.tar" -C "$WORK/saved" PROGRAM/ffstartup.cfg
grep -q '^AppName = ./software/%VERSION%/finder-plus-arm$' "$WORK/saved/PROGRAM/ffstartup.cfg"
mount -t ext4 -o ro /dev/mmcblk0p2 "$WORK/root"
trap 'cd /; sync; umount "$WORK/root"' EXIT
[ "$(sha256sum "$WORK/root/PROGRAM/software/2.8.4/finder-plus-arm" | awk '{print $1}')" = aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375 ]
SAVE=/usb/stock-rescue-$(date +%Y%m%d-%H%M%S)-$$
mkdir "$SAVE"
cp "$WORK/root/PROGRAM/ffstartup.cfg" "$SAVE/ffstartup.cfg.before-rescue"
mount -o remount,rw "$WORK/root"
# Keep the files for diagnosis; names outside S??* are not started at boot.
DISABLED="$WORK/root/etc/init.d/revival-rescue-disabled"
mkdir -p "$DISABLED"
for name in S02revival-logs S03revival-local-only S50revival-sshd S51revival-web S52revival-camera; do
    [ ! -e "$DISABLED/$name" ] || { echo 'STOP: previous rescue exists'; exit 1; }
done
for name in S02revival-logs S03revival-local-only S50revival-sshd S51revival-web S52revival-camera; do
    [ ! -e "$WORK/root/etc/init.d/$name" ] || mv "$WORK/root/etc/init.d/$name" "$DISABLED/$name"
done
cp "$WORK/saved/PROGRAM/ffstartup.cfg" "$WORK/root/PROGRAM/ffstartup.cfg.rescue"
chmod 644 "$WORK/root/PROGRAM/ffstartup.cfg.rescue"
mv "$WORK/root/PROGRAM/ffstartup.cfg.rescue" "$WORK/root/PROGRAM/ffstartup.cfg"
cmp "$WORK/saved/PROGRAM/ffstartup.cfg" "$WORK/root/PROGRAM/ffstartup.cfg"
cd /; sync; umount "$WORK/root"
trap - EXIT
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
echo 'Stock-UI rescue prepared; review output before normal boot.'
SH
```

Stop on any error; do not assume a partially completed operation rolled itself
back. The script does not reboot or touch `/data`, calibration, credentials or
the stock executable. Cloud switches remain OFF. Revival files/keys are retained
for diagnosis; this is a stock-startup rescue, **not an exact uninstall**.

Disabling these startup scripts also removes Revival SSH/dashboard, logging
mitigation and WAN blocking on the next boot. Stock update behavior returns.
**Keep the printer's WAN blocked at the router, or disconnect networking before
normal boot.** Repair/reinstall Revival before restoring its ordinary access.
Do not reconnect solely to see whether the stock updater works.

For an exact previous-overlay rollback, review the archive against the install
manifest on the PC first. Restore its saved files and remove only files proven
to have been introduced by that installation. Archives contain existing paths
only: extracting one over a newer installation leaves newly added files behind.
Restore startup selection last; keep cloud switches OFF unless cloud restoration
is explicitly intended. The USB managed-file procedure above automates this scope;
its completed round trip restored the development printer's previous Revival setup.

## Restore an unbootable kernel

First [RAM-boot your verified original kernel](recovery.md). A successful USB
boot is the prerequisite, not proof that internal storage was repaired.

On this exact profile, restore the same verified stock bytes to **both**:

| Partition | File |
| --- | --- |
| `/dev/mmcblk0p1`, FAT boot | `/uImage` relative to that partition |
| `/dev/mmcblk0p2`, ext4 root | `/PROGRAM/kernel/1.0.366/uImage-finder-367-20190814` |

Expected original SHA256:
`ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87`.

1. Use RAM maintenance with root/data unmounted and clean offline checks.
2. Mount boot/root read-only at separate temporary directories. Verify source
   device, destination mount and exact paths; save both current kernel files to
   USB/PC. Require more than 4 MiB free on each target for a staged copy.
3. Remount those two mounts read-write. Copy the verified original from USB to
   a new sibling staging file in each destination. Compare each staging SHA256
   with the expected stock hash, then `sync`.
4. Rename the staged cached kernel over its cached destination, then the staged
   boot kernel over `uImage`. `sync` again. Verify both destination hashes.
5. Unmount both. Require the root offline check to exit 0. Do not reset while
   copying/renaming or if either verified kernel copy is incomplete.
6. Reset to the saved normal U-Boot environment, with the USB recovery material
   still available. Confirm it boots from internal storage and the screen works.

This file-level kernel restoration was demonstrated on the development board.
It is not atomic across the two files or resilient to every power failure.
No eMMC boot partition, partition table or motion controller needs rewriting for
the splash-kernel failure we encountered. Do not use historical per-printer
scripts from ignored research as generic recovery utilities.

## Full-device restoration is not qualified

The acquisition script captures the user area plus `boot0`/`boot1`; it does not
fully capture RPMB, eMMC configuration registers or SoC fuses. A live dump may
also contain inconsistent filesystems. We have **not** rehearsed restoring all
those images to this printer, and do not provide a blind whole-device `dd` recipe.

Before a raw recovery, an operator must establish a working external/RAM boot
or hardware-programmer route, reconstruct/verify image geometry on the PC,
inspect partition/filesystem consistency on copies, and review exactly which
regions need restoration. Preserve this printer's identity and boot configuration.
Do not clear `force_ro`, write boot0/boot1 or replace U-Boot merely because those
files exist in the backup. Physical storage failure may require hardware repair.

## After any restore

Check normal boot, touchscreen/touch input, original serial/registration and
calibration, stored print-file hashes, network configuration and the intended
cloud/update policy. If Revival was restored, verify SSH host identity, dashboard,
camera and bounded logging too. Finish with supervised heating/motion/printing
as a separate physical check. Record exactly what was restored and keep all
pre-repair backups private.

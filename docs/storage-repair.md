# Broken logs and offline repair

The development printer had real ext4 metadata corruption, not merely garbled
log text. This does not establish that every Finder has the problem.

## What was found

* The original pre-modification dump recorded 7 root and 70 data error events;
  before repair they were 7 and 72.
* `/data/log` contained embedded NUL/control bytes, a wrong `.` inode, and two
  names referencing an inode whose stored link count was only one.
* Root also had bitmap/reference defects and damaged recent Revival paths.
* Routine TCP/menu activity was logged as `<CRITICAL>`. That noise amplified
  flash writes but did not explain the actual metadata corruption.
* eMMC reported normal reserves and the first 0–10% wear bucket. Other power,
  controller, RAM, flash or kernel faults remain possible.
* The original trigger remains unknown; repair does not prove it cannot recur.

Counters count events, not broken files. Kernel messages can repeat stored
history after a delay. Clocks may start in 1970, so dates can be unreliable.

## Installation must stop on damage

Stop if `ls` reports missing entries it just listed, names contain invalid
bytes, ext4/I/O errors appear, or offline checks fail. Zero recorded errors do
not replace a full consistency check. Do not blindly delete `/data/log`: bad
link counts can free an inode another name still references.

## Procedure

Use a **16 GB minimum USB drive formatted as FAT32** for this workflow.
FAT32 is mandatory; exFAT and NTFS are not supported. Allow extra free space
for the current-state snapshots and preserved evidence before starting.

1. Preserve this printer's current backups externally and establish UART recovery.
2. Boot into RAM and genuinely **unmount root and data**. A read-only mount is
   still mounted. See [recovery](recovery.md).
3. Verify `/proc/mounts`, geometry and target identity. In RAM, point `/etc/mtab`
   at `/proc/mounts` for old e2fsprogs mount detection.
4. Capture consistent snapshots to the PC. Repair separate copies, inspect all
   changes and verify protected files before touching device metadata.
5. On unmounted device partitions, run interactive `e2fsck -f` and review fixes.
   The tested recovery used `-fy` only after rehearsing its exact snapshot;
   unattended yes-to-all is not a generic first step.
6. Independently require `e2fsck -fn` exit 0 on both filesystems.
7. Preserve `lost+found` externally; restore damaged known assets from verified
   originals. Check identity and print-file hashes before cleanup.
8. Remove obsolete logs/cores only now. Unmount, check again, then boot normally
   and validate UI/dashboard/networking.

Baseline-only commands, **after verifying both targets are unmounted**:

```sh
cat /proc/mounts
e2fsck -f /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p2
e2fsck -f /dev/mmcblk0p3
e2fsck -fn /dev/mmcblk0p3
```

Do not copy partition numbers onto an unidentified board. Even `e2fsck -n`
on a mounted filesystem can give invalid results, as documented in the
[e2fsck manual](https://man7.org/linux/man-pages/man8/e2fsck.8.html). Formatting
or flashing somebody else's full image is not a substitute for repair review.

## Logging mitigation

The pinned Qt handler is replaced by `logging/revival_log.c`. Six exact routine
critical messages are dropped; other severities remain. The stock persistent
open is redirected to `/dev/null`, including its double-slash path. The ARM
adapter exports legacy `open`, avoiding the toolchain's `open64` redirection.

UI diagnostics rotate over two 64 KiB files. The supervisor logs in RAM at error
level. `S02revival-logs` caps the **whole vendor log mount at 256 KiB/32 inodes**.
Nothing is copied back to flash at shutdown. Save useful RAM evidence before reboot.

Post-repair checks passed on both unmounted filesystems. Normal boot reported
zero stored errors and an empty persistent log directory under repeated queries.
This is measured acceptance, not a lifetime guarantee.

A subsequent operator-performed cold boot also retained zero root/data error
counters and no storage-error kernel messages. A bounded raw inspection of the
previously damaged log directory found valid records, correct `.`/`..` entries,
link count 2 and no files. The capped RAM log mount remained active. These are
online recurrence observations; they do not replace an unmounted filesystem
check or establish that every other metadata structure is consistent.

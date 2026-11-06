# Back up your own printer

Backups are private, never release assets. Store them externally or on a PC,
not in the small root partition. Keep the original kernel, stock GUI, startup
configuration, per-printer settings and a verified recovery route.

Read [restore after failure](restore.md) before relying on a dump. File-level
kernel recovery was demonstrated; a whole-eMMC restore has not been qualified.
Keep an independently accessible copy of the original kernel and startup
configuration, not only those files buried inside a multi-gigabyte image.

## Full acquisition

**Use a USB drive of at least 16 GB, formatted as FAT32. FAT32 is mandatory;
do not use exFAT or NTFS.** Keep at least 8 GiB free for one full acquisition;
additional backups, snapshots and rollback archives need additional space.

Read the eMMC user area plus `mmcblk0boot0/boot1`. The observed user area is
7,818,182,656 bytes. FAT32 needs chunks below 4 GiB; the original acquisition
used 1 GiB chunks. Record actual geometry, complete lengths and SHA256 hashes.
Do not infer another machine's capacity or change eMMC boot/RPMB/force_ro settings.

The generic USB package includes `revival/backup-emmc.sh`, based on the read-only
acquisition script used during development. From the printer's shell, with the
external FAT stick mounted at the path you actually verified:

```sh
sh /media/revival/backup-emmc.sh
```

Use `/usb/revival/backup-emmc.sh` inside the supplied RAM maintenance environment.
The script validates the known profile's geometry and external free space,
creates a new `FFINDER-BACKUP-*` directory on the stick, reads eight user-area
chunks plus both boot partitions, and hashes all ten files. It does not write
the source block devices. Keep the stick connected until `FINISHED.txt` exists,
then independently verify `SHA256SUMS` on the PC. It can take a long time.
Incomplete/failed files are not recovery images. Optional old framebuffer
progress pictures are not required by the reader.

After copying the entire backup directory to a PC, verify every entry in
`SHA256SUMS`. On Windows, open PowerShell in that copied directory:

```powershell
$entries = @(Get-Content -LiteralPath .\SHA256SUMS)
if ($entries.Count -ne 10) { throw 'Expected 10 image hashes' }
foreach ($entry in $entries) {
    if ($entry -notmatch '^([0-9a-fA-F]{64})\s+\*?(mmcblk0(?:\.part0[0-7]|boot[01])\.img)$') {
        throw 'Unexpected backup manifest entry'
    }
    $expectedHash = $Matches[1]
    $imageName = $Matches[2]
    $actualHash = (Get-FileHash -LiteralPath $imageName -Algorithm SHA256).Hash
    if ($actualHash -ne $expectedHash) { throw "Checksum failed: $imageName" }
}
$imageNames = @($entries | ForEach-Object { ($_ -split '\s+')[-1].TrimStart('*') })
if (@($imageNames | Sort-Object -Unique).Count -ne 10) { throw 'Duplicate image entries' }
if (-not (Test-Path -LiteralPath .\FINISHED.txt)) { throw 'Backup did not finish' }
'All 10 backup images verified.'
```

The seven first user-area chunks must each be 1,073,741,824 bytes; the eighth
must be 301,989,888 bytes, totaling 7,818,182,656 bytes. Boot images are each
4,194,304 bytes. `FORMAT.txt` records this supported geometry. Keep two verified
external copies and the acquisition report; compare the actual byte lengths too.

A dump from mounted filesystems is **not atomic**. Transfer hashes prove file
integrity, not filesystem consistency. Label it as live acquisition and rehearse
restoration/repair only on copies.

## Consistent filesystem snapshots

Boot into RAM, unmount root/data, then copy them externally. `e2image -ra`
preserves metadata and allocated file contents as sparse images; unused blocks
are omitted. It is not a forensic whole-device clone. Retain the raw dump too.

The tested data filesystem ends 2 MiB before its partition ends. Validate the
superblock block count rather than assuming both lengths must match.

Do not send gigabytes of zero holes through UART or hold a snapshot in printer
RAM. `installer/sparse_stream.c` adapts e2image output to bounded PC streaming.
Old e2image flushes buffered diagnostics to stdout: image frames need a separate
FD. Source devices remain read-only. See [storage repair](storage-repair.md).

Keep original snapshots read-only and repair separate copies. Verify original
GX files and identity/configuration bytes. Do not delete old logs/cores before
preserving damaged directory metadata; dumps and logs may contain secrets.

# UART and RAM recovery

Recovery was demonstrated on the supported board after a bad splash kernel
failed to boot. That is evidence for this board and bootloader, not every Finder.

**Recovery USB requirement: 16 GB minimum, formatted as FAT32.** FAT32 is
mandatory; exFAT and NTFS are not supported by this recovery workflow. UART
provides console control; USB carries the kernel, maintenance files and backups.

## Wiring

Use **3.3 V TTL**, not RS-232 or 5 V signalling. Verify the actual header with
a meter and PCB labels. The observed header has `G RX TX V` labels.

| Raspberry Pi Pico debugprobe | Printer header |
| --- | --- |
| GP4 / UART TX | RX |
| GP5 / UART RX | TX |
| GND | G |
| No connection | V |

Power the Pico via USB; do not connect printer VCC. Official Pico debugprobe
v2.3.1 worked at **115200 8N1**, no hardware flow control. USB CDC **DTR must be
true** for that bridge firmware to poll UART. Serial-port names vary per PC.

During U-Boot's “Hit any key” countdown, send **Escape, 0x1B**. The vendor code
specifically compares Escape; spaces/Ctrl+C did not stop it on this board.

## RAM-test an original kernel

Place your own verified original kernel on the FAT32 USB as `ffstock.img`:

```text
usb reset
usb storage
fatls usb 0:1 /
fatload usb 0:1 0x7fc0 ffstock.img
iminfo 0x7fc0
crc32 0x7fc0 30d1b8
bootm 0x7fc0
```

For the exact known original, require 3,199,416 bytes, valid image checksum,
SHA256 from [compatibility](compatibility.md), and full-image CRC32 `142bff83`.
Addresses, controller numbering and lengths must not be guessed on other boards.
These commands load RAM; they do not themselves repair files or flash eMMC.

The original failure preserved gzip/uImage CRCs but overwrote the ARM
decompressor's fixed-position inflated-length word. Correct repacking keeps
the gzip trailer at its original address using internal **FNAME header padding**.
Never append padding after the gzip trailer. CRC checks alone were insufficient.

## UART maintenance without mounting data

For ordinary installation/rollback use [USB-only maintenance](usb-maintenance.md).
This console route remains available for recovery and manual work.

Use volatile boot arguments, **without `saveenv`**, to boot the known kernel
with a shell as PID 1 and root read-only:

```text
setenv bootargs noinitrd root=/dev/mmcblk0p2 rootfstype=ext4 ro init=/bin/sh rootwait console=ttyS0,115200n8
run bootcmd
```

`run bootcmd` uses the internal kernel. If that kernel is the failure, keep the
volatile `bootargs` above but instead repeat the verified `usb reset`, `fatload`,
`iminfo` and `bootm` sequence with `ffstock.img`. The USB kernel still needs a
readable internal root filesystem for this maintenance method.

Mount proc/sys/dev as needed, mount the external FAT stick at `/media`, and
**exec** its maintenance script so the old shell does not keep root busy:

```sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
# /dev is normally already devtmpfs on this kernel; inspect before mounting.
mount -t vfat /dev/sda1 /media
exec /bin/sh /media/revival/maintenance-enter.sh
```

The script copies this printer's BusyBox/e2fsck and loader dependencies into a
capped RAM root, moves virtual/USB mounts, pivots root, then unmounts the old
root. It does not automatically repair anything. Confirm no eMMC root/data
mount remains before following [storage repair](storage-repair.md).

Stock BusyBox lacks `tar -z`: use `gzip -dc archive.tgz | tar -xf -`. A script
that is merely a child of PID 1 can leave old root busy. Stop on a failed pivot
or unmount; do not force fsck against a still-mounted filesystem.

Return to normal boot arguments (`init=/linuxrc`, original profile command
line) after maintenance. A reset reloads saved environment; no persistent
bootloader change is required. Never reboot during metadata writes.

For persistent restoration after a successful RAM boot, follow
[restore after failure](restore.md). RAM boot alone does not replace the bad
internal kernel.

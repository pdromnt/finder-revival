#!/bin/sh
# Read eMMC devices; write images and logs only to the mounted USB stick.
umask 077
WORKDIR=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
case "$WORKDIR" in /mnt|/mnt/*|/media/*|/usb|/usb/*) ;; *) exit 1 ;; esac
USB_MOUNT=$(awk -v p="$WORKDIR" '$3 == "vfat" && (p == $2 || index(p, $2 "/") == 1) {print $2; exit}' /proc/mounts)
[ -n "$USB_MOUNT" ] || exit 1
BASE="$WORKDIR/FFINDER-BACKUP-$(date +%Y%m%d-%H%M%S)-$$"
OUT="$BASE"
RUN=0
while [ -e "$OUT" ]; do
    RUN=$((RUN + 1))
    [ "$RUN" -le 100 ] || exit 1
    OUT="$BASE-run$RUN"
done
mkdir "$OUT" || exit 1
exec > "$OUT/report.txt" 2>&1

show_screen() {
    # Use the same 320x480, RGB565 framebuffer layout as the factory updater.
    # A display failure must never prevent or interrupt the backup.
    SCREEN="$WORKDIR/backup-screens/$1.raw"
    if [ -c /dev/fb0 ] && [ -f "$SCREEN" ]; then
        cat "$SCREEN" > /dev/fb0 2>/dev/null || :
    fi
}
fail() {
    echo "FAILED: $*"
    printf 'FAILED: %s\nSee report.txt. Do not use incomplete images for restoration.\n' "$*" > "$OUT/FAILED.txt"
    show_screen failed
    sync
    exit 1
}
progress() {
    echo "$1"
    printf '%s\n' "$1" > "$OUT/PROGRESS.txt"
    [ -z "$2" ] || show_screen "$2"
}

show_screen starting
echo "Finder eMMC backup: no block-device writes, no firmware installation."
echo "Live image: mounted filesystems may change during acquisition."
echo "Restoration has not been tested. RPMB and SoC fuses are not captured."
uname -a
id
cat /proc/partitions
cat /proc/mounts
cat /proc/cmdline
[ "$(id -u)" = 0 ] || fail "Root execution required"
for TOOL in dd sha256sum ls awk; do
    command -v "$TOOL" >/dev/null 2>&1 || fail "Missing tool: $TOOL"
done
SECTORS=$(cat /sys/block/mmcblk0/size) || fail "Cannot read eMMC size"
[ "$SECTORS" = 15269888 ] || fail "Unexpected main eMMC size: $SECTORS sectors"
for DEVICE in mmcblk0 mmcblk0boot0 mmcblk0boot1; do
    [ -b "/dev/$DEVICE" ] || fail "Missing /dev/$DEVICE"
done
for DEVICE in mmcblk0boot0 mmcblk0boot1; do
    SIZE=$(cat "/sys/block/$DEVICE/size") || fail "Cannot read $DEVICE size"
    [ "$SIZE" = 8192 ] || fail "Unexpected $DEVICE size: $SIZE sectors"
done
FREE_KB=$(df -k "$WORKDIR" | awk 'END {print $4}')
case "$FREE_KB" in ''|*[!0-9]*) fail "Cannot determine free USB space" ;; esac
NEEDED_KB=$((SECTORS / 2 + 8192 + 131072))
[ "$FREE_KB" -ge "$NEEDED_KB" ] || fail "Need at least $NEEDED_KB KB free; found $FREE_KB KB"

cat > "$OUT/FORMAT.txt" <<'EOF'
Main eMMC user area: concatenate mmcblk0.part00.img through part07.img in order.
Expected combined size: 7818182656 bytes (15269888 sectors of 512 bytes).
First seven chunks: 1073741824 bytes each. Final chunk: 301989888 bytes.
Boot partitions: mmcblk0boot0.img and mmcblk0boot1.img, 4194304 bytes each.
SHA256SUMS covers each individual image file.
This is a live acquisition of mounted filesystems, not an atomic snapshot.
RPMB, eMMC configuration registers and SoC fuses are not fully backed up.
Do not restore any of these files until a separate recovery method is verified.
EOF

mkdir "$OUT/metadata" || fail "Cannot create metadata directory"
for FIELD in cid csd name type manfid oemid date fwrev hwrev serial; do
    SOURCE="/sys/block/mmcblk0/device/$FIELD"
    if [ -r "$SOURCE" ]; then
        cat "$SOURCE" > "$OUT/metadata/$FIELD.txt" || fail "Cannot save $FIELD"
    fi
done
for DEVICE in mmcblk0boot0 mmcblk0boot1; do
    if [ -r "/sys/block/$DEVICE/force_ro" ]; then
        cat "/sys/block/$DEVICE/force_ro" > "$OUT/metadata/$DEVICE-force_ro.txt"
    fi
done

verify_file() {
    ACTUAL=$(LC_ALL=C ls -ln "$OUT/$1" | awk '{print $5}')
    [ "$ACTUAL" = "$2" ] || fail "Wrong size for $1: $ACTUAL, expected $2"
    progress "Hashing $1" "hash-$1"
    (cd "$OUT" && sha256sum "$1") >> "$OUT/SHA256SUMS" || fail "Cannot hash $1"
    sync
}

for DEVICE in mmcblk0boot0 mmcblk0boot1; do
    progress "Reading $DEVICE (4 MiB)" "copy-$DEVICE.img"
    dd if="/dev/$DEVICE" of="$OUT/$DEVICE.img" bs=1048576 count=4 || fail "Read error: $DEVICE"
    verify_file "$DEVICE.img" 4194304
done

OFFSET=0
PART=0
TOTAL_MIB=7456
while [ "$OFFSET" -lt "$TOTAL_MIB" ]; do
    COUNT=$((TOTAL_MIB - OFFSET))
    [ "$COUNT" -le 1024 ] || COUNT=1024
    FILE=$(printf 'mmcblk0.part%02d.img' "$PART")
    progress "Reading main eMMC chunk $((PART + 1))/8 ($COUNT MiB)" "copy-$FILE"
    dd if=/dev/mmcblk0 of="$OUT/$FILE" bs=1048576 skip="$OFFSET" count="$COUNT" || fail "Read error: $FILE"
    verify_file "$FILE" "$((COUNT * 1048576))"
    OFFSET=$((OFFSET + COUNT))
    PART=$((PART + 1))
done
progress "Finished all 10 image files; returning to normal printer startup" complete
printf 'Backup finished: 8 main-area chunks and 2 boot-partition images.\nCheck SHA256SUMS on the computer before relying on these files.\nLive image; restoration remains untested.\n' > "$OUT/FINISHED.txt"
sync
sleep 10
exit 0

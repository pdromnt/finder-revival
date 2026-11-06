#!/bin/sh
# Two named kernel files only. Originals and compression workspace stay on USB.
KERNEL_STOCK=ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87
KERNEL_REVIVAL=651ded763ab91ea7b8584ba8d7f9cfb80b53a1c57835d8c62b855b893efdbdb0
KERNEL_CACHE=PROGRAM/kernel/1.0.366/uImage-finder-367-20190814
KERNEL_SIZE=3199416
BOOT_MOUNTED=0

kernel_hash() { sha256sum "$1" | awk '{print $1}'; }
kernel_verify() {
    [ -f "$1" ] && [ ! -L "$1" ] || return 1
    [ "$(wc -c < "$1")" = "$KERNEL_SIZE" ] || return 1
    [ "$(kernel_hash "$1")" = "$2" ]
}
kernel_known() {
    case "$1" in "$KERNEL_STOCK"|"$KERNEL_REVIVAL") ;; *) return 1 ;; esac
}
check_boot_fat() {
    ! grep -q 'mmcblk0p1 ' /proc/mounts || return 1
    fat_result=0
    LC_ALL=C fsck.fat -n /dev/mmcblk0p1 > "$1" 2>&1 || fat_result=$?
    cat "$1"
    [ "$fat_result" != 0 ] || return 0
    # This old baseline can retain just its dirty flag. Accept ONLY that exact
    # diagnostic, never structural repairs. -n leaves every byte unchanged.
    [ "$fat_result" = 1 ] || return 1
    grep -qx '0x25: Dirty bit is set. Fs was not properly unmounted and some data may be corrupt.' "$1" || return 1
    awk '
      /^CP437: Invalid argument$/ {next}
      /^fsck.fat 3.0.28 \(2015-05-16\)$/ {next}
      /^0x25: Dirty bit is set. Fs was not properly unmounted and some data may be corrupt\.$/ {next}
      /^ Automatically removing dirty bit\.$/ {next}
      /^Leaving filesystem unchanged\.$/ {unchanged=1;next}
      /^\/dev\/mmcblk0p1: [0-9]+ files, [0-9]+\/[0-9]+ clusters$/ {summary=1;next}
      {bad=1}
      END {exit bad || !unchanged || !summary}' "$1" || return 1
    echo 'Boot FAT has only the known dirty flag; no repair was performed.'
}
kernel_open() {
    rest=$KERNEL_CACHE
    prefix=$ROOT
    while :; do
        part=${rest%%/*}; prefix="$prefix/$part"
        [ ! -L "$prefix" ] || return 1
        case "$rest" in */*) rest=${rest#*/} ;; *) break ;; esac
    done
    KERNEL_BEFORE=$(kernel_hash "$ROOT/$KERNEL_CACHE") || return 1
    kernel_known "$KERNEL_BEFORE" || return 1
    kernel_verify "$ROOT/$KERNEL_CACHE" "$KERNEL_BEFORE" || return 1
    check_boot_fat "$REPORT/boot-fat-before.txt" || return 1
    BOOT="$STAGE/boot"
    mkdir "$BOOT" || return 1
    mount -t vfat -o ro /dev/mmcblk0p1 "$BOOT" || return 1
    BOOT_MOUNTED=1
    kernel_verify "$BOOT/uImage" "$KERNEL_BEFORE" || return 1
    [ ! -e "$BOOT/uImage.revival-new" ] && [ ! -L "$BOOT/uImage.revival-new" ] || return 1
    [ ! -e "$ROOT/$KERNEL_CACHE.revival-new" ] && [ ! -L "$ROOT/$KERNEL_CACHE.revival-new" ] || return 1
    [ "$(df -k "$BOOT" | awk 'END {print $4}')" -gt 4096 ] || return 1
}
kernel_save() {
    cp "$BOOT/uImage" "$REPORT/kernel-boot-before.img" || return 1
    cp "$ROOT/$KERNEL_CACHE" "$REPORT/kernel-cache-before.img" || return 1
    kernel_verify "$REPORT/kernel-boot-before.img" "$KERNEL_BEFORE" || return 1
    kernel_verify "$REPORT/kernel-cache-before.img" "$KERNEL_BEFORE" || return 1
    printf '%s\n' "$KERNEL_BEFORE" > "$REPORT/kernel-before.sha256" || return 1
}
kernel_prepare_install() {
    kernel_open || return 1
    kernel_save || return 1
    "$BASE/kernel-logo" "$REPORT/kernel-cache-before.img" "$BASE/kernel-logo.art" \
        "$REPORT/kernel-prepared.img" "$REPORT/kernel-work" || return 1
    KERNEL_SOURCE="$REPORT/kernel-prepared.img"
    KERNEL_AFTER=$KERNEL_REVIVAL
    kernel_verify "$KERNEL_SOURCE" "$KERNEL_AFTER" || return 1
    printf '%s\n' "$KERNEL_AFTER" > "$REPORT/kernel-after.sha256" || return 1
}
kernel_prepare_rollback() {
    KERNEL_ROLLBACK=0
    if [ ! -f "$SAVED/kernel-before.sha256" ]; then
        # Reports from the earlier overlay-only format did not change kernels.
        echo 'Legacy overlay rollback: kernel files left unchanged.'
        return 0
    fi
    [ "$(wc -c < "$SAVED/kernel-before.sha256")" = 65 ] || return 1
    [ "$(wc -c < "$SAVED/kernel-after.sha256")" = 65 ] || return 1
    KERNEL_AFTER=$(cat "$SAVED/kernel-before.sha256")
    kernel_known "$KERNEL_AFTER" || return 1
    [ "$(cat "$SAVED/kernel-after.sha256")" = "$KERNEL_REVIVAL" ] || return 1
    kernel_verify "$SAVED/kernel-boot-before.img" "$KERNEL_AFTER" || return 1
    kernel_verify "$SAVED/kernel-cache-before.img" "$KERNEL_AFTER" || return 1
    kernel_open || return 1
    [ "$KERNEL_BEFORE" = "$KERNEL_REVIVAL" ] || return 1
    kernel_save || return 1
    KERNEL_SOURCE="$SAVED/kernel-cache-before.img"
    KERNEL_ROLLBACK=1
}
kernel_commit() {
    kernel_verify "$KERNEL_SOURCE" "$KERNEL_AFTER" || return 1
    mount -o remount,rw "$BOOT" || return 1
    # Stage and verify BOTH files before either live name is replaced.
    cp -p "$ROOT/$KERNEL_CACHE" "$ROOT/$KERNEL_CACHE.revival-new" || return 1
    cp "$KERNEL_SOURCE" "$ROOT/$KERNEL_CACHE.revival-new" || return 1
    cp "$KERNEL_SOURCE" "$BOOT/uImage.revival-new" || return 1
    kernel_verify "$ROOT/$KERNEL_CACHE.revival-new" "$KERNEL_AFTER" || return 1
    kernel_verify "$BOOT/uImage.revival-new" "$KERNEL_AFTER" || return 1
    sync || return 1
    # Cached update copy first; the bootloader's actual image is replaced last.
    mv "$ROOT/$KERNEL_CACHE.revival-new" "$ROOT/$KERNEL_CACHE" || return 1
    sync || return 1
    kernel_verify "$ROOT/$KERNEL_CACHE" "$KERNEL_AFTER" || return 1
    mv "$BOOT/uImage.revival-new" "$BOOT/uImage" || return 1
    sync || return 1
    kernel_verify "$BOOT/uImage" "$KERNEL_AFTER" || return 1
    kernel_verify "$ROOT/$KERNEL_CACHE" "$KERNEL_AFTER"
}
kernel_close() {
    [ "$BOOT_MOUNTED" = 1 ] || return 0
    sync || return 1
    umount "$BOOT" || return 1
    BOOT_MOUNTED=0
}

#!/bin/sh
# RAM PID 1: operate only after root/data are unmounted. Never auto-repair/reboot.
set -eu
export PATH=/bin:/usr/sbin:/usr/bin:/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib
BASE=/usb/revival
. /status-screen.sh
{ IFS= read -r MODE; IFS= read -r RUN; IFS= read -r TARGET; } < /usb-state
REPORT="$BASE/reports/$RUN"
hold_failure() {
    result=$?
    trap - 0 HUP INT TERM
    printf 'USB maintenance stopped (exit %s). Review operation.log before restarting.\n' "$result" >&2
    printf 'Maintenance stopped: %s\nReview operation.log before restarting.\n' "$result" > "$REPORT/FAILED.txt" || :
    sync || :
    case "$MODE" in CHECK|BACKUP) revival_screen "$BASE" usb-stopped ;; *) revival_screen "$BASE" failed-writes ;; esac
    while :; do /bin/sleep 60; done
}
trap hold_failure 0
[ "$$" = 1 ]
umount /oldroot
! grep -q 'mmcblk0p[123]\|/dev/root ' /proc/mounts
busybox ln -s /proc/mounts /etc/mtab
mount -t tmpfs -o size=4m,mode=0700 tmpfs /run
exec > "$REPORT/operation.log" 2>&1
cat /proc/mounts
df -k / /run
revival_screen "$BASE" checking
. "$BASE/kernel-install.sh"
check_boot_fat "$REPORT/boot-fat-check.txt"
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
export REVIVAL_RUN_ID="$RUN"
case "$MODE" in
    CHECK)
        printf 'RAM handoff passed. Boot/root/data unmounted; offline checks passed. No installation changes.\n' > "$REPORT/CHECK-PASSED.txt"
        final=usb-check-done
        ;;
    BACKUP)
        revival_screen "$BASE" usb-backup
        /bin/sh "$BASE/backup-emmc.sh"
        final=usb-backup-done
        ;;
    INSTALL)
        /bin/sh "$BASE/install-overlay.sh" --apply
        [ -f "$REPORT/install/COMPLETE.txt" ]
        printf '%s\n' "$RUN" > "$BASE/LATEST-INSTALL.txt.new"
        mv "$BASE/LATEST-INSTALL.txt.new" "$BASE/LATEST-INSTALL.txt"
        final=usb-install-done
        ;;
    ROLLBACK)
        /bin/sh "$BASE/rollback-overlay.sh" --apply "$TARGET"
        [ -f "$REPORT/rollback/RESTORED.txt" ]
        final=usb-rollback-done
        ;;
    *) exit 1 ;;
esac
! grep -q 'mmcblk0p[123]\|/dev/root ' /proc/mounts
mkdir -p /tmp/status-screens
cp "$BASE/status-screens/$final.raw" "/tmp/status-screens/$final.raw"
[ "$(wc -c < "/tmp/status-screens/$final.raw")" = 307200 ]
printf 'Operation complete. Root/data unmounted; preparing safe USB removal.\n' > "$REPORT/FINISHED.txt"
sync
cd /
# Close all USB log descriptors before unmounting. This script itself is in RAM.
exec >/dev/console 2>&1
umount /usb
trap - 0
revival_screen /tmp "$final"
echo 'USB safely unmounted. Remove USB, then power-cycle the printer.'
while :; do /bin/sleep 60; done

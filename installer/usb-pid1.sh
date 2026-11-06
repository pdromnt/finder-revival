#!/bin/sh
# Executed by BusyBox init as PID 1 using the copied RAM loader and shell.
set -eu
RAM=/media
export PATH=/media/bin:/media/usr/sbin:/media/usr/bin:/bin:/sbin:/usr/bin:/usr/sbin
export LD_LIBRARY_PATH=/media/lib:/media/usr/lib
unset LD_PRELOAD
PHASE=pid1-entry
bb() {
    "$RAM/lib/ld-linux.so.3" --library-path "$RAM/lib:$RAM/usr/lib" "$RAM/bin/busybox" "$@"
}
hold() {
    result=$?
    trap - 0 HUP INT TERM
    echo "STOP: transition failed at $PHASE (exit $result). No overlay operation was started." >&2
    for helper in /media/status-screen.sh /status-screen.sh; do
        if [ -f "$helper" ]; then . "$helper"; break; fi
    done
    for location in /media/usb/revival /mnt/revival /usb/revival; do
        if [ -d "$location/reports" ]; then revival_screen "$location" usb-stopped || :; break; fi
    done
    while :; do
        if [ -x /media/bin/busybox ]; then bb sleep 60; else /bin/busybox sleep 60; fi
    done
}
# PID 1 must hold on every failure, including an unavailable USB log.
trap hold 0
exec > /mnt/revival/reports/pid1-early.txt 2>&1
set -x
echo "RAM PID1 entry started: pid=$$"
[ "$$" = 1 ]
{ IFS= read -r MODE; IFS= read -r RUN; IFS= read -r TARGET; } < "$RAM/usb-state"
echo "Request run: $RUN"
PHASE=stop-userspace
# Do not use kill(-1): the vendor's raw SD/eMMC workers have PPID 1 and
# unblocked signals. A nonzero userspace address space distinguishes processes
# we can stop from kernel workers, including those misleadingly named swapper.
stop_userspace() {
    signal=$1
    for process in /proc/[0-9]*; do
        pid=${process##*/}
        [ "$pid" = 1 ] && continue
        [ -r "$process/statm" ] || continue
        IFS=' ' read -r pages rest < "$process/statm" || continue
        case "$pages" in ''|*[!0-9]*|0) continue ;; esac
        echo "Stopping userspace PID $pid: $signal"
        kill -"$signal" "$pid" 2>/dev/null || :
    done
}
bb cat /proc/partitions
stop_userspace TERM
bb sleep 2
stop_userspace KILL
bb sleep 1
# Record the storage driver before any mount movement/offline operation.
bb cat /proc/partitions
bb cat /sys/block/mmcblk0/size
bb dmesg | bb tail -80
PHASE=verify-storage-read
bb dd if=/dev/mmcblk0p2 of="$RAM/root-read-probe" bs=1024 skip=1 count=1
[ "$(bb wc -c < "$RAM/root-read-probe")" = 1024 ]
PHASE=restore-inittab
bb umount /etc/inittab
[ "$(bb sha256sum /etc/inittab | bb awk '{print $1}')" = 5c0ae0ca9039f2426406b644bfe3841f0ca9563b3d2df570327cbd82dda91cc4 ]
PHASE=move-usb
bb mount --move /mnt "$RAM/usb"
exec > "$RAM/usb/revival/reports/$RUN/transition.log" 2>&1
PHASE=unmount-data
if bb awk '$2=="/data" {found=1} END {exit !found}' /proc/mounts; then bb umount /data; fi
if bb awk '$2=="/run/revival-logs" {found=1} END {exit !found}' /proc/mounts; then bb umount /run/revival-logs; fi
for path in /run /tmp; do
    if bb awk -v p="$path" '$2==p {found=1} END {exit !found}' /proc/mounts; then bb umount "$path"; fi
done
bb sync
PHASE=read-only-root
bb mount -o remount,ro /
bb grep -q '^/dev/root / ext4 ro,' /proc/mounts
! bb grep -q 'mmcblk0p3 ' /proc/mounts
PHASE=move-virtual-filesystems
bb mount --move /proc "$RAM/proc"
bb mount --move /sys "$RAM/sys"
bb mount --move /dev "$RAM/dev"
cd "$RAM"
PHASE=pivot-root
bb pivot_root . oldroot
# Relative RAM paths work on either side of pivot_root's process-root adjustment.
exec ./lib/ld-linux.so.3 --library-path ./lib:./usr/lib ./bin/busybox chroot . /bin/sh /init-usb-job

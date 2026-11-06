#!/bin/sh
# Invoke with exec from init=/bin/sh. No automatic fsck or eMMC writes.
set -eu
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
[ "$$" = 1 ] || { echo 'Must replace PID 1 shell with exec.' >&2; exit 1; }
SOURCE=$(CDPATH= cd "$(dirname "$0")" && pwd)
USB=$(awk -v p="$SOURCE" '$3=="vfat" && (p==$2 || index(p,$2 "/")==1) {print $2;exit}' /proc/mounts)
[ -n "$USB" ]
! grep -q 'mmcblk0p3 ' /proc/mounts
grep -q '^/dev/root / ext4 ro,' /proc/mounts
mount -t tmpfs -o size=16m,mode=0755 tmpfs /mnt
RAM=/mnt
mkdir -p "$RAM/bin" "$RAM/sbin" "$RAM/lib" "$RAM/usr/lib" "$RAM/usr/bin" "$RAM/usr/sbin" "$RAM/etc" "$RAM/dev" "$RAM/proc" "$RAM/sys" "$RAM/usb" "$RAM/oldroot" "$RAM/run" "$RAM/tmp"
for binary in /bin/busybox /sbin/fsck.fat /usr/sbin/e2fsck /usr/sbin/e2image /usr/bin/ssh-keygen; do
    cp -L "$binary" "$RAM$binary"
    /lib/ld-linux.so.3 --list "$binary" | awk '{for(i=1;i<=NF;i++) if($i ~ /^\//) print $i}' | while read -r dependency; do
        mkdir -p "$RAM$(dirname "$dependency")"
        cp -L "$dependency" "$RAM$dependency"
    done
done
cp -L /lib/ld-linux.so.3 "$RAM/lib/ld-linux.so.3"
for applet in $(busybox --list); do
    [ -e "$RAM/bin/$applet" ] || busybox ln -s /bin/busybox "$RAM/bin/$applet"
done
cp "$SOURCE/maintenance-ram.sh" "$RAM/init-ram"
mount --move /proc "$RAM/proc"
mount --move /sys "$RAM/sys"
mount --move /dev "$RAM/dev"
mount --move "$USB" "$RAM/usb"
cd "$RAM"
pivot_root . oldroot
exec /bin/chroot . /bin/sh /init-ram </dev/console >/dev/console 2>&1

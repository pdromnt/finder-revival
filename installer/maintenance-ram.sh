#!/bin/sh
export PATH=/bin:/usr/sbin:/usr/bin:/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib
umount /oldroot || { echo 'STOP: old root remains mounted'; exec /bin/sh; }
if grep -q 'mmcblk0p[23]\|/dev/root ' /proc/mounts; then
    echo 'STOP: a repair target is still mounted'; exec /bin/sh
fi
busybox ln -s /proc/mounts /etc/mtab
mount -t tmpfs -o size=4m,mode=0700 tmpfs /run
echo 'RAM maintenance ready. Root/data unmounted. No repair or install has run.'
cat /proc/mounts
while :; do /bin/sh -i; sleep 1; done

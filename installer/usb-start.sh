#!/bin/sh
# Boot-hook entry: transient init configuration, no kernel/bootloader/startup writes.
set -eu
umask 077
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
unset LD_PRELOAD
MEDIA=${1:-}
[ "$MEDIA" = /mnt ]
BASE="$MEDIA/revival"
. "$BASE/status-screen.sh"
REPORT=
RAM=/media
BOUND=0
RAM_MOUNTED=0
GUARD_ACTIVE=0
fail() {
    result=$?
    trap - 0 HUP INT TERM
    [ "$result" = 0 ] && exit 0
    echo "USB maintenance entry refused (exit $result). No overlay operation ran." >&2
    if [ "$GUARD_ACTIVE" = 1 ]; then
        if ! "$RAM/init-guard" --restore; then
            echo 'Cannot verify restored init instructions. Power-cycle; do not continue boot.' >&2
            revival_screen "$BASE" usb-stopped
            while :; do sleep 60; done
        fi
    fi
    if [ "$BOUND" = 1 ]; then
        umount /etc/inittab && kill -HUP 1 || :
    fi
    if [ -n "$REPORT" ]; then
        printf 'USB maintenance entry failed: %s\nNo overlay operation ran.\n' "$result" > "$REPORT/FAILED.txt" || :
        sync || :
    fi
    revival_screen "$BASE" failed
    # A failed entry may leave a volatile RAM mount. A normal power cycle clears it.
    exit "$result"
}
trap fail 0
trap 'exit 130' INT
trap 'exit 143' TERM
[ "$(id -u)" = 0 ]
[ "$(readlink /proc/1/exe)" = /bin/busybox ]
[ "$(sha256sum /bin/busybox | awk '{print $1}')" = 414b14705623e30b7b22f4ffaa686c12023424f97005a6402a1354fd988cde41 ]
[ "$(sha256sum /etc/inittab | awk '{print $1}')" = 5c0ae0ca9039f2426406b644bfe3841f0ca9563b3d2df570327cbd82dda91cc4 ]
[ "$(sha256sum /etc/init.d/rcS | awk '{print $1}')" = 5101ec596402f44354c3679ec9a7c2cf7e42e5644d7d097e19a4f5d361562c29 ]
[ "$(sha256sum /opt/auto_run.sh | awk '{print $1}')" = 9452b9739702bdf52b55e9aa3e031d9f3089d789b8e430d0cfce2186c06e862e ]
[ "$(sha256sum /PROGRAM/software/2.8.4/finder-plus-arm | awk '{print $1}')" = aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375 ]
[ "$(cat /sys/block/mmcblk0/size)" = 15269888 ]
[ "$(uname -r)" = 3.10.32+ ]
[ ! -e /proc/net/if_inet6 ] && [ ! -d /proc/sys/net/ipv6 ]
case "$(uname -m)" in armv5*) ;; *) exit 1 ;; esac
. "$BASE/overlay-common.sh"
ROOT=/
verify_stock
# Only the early stock boot hook is supported. Never shut down an active GUI/job.
! pidof finder-plus-arm ffstartup-arm >/dev/null
awk '$2=="/mnt" && $3=="vfat" {ok=1} END {exit !ok}' /proc/mounts
awk '$2=="/media" || $2=="/etc/inittab" {bad=1} END {exit bad}' /proc/mounts
awk '$2!="/" && $2!="/proc" && $2!="/sys" && $2!="/data" && $2!="/mnt" && $2!="/tmp" && $2!="/run" && $2!="/run/revival-logs" && $2!="/dev" && index($2,"/dev/")!=1 {bad=1} END {exit bad}' /proc/mounts
[ "$(df -k "$MEDIA" | awk 'END {print $4}')" -gt 65536 ]
[ "$(wc -c < "$BASE/REQUEST.txt")" -lt 160 ]
request=$(tr -d '\r' < "$BASE/REQUEST.txt")
target=
case "$request" in
    CHECK|BACKUP|INSTALL|ROLLBACK) mode=$request ;;
    'ROLLBACK usb-'*) mode=ROLLBACK; target=${request#ROLLBACK } ;;
    *) echo 'Unknown request. Use CHECK, BACKUP, INSTALL or ROLLBACK [usb-report-id].' >&2; exit 1 ;;
esac
if [ "$mode" = ROLLBACK ]; then
    [ -n "$target" ] || target=$(cat "$BASE/LATEST-INSTALL.txt")
    case "$target" in usb-*) ;; *) exit 1 ;; esac
    case "$target" in *[!a-z0-9-]*) exit 1 ;; esac
    [ -f "$BASE/reports/$target/install/COMPLETE.txt" ]
fi
(cd "$BASE" && sha256sum -c SHA256SUMS)
# Read-only live instruction probe; refuse any different PID 1 layout.
"$BASE/init-guard" --probe
nonce=$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')
RUN="usb-$(date +%Y%m%d-%H%M%S)-$nonce"
REPORT="$BASE/reports/$RUN"
mkdir "$REPORT"
printf '%s\n' "$mode" > "$REPORT/mode.txt"
{ uname -a; sha256sum /bin/busybox /etc/inittab /etc/init.d/rcS /opt/auto_run.sh /PROGRAM/software/2.8.4/finder-plus-arm; } > "$REPORT/profile.txt"
revival_screen "$BASE" usb-enter
mount -t tmpfs -o size=16m,mode=0755 tmpfs "$RAM"
RAM_MOUNTED=1
mkdir -p "$RAM/bin" "$RAM/lib" "$RAM/usr/lib" "$RAM/usr/bin" "$RAM/usr/sbin" "$RAM/etc" "$RAM/dev" "$RAM/proc" "$RAM/sys" "$RAM/usb" "$RAM/oldroot" "$RAM/run" "$RAM/tmp"
for binary in /bin/busybox /usr/sbin/e2fsck /usr/sbin/e2image /usr/bin/ssh-keygen; do
    cp -L "$binary" "$RAM$binary"
    /lib/ld-linux.so.3 --list "$binary" | awk '{for(i=1;i<=NF;i++) if($i ~ /^\//) print $i}' > "$RAM/dependencies"
    while read -r dependency; do
        mkdir -p "$RAM$(dirname "$dependency")"
        cp -L "$dependency" "$RAM$dependency"
    done < "$RAM/dependencies"
done
cp -L /lib/ld-linux.so.3 "$RAM/lib/ld-linux.so.3"
cp -L /lib/libnss_files.so.2 "$RAM/lib/libnss_files.so.2"
for name in passwd group nsswitch.conf; do cp "/etc/$name" "$RAM/etc/$name"; done
for applet in $(busybox --list); do
    [ -e "$RAM/bin/$applet" ] || busybox ln -s busybox "$RAM/bin/$applet"
done
cp "$BASE/usb-pid1.sh" "$RAM/init-entry"
cp "$BASE/usb-job.sh" "$RAM/init-usb-job"
cp "$BASE/status-screen.sh" "$RAM/status-screen.sh"
cp "$BASE/init-guard" "$RAM/init-guard"
chmod 755 "$RAM/init-guard"
printf '%s\n%s\n%s\n' "$mode" "$RUN" "$target" > "$RAM/usb-state"
cat > "$RAM/init-ack" <<'SH'
#!/bin/sh
echo "$$" > /media/ack.pid
while :; do sleep 60; done
SH
printf '::respawn:/bin/sh /media/init-ack\n::restart:/media/lib/ld-linux.so.3 --library-path /media/lib:/media/usr/lib /media/bin/busybox sh /media/init-entry\n' > "$RAM/inittab"
# Prove the copied loader, libraries and multicall binary execute before stopping init.
"$RAM/lib/ld-linux.so.3" --library-path "$RAM/lib:$RAM/usr/lib" "$RAM/bin/busybox" true
[ "$(du -sk "$RAM" | awk '{print $1}')" -lt 12288 ]
{ du -sk "$RAM"; head -n 4 /proc/meminfo; } > "$REPORT/ram-preparation.txt"
# Consume once, before the init handoff. A forgotten stick must not reinstall.
mv "$BASE/REQUEST.txt" "$REPORT/request.txt"
sync
mount --bind "$RAM/inittab" /etc/inittab
BOUND=1
kill -HUP 1
# BusyBox defers HUP until rcS returns. A respawn action proves the reload ran.
tries=0
while [ ! -s "$RAM/ack.pid" ]; do
    sleep 1
    tries=$((tries+1))
    [ "$tries" -lt 45 ]
done
printf 'Init reload acknowledged; entering RAM maintenance.\n' > "$REPORT/HANDOFF.txt"
sync
# This kernel's raw eMMC worker is vulnerable to init's kill(-1) broadcasts.
# Skip those two calls only in current PID 1 RAM. New PID 1 stops userspace by PID.
GUARD_ACTIVE=1
"$RAM/init-guard" --apply
printf 'Guarded two volatile init instructions; disk executable unchanged.\n' >> "$REPORT/HANDOFF.txt"
trap - 0 HUP INT TERM
kill -QUIT 1
# The replacement PID 1 terminates this process and its stock parent individually.
while :; do sleep 60; done

#!/bin/sh
# Experimental offline-only overlay. Never formats, flashes controllers or clones identity.
set -eu
umask 077
case "${1:-}" in --apply) ;; *) echo 'Usage: sh install-overlay.sh --apply (RAM maintenance only)'; exit 2 ;; esac
BASE=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=
REPORT=
MOUNTED=0
WRITES_STARTED=0
PHASE=checking
# A missing display helper must not prevent console-only recovery/install.
revival_screen() { :; }
[ ! -f "$BASE/status-screen.sh" ] || . "$BASE/status-screen.sh"
status() { PHASE=$1; echo "Installation stage: $PHASE"; revival_screen "$BASE" "$PHASE"; }
finish() {
    cd /
    sync || return 1
    umount "$ROOT" || return 1
    MOUNTED=0
}
install_exit() {
    result=$1
    trap - 0 HUP INT TERM
    if [ "$MOUNTED" = 1 ]; then
        if ! finish; then
            echo 'STOP: storage cleanup failed; do not reboot.' >&2
            result=1
        fi
    fi
    if [ "$result" != 0 ]; then
        failed=failed
        [ "$WRITES_STARTED" = 0 ] || failed=failed-writes
        echo "Installation failed in $PHASE (exit $result); install writes started: $WRITES_STARTED" >&2
        if [ -n "$REPORT" ]; then
            rm -f "$REPORT/COMPLETE.txt" || :
            printf 'Failed stage: %s\nExit: %s\nInstall writes started: %s\nSee console and restore guide.\n' \
                "$PHASE" "$result" "$WRITES_STARTED" > "$REPORT/FAILED.txt" || :
            sync || :
        fi
        revival_screen "$BASE" "$failed"
    fi
    exit "$result"
}
trap 'install_exit $?' 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
[ "$(id -u)" = 0 ]
grep -q ' / tmpfs ' /proc/mounts
! grep -q 'mmcblk0p[23]\|/dev/root ' /proc/mounts
[ "$(cat /sys/block/mmcblk0/size)" = 15269888 ]
case "$(uname -m)" in armv5*) ;; *) echo 'Unsupported CPU'; exit 1 ;; esac
[ "$(uname -r)" = 3.10.32+ ]
if [ -e /proc/net/if_inet6 ] || [ -d /proc/sys/net/ipv6 ]; then echo 'Unsupported IPv6 kernel'; exit 1; fi
for tool in e2fsck tar sha256sum mount umount awk sed od tr; do command -v "$tool" >/dev/null; done
status checking
if [ -s "$BASE/authorized_keys" ]; then
    awk 'NF {if ($1!="ssh-ed25519" && $1!="ssh-rsa" && $1!~ /^ecdsa-sha2-/) exit 1; count++} END {if(count!=1) exit 1}' "$BASE/authorized_keys"
    ssh-keygen -l -f "$BASE/authorized_keys" >/dev/null
fi
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
status package
cd "$BASE"; sha256sum -c SHA256SUMS
awk -v p="$BASE" '$3=="vfat" && (p==$2 || index(p,$2 "/")==1) {ok=1} END {exit !ok}' /proc/mounts
STAGE=/run/revival-install
[ ! -e "$STAGE" ]; mkdir -p "$STAGE/root" "$STAGE/payload"
tar -xf "$BASE/payload.tar" -C "$STAGE/payload"
ROOT="$STAGE/root"
mount -t ext4 -o ro /dev/mmcblk0p2 "$ROOT"
MOUNTED=1
[ "$(sha256sum "$ROOT/PROGRAM/software/2.8.4/finder-plus-arm" | awk '{print $1}')" = aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375 ] || {
    echo 'Unsupported stock executable: requires Finder 2.8.4.366 F2.12 20210823 and the documented SHA256.' >&2
    exit 1
}
KERNEL=$(sha256sum "$ROOT/PROGRAM/kernel/1.0.366/uImage-finder-367-20190814" | awk '{print $1}')
case "$KERNEL" in
 ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87|651ded763ab91ea7b8584ba8d7f9cfb80b53a1c57835d8c62b855b893efdbdb0) ;;
 *) echo 'Unsupported cached kernel'; exit 1 ;;
esac
[ "$(df -k "$ROOT" | awk 'END {print $4}')" -gt 8192 ]
status backup
REPORT="$BASE/reports/install-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$REPORT"
EXISTING="$STAGE/existing.txt"
: > "$EXISTING"
for path in PROGRAM/ffstartup.cfg PROGRAM/revival-ui PROGRAM/revival-web PROGRAM/revival-logging PROGRAM/revival-network PROGRAM/software/2.8.4/finder-revival.sh etc/revival etc/revival-web etc/init.d/S02revival-logs etc/init.d/S03revival-local-only etc/init.d/S50revival-sshd etc/init.d/S51revival-web etc/init.d/S52revival-camera opt/flashforge/cloud-ff opt/flashforge/cloud-mymini; do
    [ ! -e "$ROOT/$path" ] || printf '%s\n' "$path" >> "$EXISTING"
done
tar -cf "$REPORT/rollback-private.tar" -C "$ROOT" -T "$EXISTING"
sha256sum "$REPORT/rollback-private.tar" > "$REPORT/rollback.sha256"
cp "$ROOT/PROGRAM/ffstartup.cfg" "$STAGE/ffstartup.cfg"
grep -q '^AppName = ./software/%VERSION%/finder-\(plus-arm\|revival.sh\)$' "$STAGE/ffstartup.cfg"
sed -e 's|^AppName = ./software/%VERSION%/finder-plus-arm$|AppName = ./software/%VERSION%/finder-revival.sh|' \
    -e 's|^LogDir = /data/log$|LogDir = /run/revival-logs|' \
    -e 's|^LogLevel = 0$|LogLevel = 2|' -e 's|^LogConsole = true$|LogConsole = false|' \
    "$STAGE/ffstartup.cfg" > "$STAGE/ffstartup.cfg.new"
status installing
# Mark this before attempting remount; any later failure needs recovery review.
WRITES_STARTED=1
mount -o remount,rw "$ROOT"
tar -cf - -C "$STAGE/payload" . | tar -xf - -C "$ROOT"
# Only the target's cloud flags change; registration/account/calibration stay.
for name in cloud-ff cloud-mymini; do
    printf 'OFF\r\n' > "$ROOT/opt/flashforge/$name.new"
    chmod 644 "$ROOT/opt/flashforge/$name.new"
    mv "$ROOT/opt/flashforge/$name.new" "$ROOT/opt/flashforge/$name"
done
mkdir -p "$ROOT/etc/revival-web"
if [ ! -e "$ROOT/etc/revival-web/key" ]; then
    od -An -N32 -tx1 /dev/urandom | tr -d ' \n' > "$ROOT/etc/revival-web/key.new"
    [ "$(wc -c < "$ROOT/etc/revival-web/key.new")" = 64 ]
    grep -Eq '^[0-9a-f]{64}$' "$ROOT/etc/revival-web/key.new"
    chmod 600 "$ROOT/etc/revival-web/key.new"
    mv "$ROOT/etc/revival-web/key.new" "$ROOT/etc/revival-web/key"
fi
[ "$(wc -c < "$ROOT/etc/revival-web/key")" = 64 ]
grep -Eq '^[0-9a-f]{64}$' "$ROOT/etc/revival-web/key"
if [ -s "$BASE/authorized_keys" ]; then
    mkdir -p "$ROOT/etc/revival"; chmod 700 "$ROOT/etc/revival"
    [ -e "$ROOT/etc/revival/authorized_keys" ] || cp "$BASE/authorized_keys" "$ROOT/etc/revival/authorized_keys"
    [ -e "$ROOT/etc/revival/sshd_config" ] || cp "$BASE/sshd_config" "$ROOT/etc/revival/sshd_config"
    chmod 600 "$ROOT/etc/revival/authorized_keys" "$ROOT/etc/revival/sshd_config"
fi
cp "$STAGE/ffstartup.cfg.new" "$ROOT/PROGRAM/ffstartup.cfg.new"
chmod 644 "$ROOT/PROGRAM/ffstartup.cfg.new"
mv "$ROOT/PROGRAM/ffstartup.cfg.new" "$ROOT/PROGRAM/ffstartup.cfg"
status verifying
for directory in revival-ui revival-web revival-logging revival-network; do
    (cd "$ROOT/PROGRAM/$directory" && sha256sum -c SHA256SUMS)
done
finish
e2fsck -fn /dev/mmcblk0p2
printf 'Overlay installed; root unmounted and checked. No kernel was flashed.\n' > "$REPORT/COMPLETE.txt"
sync
status complete
echo 'Experimental overlay complete. Review report and normal-boot validation before use.'

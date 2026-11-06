#!/bin/sh
# Offline overlay and guarded splash installation. Never formats or flashes controllers.
set -eu
umask 077
case "${1:-}" in --apply) ;; *) echo 'Usage: sh install-overlay.sh --apply (RAM maintenance only)'; exit 2 ;; esac
BASE=$(CDPATH= cd "$(dirname "$0")" && pwd)
ROOT=
REPORT=
MOUNTED=0
BOOT_MOUNTED=0
WRITES_STARTED=0
PHASE=checking
# A missing display helper must not prevent console-only recovery/install.
revival_screen() { :; }
[ ! -f "$BASE/status-screen.sh" ] || . "$BASE/status-screen.sh"
status() { PHASE=$1; echo "Installation stage: $PHASE"; revival_screen "$BASE" "$PHASE"; }
finish() {
    cd /
    sync || return 1
    kernel_close || return 1
    if [ "$MOUNTED" = 1 ]; then
        umount "$ROOT" || return 1
        MOUNTED=0
    fi
}
install_exit() {
    result=$1
    trap - 0 HUP INT TERM
    if [ "$MOUNTED" = 1 ] || [ "$BOOT_MOUNTED" = 1 ]; then
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
. "$BASE/overlay-common.sh"
. "$BASE/ssh-password.sh"
. "$BASE/kernel-install.sh"
offline_profile
for tool in e2fsck fsck.fat tar sha256sum mount umount awk sed od tr; do command -v "$tool" >/dev/null; done
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
verify_stock
validate_paths "$BASE/managed-paths.txt"
validate_paths "$BASE/payload-paths.txt"
prepare_ssh_setup
[ "$(df -k "$ROOT" | awk 'END {print $4}')" -gt 8192 ]
status backup
if [ -n "${REVIVAL_RUN_ID:-}" ]; then
    case "$REVIVAL_RUN_ID" in usb-*) ;; *) exit 1 ;; esac
    case "$REVIVAL_RUN_ID" in *[!a-z0-9-]*) exit 1 ;; esac
    REPORT="$BASE/reports/$REVIVAL_RUN_ID/install"
else
    REPORT="$BASE/reports/install-$(date +%Y%m%d-%H%M%S)-$$"
fi
[ ! -e "$REPORT" ]
mkdir -p "$REPORT"
cp "$BASE/managed-paths.txt" "$REPORT/managed-paths.txt"
cp "$BASE/payload-paths.txt" "$REPORT/payload-paths.txt"
record_files "$REPORT/managed-paths.txt" "$REPORT/before"
backup_ssh_setup
kernel_prepare_install
sha256sum "$ROOT/opt/flashforge/serial-number" | awk '{print $1}' > "$REPORT/device-id.sha256"
tar -cf "$REPORT/rollback-private.tar" -C "$ROOT" -T "$REPORT/before.paths"
sha256sum "$REPORT/rollback-private.tar" > "$REPORT/rollback.sha256"
cp "$ROOT/PROGRAM/ffstartup.cfg" "$STAGE/ffstartup.cfg"
grep -q '^AppName = ./software/%VERSION%/finder-\(plus-arm\|revival.sh\)$' "$STAGE/ffstartup.cfg"
sed -e 's|^AppName = ./software/%VERSION%/finder-plus-arm$|AppName = ./software/%VERSION%/finder-revival.sh|' \
    -e 's|^LogDir = /data/log$|LogDir = /run/revival-logs|' \
    -e 's|^LogLevel = 0$|LogLevel = 2|' -e 's|^LogConsole = true$|LogConsole = false|' \
    "$STAGE/ffstartup.cfg" > "$STAGE/ffstartup.cfg.new"
status installing
WRITES_STARTED=1
mount -o remount,rw "$ROOT"
while IFS= read -r path; do
    safe_path "$path"
    [ -f "$STAGE/payload/$path" ] && [ ! -L "$STAGE/payload/$path" ]
    mkdir -p "$ROOT/$(dirname "$path")"
    cp -p "$STAGE/payload/$path" "$ROOT/$path"
done < "$BASE/payload-paths.txt"
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
install_ssh_setup
status verifying
for directory in revival-ui revival-web revival-logging revival-network; do
    (cd "$ROOT/PROGRAM/$directory" && sha256sum -c SHA256SUMS)
done
kernel_commit
cp "$STAGE/ffstartup.cfg.new" "$ROOT/PROGRAM/ffstartup.cfg.new"
chmod 644 "$ROOT/PROGRAM/ffstartup.cfg.new"
mv "$ROOT/PROGRAM/ffstartup.cfg.new" "$ROOT/PROGRAM/ffstartup.cfg"
record_files "$REPORT/managed-paths.txt" "$REPORT/after"
(cd "$REPORT"
 sha256sum rollback-private.tar before.paths before.sha256 after.paths after.sha256 managed-paths.txt payload-paths.txt device-id.sha256 SSH-ACCESS.txt > ROLLBACK-SHA256SUMS
 sha256sum kernel-boot-before.img kernel-cache-before.img kernel-before.sha256 kernel-after.sha256 >> ROLLBACK-SHA256SUMS
 if [ -f ssh-root-before.private ]; then sha256sum ssh-root-before.private SSH-ACCOUNT-SHA256SUMS >> ROLLBACK-SHA256SUMS; fi)
finish
check_boot_fat "$REPORT/boot-fat-after.txt"
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
printf 'Revival and boot splash installed; both kernel files verified. Boot/root/data unmounted and checked.\n' > "$REPORT/COMPLETE.txt"
sync
status complete
echo 'Experimental overlay complete. Review report and normal-boot validation before use.'

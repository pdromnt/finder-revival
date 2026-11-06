#!/bin/sh
# Offline rollback; preserve SSH credentials and cloud OFF policy.
set -eu
umask 077
[ "${1:-}" = --apply ] && [ -n "${2:-}" ]
BASE=$(CDPATH= cd "$(dirname "$0")" && pwd)
TARGET=$2
case "$TARGET" in usb-*) ;; *) exit 1 ;; esac
case "$TARGET" in *[!a-z0-9-]*) exit 1 ;; esac
. "$BASE/overlay-common.sh"
. "$BASE/status-screen.sh"
. "$BASE/kernel-install.sh"
offline_profile
(cd "$BASE" && sha256sum -c SHA256SUMS)
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
SAVED="$BASE/reports/$TARGET/install"
[ -f "$SAVED/COMPLETE.txt" ]
(cd "$SAVED" && sha256sum -c ROLLBACK-SHA256SUMS)
STAGE=/run/revival-rollback
[ ! -e "$STAGE" ]
mkdir -p "$STAGE/root" "$STAGE/saved"
ROOT="$STAGE/root"
MOUNTED=0
WRITES_STARTED=0
finish() { cd /; sync && kernel_close && umount "$ROOT" && MOUNTED=0; }
failed() {
    result=$?
    trap - 0 HUP INT TERM
    if [ "$MOUNTED" = 1 ]; then finish || result=1; fi
    if [ "$result" != 0 ]; then
        echo "Rollback failed (exit $result); writes started: $WRITES_STARTED" >&2
        if [ "$WRITES_STARTED" = 1 ]; then revival_screen "$BASE" failed-writes; else revival_screen "$BASE" failed; fi
    fi
    exit "$result"
}
trap failed 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
mount -t ext4 -o ro /dev/mmcblk0p2 "$ROOT"
MOUNTED=1
verify_stock
validate_paths "$SAVED/managed-paths.txt"
validate_paths "$SAVED/before.paths"
validate_paths "$SAVED/after.paths"
[ "$(cat "$SAVED/device-id.sha256")" = "$(sha256sum "$ROOT/opt/flashforge/serial-number" | awk '{print $1}')" ]
# Refuse to overwrite changes made since this installation, including edited keys.
(cd "$ROOT" && sha256sum -c "$SAVED/after.sha256")
tar -tf "$SAVED/rollback-private.tar" > "$STAGE/members"
sort "$STAGE/members" > "$STAGE/archive-sorted"
sort "$SAVED/before.paths" > "$STAGE/before-sorted"
cmp "$STAGE/archive-sorted" "$STAGE/before-sorted"
tar -tvf "$SAVED/rollback-private.tar" | awk 'substr($0,1,1)!="-" {bad=1} END {exit bad}'
tar -xf "$SAVED/rollback-private.tar" -C "$STAGE/saved"
(cd "$STAGE/saved" && sha256sum -c "$SAVED/before.sha256")
grep -qx 'PROGRAM/ffstartup.cfg' "$SAVED/before.paths"
[ -n "${REVIVAL_RUN_ID:-}" ]
case "$REVIVAL_RUN_ID" in usb-*) ;; *) exit 1 ;; esac
case "$REVIVAL_RUN_ID" in *[!a-z0-9-]*) exit 1 ;; esac
REPORT="$BASE/reports/$REVIVAL_RUN_ID/rollback"
[ ! -e "$REPORT" ]
mkdir "$REPORT"
record_files "$SAVED/managed-paths.txt" "$REPORT/before-rollback"
tar -cf "$REPORT/pre-rollback-private.tar" -C "$ROOT" -T "$REPORT/before-rollback.paths"
sha256sum "$REPORT/pre-rollback-private.tar" > "$REPORT/pre-rollback.sha256"
kernel_prepare_rollback
revival_screen "$BASE" rolling-back
WRITES_STARTED=1
mount -o remount,rw "$ROOT"
while IFS= read -r path; do
    case "$path" in PROGRAM/ffstartup.cfg|etc/revival/*|etc/revival-web/key|opt/flashforge/cloud-*) continue ;; esac
    safe_path "$path"
    if grep -qxF "$path" "$SAVED/before.paths"; then
        mkdir -p "$ROOT/$(dirname "$path")"
        cp -p "$STAGE/saved/$path" "$ROOT/$path"
    else
        # Only this named, hash-verified managed file may be removed. No tree deletion.
        rm -f "$ROOT/$path"
    fi
done < "$SAVED/managed-paths.txt"
for name in cloud-ff cloud-mymini; do printf 'OFF\r\n' > "$ROOT/opt/flashforge/$name"; done
if [ "$KERNEL_ROLLBACK" = 1 ]; then kernel_commit; fi
# Restore the previous startup selection last. Newly generated owner keys are retained.
cp -p "$STAGE/saved/PROGRAM/ffstartup.cfg" "$ROOT/PROGRAM/ffstartup.cfg.rollback"
mv "$ROOT/PROGRAM/ffstartup.cfg.rollback" "$ROOT/PROGRAM/ffstartup.cfg"
while IFS= read -r path; do
    case "$path" in etc/revival/*|etc/revival-web/key|opt/flashforge/cloud-*) continue ;; esac
    cmp "$STAGE/saved/$path" "$ROOT/$path"
done < "$SAVED/before.paths"
finish
check_boot_fat "$REPORT/boot-fat-after.txt"
e2fsck -fn /dev/mmcblk0p2
e2fsck -fn /dev/mmcblk0p3
printf 'Previous managed files/startup restored. SSH credentials retained; cloud switches OFF.\n' > "$REPORT/RESTORED.txt"
sync
trap - 0

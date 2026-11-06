#!/bin/sh
# Persistent, untimed launcher for the pinned stock GUI and reviewed adapter.
set -u
APP=/PROGRAM/software/2.8.4/finder-plus-arm
ROOT=/PROGRAM/revival-ui
STATE=/run/revival-ui-installed
EXPECTED=aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375
POLICY=/PROGRAM/revival-network/librevival_offline.so
LOGGER=/PROGRAM/revival-logging/librevival_log.so
[ -r "$POLICY" ] && [ -r "$LOGGER" ] || exit 126
mkdir -p "$STATE"
chmod 700 "$STATE"
stock() {
    unset REVIVAL_UI_CONFIG REVIVAL_UI_FLOW_CONFIG REVIVAL_UI_PROFILE
    LD_PRELOAD="$POLICY:$LOGGER${LD_PRELOAD:+:$LD_PRELOAD}" exec "$APP" "$@"
}
if [ "$(sha256sum "$APP" | awk '{print $1}')" != "$EXPECTED" ]; then
    echo 'Unsupported stock ELF; refusing fixed-address hooks.' > "$STATE/failed"
    exit 126
fi
if [ -e "$ROOT/DISABLED" ] || [ -e "$STATE/failed" ]; then stock "$@"; fi
if ! (cd "$ROOT" && sha256sum -c SHA256SUMS > "$STATE/integrity.log" 2>&1); then
    echo 'Integrity check failed; using stock UI.' > "$STATE/failed"
    stock "$@"
fi
ulimit -c 0
CHILD=
GUARD=
stop() {
    trap - INT TERM
    [ -z "$GUARD" ] || kill "$GUARD" 2>/dev/null || :
    [ -z "$CHILD" ] || kill -TERM "$CHILD" 2>/dev/null || :
    [ -z "$CHILD" ] || wait "$CHILD" 2>/dev/null || :
    exit 143
}
trap stop INT TERM
REVIVAL_UI_CONFIG="$ROOT/theme.conf" REVIVAL_UI_FLOW_CONFIG="$ROOT/flow.conf" \
    LD_PRELOAD="$POLICY:$LOGGER:$ROOT/librevival_ui.so${LD_PRELOAD:+:$LD_PRELOAD}" \
    "$APP" "$@" > "$STATE/ui.log" 2>&1 &
CHILD=$!
echo "$CHILD" > "$STATE/ui.pid"
(
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        kill -0 "$CHILD" 2>/dev/null || exit 0
        if grep -q '^REVIVAL_UI: applied ' "$STATE/ui.log"; then
            if [ "$(readlink "/proc/$CHILD/exe")" = "$APP" ] &&
               prlimit --pid "$CHILD" --core=0:0; then
                echo 'Post-startup core limit applied (0:0).' > "$STATE/core-guard.log"
                exit 0
            fi
            break
        fi
        sleep 1
    done
    echo 'Startup/core guard failed; using stock UI.' > "$STATE/failed"
    kill -TERM "$CHILD" 2>/dev/null || :
) &
GUARD=$!
if wait "$CHILD"; then RESULT=0; else RESULT=$?; fi
kill "$GUARD" 2>/dev/null || :
wait "$GUARD" 2>/dev/null || :
rm -f "$STATE/ui.pid"
trap - INT TERM
if [ "$RESULT" -ne 0 ]; then
    echo "Themed GUI exited $RESULT; using stock until reboot or manual re-enable." > "$STATE/failed"
    stock "$@"
fi
exit 0

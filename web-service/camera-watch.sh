#!/bin/sh
# Reuse the vendor's MJPEG encoder; never write frames to printer storage.
set -u
CAMERA=/PROGRAM/library/1.0.1/mjpg-streamer/bin/mjpg_streamer
CAMERA_LIB=/PROGRAM/library/1.0.1/mjpg-streamer/lib
CHILD=
stop() {
    trap - INT TERM
    if [ -n "$CHILD" ] && [ "$(readlink /proc/$CHILD/exe 2>/dev/null)" = "$CAMERA" ]; then
        kill -TERM "$CHILD" 2>/dev/null || :
        wait "$CHILD" 2>/dev/null || :
    fi
    exit 0
}
trap stop INT TERM
ulimit -c 0
while :; do
    if [ -n "$CHILD" ] && ! kill -0 "$CHILD" 2>/dev/null; then
        wait "$CHILD" 2>/dev/null || :
        CHILD=
    fi
    if [ -z "$CHILD" ] && [ -c /dev/video0 ] && [ -x "$CAMERA" ] &&
       [ -z "$(pidof mjpg_streamer)" ] &&
       ! awk '$2 ~ /:1F90$/ && $4 == "0A" {found=1} END {exit !found}' /proc/net/tcp; then
        LD_LIBRARY_PATH="$CAMERA_LIB" "$CAMERA" \
            -i "input_uvc.so -d /dev/video0 -r 640x480 -f 10" \
            -o "output_http.so -p 8080 -n" >/dev/null 2>&1 &
        CHILD=$!
    fi
    sleep 3 &
    wait $! || :
done

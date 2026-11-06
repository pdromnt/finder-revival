#!/bin/sh
# Source this helper. Display is best effort; it never decides install success.
# Frames stay on USB and stream directly to the supported 320x480 RGB565 panel.
revival_screen() (
    case "${2:-}" in checking|package|backup|installing|verifying|complete|failed|failed-writes) ;;
        *) exit 0 ;;
    esac
    frame="$1/status-screens/$2.raw"
    if [ -c /dev/fb0 ] &&
       [ "$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null)" = 320,480 ] &&
       [ "$(cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null)" = 16 ] &&
       [ "$(cat /sys/class/graphics/fb0/stride 2>/dev/null)" = 640 ] &&
       [ "$(cat /sys/class/graphics/fb0/pan 2>/dev/null)" = 0,0 ] &&
       [ -f "$frame" ] && [ "$(wc -c < "$frame")" = 307200 ]; then
        cat "$frame" > /dev/fb0 2>/dev/null && exit 0
    fi
    echo "Touchscreen status unavailable ($2); follow the console." >&2
    exit 0
)

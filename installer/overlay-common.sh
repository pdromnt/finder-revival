#!/bin/sh
# Shared file-level install/rollback guards. No block-device writes or repair.
offline_profile() {
    [ "$(id -u)" = 0 ] || return 1
    grep -q ' / tmpfs ' /proc/mounts || return 1
    ! grep -q 'mmcblk0p[23]\|/dev/root ' /proc/mounts || return 1
    [ "$(cat /sys/block/mmcblk0/size)" = 15269888 ] || return 1
    case "$(uname -m)" in armv5*) ;; *) return 1 ;; esac
    [ "$(uname -r)" = 3.10.32+ ] || return 1
    [ ! -e /proc/net/if_inet6 ] && [ ! -d /proc/sys/net/ipv6 ]
}
safe_path() {
    case "$1" in ''|/*|..|../*|*/../*|*/..|*[!A-Za-z0-9._/-]*) return 1 ;; esac
    case "$1" in
        PROGRAM/ffstartup.cfg|PROGRAM/software/2.8.4/finder-revival.sh|PROGRAM/revival-ui/*|PROGRAM/revival-web/*|PROGRAM/revival-logging/*|PROGRAM/revival-network/*|etc/revival/authorized_keys|etc/revival/sshd_config|etc/revival-web/key|etc/init.d/S02revival-logs|etc/init.d/S03revival-local-only|etc/init.d/S50revival-sshd|etc/init.d/S51revival-web|etc/init.d/S52revival-camera|opt/flashforge/cloud-ff|opt/flashforge/cloud-mymini) ;;
        *) return 1 ;;
    esac
    rest=$1
    prefix=$ROOT
    while :; do
        part=${rest%%/*}
        prefix="$prefix/$part"
        [ ! -L "$prefix" ] || return 1
        case "$rest" in */*) rest=${rest#*/} ;; *) break ;; esac
    done
    [ ! -e "$prefix" ] || [ -f "$prefix" ]
}
validate_paths() {
    [ -s "$1" ] || return 1
    [ "$(wc -l < "$1")" -le 128 ] || return 1
    [ "$(sort -u "$1" | wc -l)" = "$(wc -l < "$1")" ] || return 1
    while IFS= read -r path; do safe_path "$path" || return 1; done < "$1"
}
record_files() {
    list=$1
    output=$2
    : > "$output.paths"
    : > "$output.sha256"
    while IFS= read -r path; do
        safe_path "$path"
        if [ -f "$ROOT/$path" ]; then
            printf '%s\n' "$path" >> "$output.paths"
            (cd "$ROOT" && sha256sum "$path") >> "$output.sha256"
        fi
    done < "$list"
}
verify_stock() {
    [ "$(sha256sum "$ROOT/PROGRAM/software/2.8.4/finder-plus-arm" | awk '{print $1}')" = aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375 ] || {
        echo 'Unsupported stock executable: requires Finder 2.8.4.366 F2.12 20210823 and its documented SHA256.' >&2
        return 1
    }
    kernel=$(sha256sum "$ROOT/PROGRAM/kernel/1.0.366/uImage-finder-367-20190814" | awk '{print $1}')
    case "$kernel" in ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87|651ded763ab91ea7b8584ba8d7f9cfb80b53a1c57835d8c62b855b893efdbdb0) ;; *) return 1 ;; esac
}

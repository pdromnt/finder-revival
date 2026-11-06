#!/bin/sh
# Generic starter: RAM-only owner-key SSH; never auto-flash or auto-repair.
set -eu
umask 077
MEDIA=$(CDPATH= cd "$(dirname "$0")" && pwd)
case "$MEDIA" in /mnt|/media|/media/*) ;; *) exit 1 ;; esac
awk -v p="$MEDIA" '$2==p && $3=="vfat" {ok=1} END {exit !ok}' /proc/mounts
[ "$(id -u)" = 0 ]
mkdir -p "$MEDIA/revival/reports"
exec > "$MEDIA/revival/reports/bootstrap.txt" 2>&1
echo 'Finder Revival experimental USB bootstrap. No persistent install requested.'
uname -a
[ "$(sha256sum /PROGRAM/software/2.8.4/finder-plus-arm | awk '{print $1}')" = aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375 ] || {
    echo 'Unsupported stock executable: requires Finder 2.8.4.366 F2.12 20210823 and the documented SHA256.' >&2
    exit 1
}
echo 'Full installation and any repair require the offline maintenance guide.'
KEY="$MEDIA/revival/authorized_keys"
if [ ! -s "$KEY" ]; then
    echo 'No owner public key supplied; temporary SSH not started.'
    exit 0
fi
# Reject private keys/options; one ordinary owner public key is sufficient.
awk 'NF {if ($1!="ssh-ed25519" && $1!="ssh-rsa" && $1!~ /^ecdsa-sha2-/) exit 1; count++} END {if(count!=1) exit 1}' "$KEY"
ssh-keygen -l -f "$KEY" >/dev/null
RUNTIME=/run/revival-bootstrap
mkdir -p "$RUNTIME"; chmod 700 "$RUNTIME"
cp "$KEY" "$RUNTIME/authorized_keys"
ssh-keygen -q -t ed25519 -N '' -f "$RUNTIME/host_key"
sed -e "s|/etc/revival/ssh_host_ed25519_key|$RUNTIME/host_key|" \
    -e "s|/etc/revival/authorized_keys|$RUNTIME/authorized_keys|" \
    -e "s|/run/revival-sshd.pid|$RUNTIME/sshd.pid|" \
    "$MEDIA/revival/sshd_config" > "$RUNTIME/sshd_config"
/usr/sbin/sshd -t -f "$RUNTIME/sshd_config"
/usr/sbin/sshd -f "$RUNTIME/sshd_config" -E "$RUNTIME/sshd.log"
echo 'Temporary key-only SSH on 2222; host fingerprint:'
ssh-keygen -l -f "$RUNTIME/host_key.pub"
echo 'Runtime state is in RAM; returning to normal printer startup.'

#!/bin/sh
# Offline first-SSH setup only. Existing owner configuration/passwords are kept.
# ROOT, BASE, STAGE and REPORT are supplied by the guarded offline installer.
prepare_ssh_setup() {
    SSH_SETUP=existing
    [ ! -e "$ROOT/etc/revival/sshd_config" ] || return 0
    if [ -s "$BASE/authorized_keys" ] || [ -s "$ROOT/etc/revival/authorized_keys" ]; then
        SSH_SETUP=key
        return 0
    fi
    SSH_SETUP=password
    [ -f "$ROOT/etc/passwd" ] && [ ! -L "$ROOT/etc/passwd" ] || return 1
    [ -f "$ROOT/etc/shadow" ] && [ ! -L "$ROOT/etc/shadow" ] || return 1
    awk -F: '$1=="root" {n++; if(NF!=7 || $2!="x" || $3!="0") bad=1} END {exit n!=1 || bad}' "$ROOT/etc/passwd" || return 1
    awk -F: '$1=="root" {n++; if(NF!=9) bad=1} END {exit n!=1 || bad}' "$ROOT/etc/shadow" || return 1
    [ ! -e "$ROOT/etc/shadow.revival-new" ] && [ ! -L "$ROOT/etc/shadow.revival-new" ] || return 1
    [ "$(wc -l < "$BASE/default-root-password.hash")" = 1 ] || return 1
    grep -Eq '^\$6\$[A-Za-z0-9./]{1,16}\$[A-Za-z0-9./]{86}$' "$BASE/default-root-password.hash"
}

backup_ssh_setup() {
    [ "$SSH_SETUP" = password ] || return 0
    # Save only the target's original root record, not somebody else's account tree.
    awk -F: '$1=="root" {print}' "$ROOT/etc/shadow" > "$REPORT/ssh-root-before.private"
    (cd "$REPORT" && sha256sum ssh-root-before.private > SSH-ACCOUNT-SHA256SUMS)
}

install_ssh_setup() {
    mkdir -p "$ROOT/etc/revival"
    chmod 700 "$ROOT/etc/revival"
    if [ -s "$BASE/authorized_keys" ]; then
        [ -e "$ROOT/etc/revival/authorized_keys" ] || cp "$BASE/authorized_keys" "$ROOT/etc/revival/authorized_keys"
    fi
    case "$SSH_SETUP" in
        password)
            hash=$(cat "$BASE/default-root-password.hash")
            cp -p "$ROOT/etc/shadow" "$ROOT/etc/shadow.revival-new"
            awk -F: -v h="$hash" 'BEGIN {OFS=":"} $1=="root" {$2=h; if($3=="0") $3=1} {print}' \
                "$ROOT/etc/shadow" > "$ROOT/etc/shadow.revival-new"
            # Other accounts must retain their exact records.
            awk -F: '$1!="root" {print}' "$ROOT/etc/shadow" > "$STAGE/shadow-other-before"
            awk -F: '$1!="root" {print}' "$ROOT/etc/shadow.revival-new" > "$STAGE/shadow-other-after"
            cmp "$STAGE/shadow-other-before" "$STAGE/shadow-other-after"
            awk -F: -v h="$hash" '$1=="root" && $2==h {n++} END {exit n!=1}' "$ROOT/etc/shadow.revival-new"
            chmod 600 "$ROOT/etc/shadow.revival-new"
            mv "$ROOT/etc/shadow.revival-new" "$ROOT/etc/shadow"
            cp "$BASE/sshd_password_config" "$ROOT/etc/revival/sshd_config"
            printf 'SSH: root, port 2222\nInitial password: flashforge_revival\nChange it with passwd after login.\n' > "$REPORT/SSH-ACCESS.txt"
            ;;
        key)
            cp "$BASE/sshd_config" "$ROOT/etc/revival/sshd_config"
            printf 'SSH: root, port 2222; owner public key. Existing root password unchanged.\n' > "$REPORT/SSH-ACCESS.txt"
            ;;
        existing)
            printf 'Existing SSH configuration, owner keys and root password preserved.\n' > "$REPORT/SSH-ACCESS.txt"
            ;;
        *) return 1 ;;
    esac
    [ ! -e "$ROOT/etc/revival/authorized_keys" ] || chmod 600 "$ROOT/etc/revival/authorized_keys"
    chmod 600 "$ROOT/etc/revival/sshd_config"
}

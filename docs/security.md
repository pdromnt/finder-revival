# Security and local-only operation

Finder Revival deliberately deprecates vendor cloud and OTA/update integration.
The project targets an abandoned legacy firmware baseline and favors a local,
maintainable interface over continuing to trust remote vendor services.
No allegation of intentional vendor bricking is required for this policy.

## Separate layers

1. Hide Flashforge Cloud, Polar Cloud, Cloud queue and Check for updates controls.
2. Set both stored cloud switches OFF, preserving account/registration data and
   their original switch values externally. Do not factory-reset the printer.
3. Suppress the pinned stock cloud/update trigger entry points in process RAM,
   with exact instruction guards. A mismatch refuses the protected GUI instead
   of silently claiming protection. No stock ELF bytes are rewritten on disk.
4. Install IPv4 WAN **reject routes** covering 0/1 and 128/1, retaining loopback
   and more-specific connected LAN routes. These outrank later DHCP default
   routes. This kernel has no usable iptables/nftables; these are not firewall rules.
5. For the stronger boundary, put the printer on a router/VLAN policy that
   denies Internet egress and accepts only required local clients/services.

The new layers need their own validation; see [validation](validation.md) and
release notes. Do not infer network protection from a hidden menu or OFF switch.

## Limits of route-based blocking

The baseline exposes no IPv6 stack; the route service refuses an IPv6-enabled
kernel even if no interface currently has an IPv6 address. More-specific routes, a LAN proxy/DNS relay,
root software changing routes, or a malicious LAN are outside this protection.
DNS queries to a local router may still leave the printer even when subsequent
public-IP connections are rejected. Disabling stock triggers reduces those
requests but is not a claim of zero outbound packets.

Routed management networks need explicit, reviewed exceptions; ordinary on-link
LAN printing, DHCP, SSH, dashboard responses and loopback camera/control access
must be tested. Do not add an exception allowing general vendor OTA traffic.
Router isolation also protects against processes not covered by application hooks.

## Dashboard and SSH

HTTP is trusted-LAN only. `/api/session` intentionally returns the request key
to LAN clients: it is a browser write/CSRF guard, **not multi-user authentication**.
The service has same-origin/Host checks and guarded mutation identities, but no
TLS or accounts. The stock TCP and vendor camera services are also not hardened
public endpoints. No port forwarding, public reverse proxy or Internet exposure.

First-time SSH setup uses **root / `flashforge_revival`**, port **2222**. This is
a public project default: change it with `passwd` after login. Supplying your
own public key as `revival/authorized_keys` selects key-only setup instead.
Existing SSH configuration, owner keys and passwords are preserved on upgrades.
New installations generate a unique host key; no private keys are distributed.
Keep keys and backups private. Do not bypass host-key warnings without checking
the actual target. The old vendor kernel and userspace remain old after theming.

Physical USB is privileged: the stock startup hook executes scripts as root.
Only use trusted media. A release checksum detects accidental changes, not an
attacker replacing both a payload and its checksum. Verify release provenance.

# Local-only network policy

The supported kernel has no usable iptables/nftables and no active IPv6 stack.
`S03revival-local-only` installs two IPv4 **reject routes** covering 0/1 and
128/1. More-specific connected LAN routes and loopback remain reachable; a
later DHCP default route does not override these /1 routes.

This is not a firewall. More-specific routes, LAN proxies/DNS relays, software
running as root, or an untrusted LAN can bypass or fall outside its protection.
Use router/VLAN outbound isolation for the stronger boundary. A routed LAN
outside the connected subnet requires an explicit reviewed exception.

Cloud switches are disabled separately, cloud/update navigation hidden, and
the pinned stock update trigger routines suppressed in RAM. Hiding controls
alone does not stop networking. No claim about malicious vendor intent is needed
for this project to reject automatic vendor firmware changes.

# Installing with a coding assistant

Give the assistant this repository and the target printer's access details
privately. Tell it which operation you authorize: diagnostics, backup, repair,
overlay installation or rollback. An installation request covers its necessary
stated file/init changes and validation; it does not authorize printing, heating,
homing, factory reset, public exposure or publishing private backups.

The assistant should read `AGENTS.md`, compatibility, identity, security,
installation, backup and recovery guides before operating the printer.

It must establish facts from your machine:

* Exact board, CPU and storage layout; stock **2.8.4.366 F2.12 20210823** and
  matching GUI/kernel hashes. Stop on another build rather than bypassing guards.
* Your own serial/registration/calibration/network settings, preserved privately.
* The SSH setup choice: documented initial password or your own public key,
  preserving existing owner credentials, and a verified recovery path.
* A USB drive of at least 16 GB, formatted as FAT32, with enough free space for
  backups and rollback. FAT32 is mandatory; do not substitute exFAT or NTFS.
* Filesystem health with root/data unmounted for valid consistency checks.
* Current readiness and no ongoing print before any restart/maintenance action.

Do not give it somebody else's 8 GB image and ask it to force-flash it. Do not
have it run historical scripts from ignored research/session folders, bypass
hash guards, remove host-key checking, infer a UART pinout, or reuse another
owner's private credentials. The documented initial SSH password is public;
change it after login. Generic packages contain no private keys or owner identity.

If a check fails, it should preserve evidence externally and follow the recovery
or repair guide, not answer yes to unrelated destructive operations. Hardware
incompatibility requires a new audited profile. The integrated installer remains
experimental even when an assistant knows how to run shell commands.

Reports should separate fixture/build success from actual runtime and physical
printing tests. Redact all public evidence. Never publish backups, credentials,
registration data or camera frames without explicit owner authorization.

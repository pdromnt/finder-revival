# Contributing

Read [development](docs/development.md), [compatibility](docs/compatibility.md)
and [validation](docs/validation.md). Work from source and synthetic fixtures;
never commit vendor firmware, printer dumps, keys or identifying diagnostics.

Keep stock control/motion behavior intact unless a change explicitly targets it.
UI changes must preserve actual state, native capabilities and touch behavior.
Run relevant tests and distinguish build/fixture evidence from device evidence.

New hardware profiles need board identification, hash-verified binaries, ABI
analysis, backup/recovery, RAM testing and documented acceptance. A marketing
name or matching version string alone is insufficient.

Do not remove guards to make an unknown binary load. Do not run automatic GUI
rollback timers during printing, or fsck against mounted partitions. Seek
explicit authorization before transferring experimental code to someone's printer.

Original contributions use the repository's Unlicense; third-party code/assets
must retain their own provenance and license notices.

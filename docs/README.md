# Documentation

Start with [compatibility](compatibility.md), then [installation](installation.md)
and the [USB workflow](usb-maintenance.md). If something has already failed,
choose a recovery route in [restore after failure](restore.md).

## Install and use

| Guide | Purpose |
| --- | --- |
| [USB quickstart](quickstart.md) | First CHECK, verified backup, install and rollback for a new operator |
| [Compatibility](compatibility.md) | Supported Linux/eMMC board, stock build and binary hashes |
| [Installation](installation.md) | Requirements, package setup and installation overview |
| [USB-only maintenance](usb-maintenance.md) | CHECK, BACKUP, INSTALL and ROLLBACK without routine UART |
| [Assistant-guided installation](agent-installation.md) | Hand the job to your preferred coding assistant |
| [Usage](usage.md) | Local dashboard, Orca uploads, native previews and camera |
| [Orca presets and bed assets](../slicer/orca/README.md) | Guider 2s prerequisite, stable asset paths and optional AllMetal PLA |
| [Identity](identity.md) | What is preserved, generated and intentionally changed |
| [Security](security.md) | Cloud deprecation, update blocking and trusted-LAN limits |

## Back up, troubleshoot and recover

| Guide | Purpose |
| --- | --- |
| [Backup](backup.md) | Acquire your own storage, verify PC copies and keep original files accessible |
| [Troubleshooting](troubleshooting.md) | USB detection, menu fallback, camera, SSH and networking |
| [Restore after failure](restore.md) | Choose theme fallback, managed rollback, stock-UI rescue or kernel restoration |
| [UART recovery](recovery.md) | 3.3 V UART/Pico wiring, U-Boot, original-kernel RAM boot and manual maintenance |
| [Storage repair](storage-repair.md) | Preserve damaged state and repair only unmounted filesystems |
| [Factory Reset](factory-reset.md) | What the stock settings reset changes and what survives |

## Build and understand the project

| Guide | Purpose |
| --- | --- |
| [Architecture](architecture.md) | Processes, hooks, storage, memory and protocol |
| [Development](development.md) | Builds, tests, packaging and repository hygiene |
| [Runtime licensing/materials](../licenses/runtime/README.md) | Notices, matching source archives and verified relinking |
| [Reverse engineering](reverse-engineering.md) | Symbols, ABI traps and references |
| [Validation](validation.md) | Recorded hardware results and fixture evidence |

Historical local transcripts are deliberately unpublished: they contain device
identities, network details, credentials, firmware and crash dumps. These guides
preserve reproducible findings without redistributing those files.

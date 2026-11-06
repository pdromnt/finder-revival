# Installation and updates

## Status

**Prerequisite: stock 2.8.4.366 F2.12, build 20210823.** Verify About and the
[stock executable hash](compatibility.md) before using the starter/installer.
Prospective users on a different build must establish the matching stock
baseline first; these files do not upgrade arbitrary stock versions to it.

The packaged USB-only CHECK, upgrade/reinstallation and rollback passed on the
development Finder, including subsequent normal power cycles.
The community installer remains experimental.
Do not call a generated ZIP “safe to flash” based on build/fixture checks alone.

Release distribution is a small overlay, **not** an 8 GB image. It reuses the
target printer's own Linux, Qt, control software and identity. The custom splash
is optional and built locally from the target's hash-verified original kernel.

## USB bootstrap

**Required: a USB drive of at least 16 GB, formatted as FAT32.** FAT32 is
mandatory for this workflow; exFAT and NTFS are not supported. Follow the
[backup guide](backup.md) for free-space requirements.

The supported stock `/opt/auto_run.sh` checks FAT USB storage for model marker
`0007` (or `universal-arm`), then executes `flashforge_init.sh` as root before
starting the normal application. This was used successfully for diagnostics,
backup and temporary key-only SSH on the development printer.

This hook is not evidence that another Finder revision supports it. The
`moonglow/flashforge_firmware_tool` lists Finder among encrypted firmware formats,
but that does not prove its format applies to this Linux board or establishes
a compatible vendor update package. See [references](reverse-engineering.md).

Without a maintenance request, the USB starter uses the narrow `0007` marker and starts temporary SSH
in RAM only when the owner supplies a public key, and writes reports to the USB.
It does not automatically install a kernel, repair filesystems or reset passwords.
Removing the stick after a completed bootstrap is different from interrupting
backup/installation; follow the operation's completion report.

Expected layout:

```text
0007
flashforge_init.sh
revival/
  payload.tar
  SHA256SUMS
  install-overlay.sh
  rollback-overlay.sh
  usb-start.sh
  usb-pid1.sh
  usb-job.sh
  init-guard
  status-screen.sh
  status-screens/       # generated native framebuffer frames
  maintenance-enter.sh
  maintenance-ram.sh
  authorized_keys       # OWNER adds this; never in generic release
```

## USB-only first install

Follow [USB-only maintenance](usb-maintenance.md), beginning with CHECK. An
explicit REQUEST.txt selects CHECK, BACKUP, INSTALL or ROLLBACK. SSH and UART
are not normal prerequisites.

## Manual/console alternative

1. Confirm stock **2.8.4.366 F2.12 20210823**, the [exact profile/hash](compatibility.md),
   [backup](backup.md) and UART path.
2. Boot the USB starter for diagnostics/temporary SSH, if needed. Use your own
   public key and verify host identity through your own trusted connection.
3. Check storage. If damage exists, use [offline repair](storage-repair.md) first.
4. Boot RAM maintenance with root/data unmounted. The overlay installer refuses
   a running stock root, requires clean offline checks, mounts only the target
   root for file installation, preserves rollback externally, then unmounts it.
5. Review the payload manifest and identity policy. Installation intentionally
   disables Flashforge/Polar cloud switches and installs local-only controls.
6. Install the overlay using its explicit apply action. Review its external
   report. Do not reboot while file writes are in progress.
7. Check both unmounted filesystems again. Boot normally without the stick;
   verify identity, UI, dashboard, camera and network policy.

See the installer's `--help`/usage before use. Do not bypass its checks or feed
the old ignored `tools/install_*.py` scripts to an agent: those scripts are
historical operations tied to one development session.

### On-screen installation status

The offline installer shows **Checking printer → Checking USB files → Saving
rollback files → Installing Revival → Verifying installation → Installation
verified** on the supported framebuffer. The USB job then finishes and unmounts
USB before showing **Installation complete / Remove USB, then power-cycle**.
These are actual stages, not a timed
progress percentage; filesystem checks can take several minutes. Keep power
and USB connected. The screens have no touch controls and do not reboot for you.

**Installation stopped** means no install writes were started; inspect the
console for the failed prerequisite. **Installation incomplete / Do not reboot
yet** means writes may have started. Follow the console and [restore runbook](restore.md)
before attempting normal boot. Check `revival/reports/usb-*/install/FAILED.txt`
and the run's `operation.log`; the manual/console path uses `reports/install-*`.
Only successful final unmounts/offline checks and flushed completion reports lead
to the removal screen. Review `install/COMPLETE.txt` and the run's `FINISHED.txt`.

If the framebuffer or screen files are unavailable/incompatible, the console
remains authoritative. These screens accompany explicit maintenance requests; ordinary diagnostic
bootstrap does not claim an installation. Their PC render and shell fixture checks do not
replace the recorded hardware installation and rollback evidence.

## Updates

Factory Reset is a stock settings operation, not a Revival uninstall or firmware
restore. Read the [audited reset behavior and test limits](factory-reset.md).

Use a new allowlisted overlay against the same verified profile. Preserve the
existing owner key, dashboard key, identity and calibration. Never let a payload
replace the whole vendor settings tree. Stop active jobs before maintenance.

Vendor OTA/check-for-updates controls are intentionally disabled. This project
does not impersonate a vendor update server or accept arbitrary official updates
over a customized installation. A future stock restore is a separate, reviewed
operation using that printer's own backup.

## Rollback

Use the [restore runbook](restore.md) for failure-specific steps, stock-UI
rescue and the limits of full-device recovery.

The stock ELF is not rewritten. Restore the target's original `ffstartup.cfg`
to select the stock executable; disable Revival init scripts and restore its
externally saved files/configuration. Restore original cloud switches only if
you explicitly want cloud operation again. Preserve identity throughout.

Kernel rollback, when needed, must restore both active `/uImage` on the FAT boot
partition and the matching cached kernel under `/PROGRAM/kernel`. RAM-test it
first. [UART recovery](recovery.md) covers the known baseline.

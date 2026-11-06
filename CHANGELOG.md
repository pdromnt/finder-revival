# Changelog

## 0.1.0-rc.1 — private volunteer prerelease

* First-time SSH setup supports root / `flashforge_revival` without PC key generation.
  Owner public keys still select key-only setup; existing SSH credentials/configuration
  and later password changes survive upgrades and managed-file rollback.

* Integrate the boot splash into the same INSTALL as touchscreen, SSH and dashboard:
  convert the target's pinned kernel on USB with bounded RAM, save both original
  kernel copies externally, stage/readback both replacements and support matching rollback.

* Add a standalone USB quickstart for CHECK, verified backup, installation and rollback.
* Expand recovery documentation and organize the guide index by task.
* Normalize public Python/browser source formatting and enforce lint/format checks.
* Fix unprivileged CI fixtures without weakening production installer guards.
* Retain build/ABI/kernel checks under optimized Python and canonicalize gzip
  metadata so the splash tool reproduces the approved image across Python versions.

* Add one-shot USB CHECK/BACKUP/INSTALL/ROLLBACK requests with a RAM-only init
  handoff; routine installation need not depend on a soldered UART console.
* USB-only CHECK and the following power cycle passed on the development unit;
  avoid blanket init signals that disrupt its eMMC event worker. Packaged
  upgrade/reinstallation, managed rollback and normal boots also passed,
  preserving protected baseline bytes.
* Factory Reset and a subsequent power cycle verified on the development unit,
  preserving Revival, owner access and calibration with clean storage observations.
* Lightweight offline installer touchscreen stages and distinct failure guidance,
  with guarded console fallback and USB-packaged RGB565 screens.
* Native Qt touchscreen theme across home, local files, Tools/settings, dialogs,
  maintenance layouts and keyboard; neutral copy and consistent status/header.
* ARM C dashboard with GX conversion, Orca uploads, metadata/previews, file
  listing/deletion, explicit print prompts, identity footer and USB camera.
* Orca embedded-dashboard compatibility: avoid duplicate API-key values caused
  by differently cased header names in the slicer's fetch injection.
* Guarded native capacity queries and stock fallback; no motion-planner rewrite.
* Capped RAM vendor logging and documented offline ext4 repair/recovery.
* Local-first cloud/update deprecation and explicit outbound policy.
* Small allowlisted payload/USB bootstrap, per-printer identity/key policy,
  public-source exclusions and guides instead of publishing an eMMC clone.
* PC-side Orca presets for Finder 1/2/Plus, optional AllMetal PLA, user-created
  bed texture and Hydrahimera's separately licensed bed model with import guide.

The integrated public install path is experimental. Feature operation on one
printer is not acceptance of a universal installer. See `docs/validation.md`.

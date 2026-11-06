# Finder Revival agent guide

Read `README.md`, `docs/compatibility.md`, `docs/identity.md`,
`docs/security.md` and `docs/validation.md` before printer work.

* This repository shares a workspace with ignored private dumps, credentials,
  raw firmware and historical scripts. Never publish them or use them as generic
  release payloads. Run `python tools/check_public_tree.py` before staging/pushing.
* Preserve the target's serial, registration, calibration, MAC, Wi-Fi, accounts,
  print files and existing owner keys. Releases are file overlays, not clones.
* Supported GUI/kernel hashes are in `profiles/`. Do not guess addresses or
  disable guards to load another binary. Keep vendor ELF/control firmware intact.
* Never repair a mounted filesystem, including a read-only mount. Capture
  current snapshots externally, rehearse on copies, and inspect repairs first.
* Cloud/update controls are intentionally deprecated. Preserve credentials,
  save old switches externally, then set them OFF during authorized installation.
  Hidden menus do not prove network isolation. Keep local TCP/Wi-Fi/HTTP/camera working.
* No blind GUI restarts, homing, heating, printing, file deletion or firmware
  flashing. Obtain task-specific authorization for physical/installed changes.
  Stop the launcher and supervisor together when an idle GUI restart is authorized.
* Build/test work does not authorize a hardware transfer, push or GitHub release.
  Report fixture, RAM trial, installed runtime and physical validation separately.
* Do not launch timed rollback prototypes during a print. Keep logs and temporary
  state bounded in RAM; backups belong on the PC or external media.
* Follow `docs/recovery.md` for the verified 3.3 V UART/Pico mapping. Never connect VCC.
* Do not embed a personal SSH config, fixed LAN IP, private key or device identifier
  in public tooling. Accept operator-supplied parameters and redact reports.

Write concise user updates. Detailed evidence belongs in linked guides/reports.

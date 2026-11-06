# Native touchscreen

Production Qt 4/QWS presentation adapter for the exact supported Finder ELF.
The historical folder name remains to keep build paths stable. The stock ELF,
printer controller and native job actions are retained.

`native/revival_ui.c` handles home styling, shared Qt interception and guarded
query integration. `native/flow_skin.c` applies `flow.conf` to local files,
preview/printing, Tools/settings/network/information, maintenance dialogs and
the keyboard. `theme.conf` owns the home screen. `native_capacity.c` supplies
bounded read-only replacements for specific synchronous df/du pipelines.

The new home and subscreens, rounded dialogs, consistent status/header, neutral
homing copy and source rows preserve the original action/state model. Cloud
settings, Cloud queue and vendor update navigation are deliberately removed.
Updater/network suppression is separate; hiding widgets alone is not security.

`run-installed.sh` verifies the pinned stock ELF and theme checksums, loads
network/logging adapters for both themed and stock fallback launches, and applies
a post-startup core limit. A hang is not automatically detected. Stop the
launcher and supervisor together for an authorized idle restart. Never use a
timed rollback trial during printing.

Build with `python tools/build_ui_adapter.py` from the repository root.
The compiler targets ARMv5TE soft-float and rejects unavailable C23/time64 imports.
Use [compatibility](../docs/compatibility.md), [architecture](../docs/architecture.md),
[validation](../docs/validation.md) and [development](../docs/development.md).

`native/screen_audit.c` is optional diagnostic source, not a production payload.
Its inert previews and guarded navigation are not physical heater/motion tests.
Historical private scripts/transcripts are intentionally not part of this repo.

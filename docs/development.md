# Build, test and release

## Requirements

Python 3.11+ with `pip install -r requirements-dev.txt`, Git and Docker with Linux
containers. The tools derive the repository root from their own file location;
no maintainer username, private SSH configuration or live printer is required
to build. Linux is needed for the backend's process/socket integration tests.

```sh
docker build -t ffinder-image-tools:local tools/image-builder
docker build -t ffinder-ui-tools:local tools/ui-builder
docker build -t ffinder-web-tools:local web-service
python tools/build_ui_adapter.py
python tools/build_logging_patch.py
python tools/build_web_service.py
python tools/build_release.py --version 0.1.0-dev
python tools/check_public_tree.py
```

Generated files go under ignored `build/` or `dist/` paths. Payloads include only
explicit known inputs and per-file hashes. Stock firmware, complete images,
credentials and personalized keys are never inputs to the generic package.
Packaging also downloads approximately 285 MiB of hash-pinned Debian runtime/build
sources into the ignored `dist/runtime-source-cache/`. Subsequent builds reuse
verified cached files. It refuses different toolchain package identities or
notices until their source lock/materials are explicitly reviewed and updated.
The optional local splash tool requires your own verified kernel and outputs
an ignored patched image; it does not flash anything.

```sh
python tools/patch_boot_logo.py --input YOUR_ORIGINAL_uImage --output dist/private/uImage-revival
```

The generic patcher reproduced the exact previously RAM-tested splash image
from the supported original during development. Its output report still defaults
to `boot_tested: false`: each operator must validate their own artifact/target.
The ZIP deliberately excludes this local vendor-derived kernel output.

Backend tests:

```sh
docker run --rm --entrypoint python3 --mount type=bind,source=REPO_ABSOLUTE_PATH,target=/work ffinder-web-tools:local web-service/tests/integration.py
python -m unittest discover -s tests
docker run --rm --entrypoint node --mount type=bind,source=REPO_ABSOLUTE_PATH,target=/work ffinder-web-tools:local web-service/tests/orca_headers.cjs
```

Replace `REPO_ABSOLUTE_PATH` with the actual checkout. Repeat sanitizer tests
for release binaries; old logs do not establish acceptance of a new build.
Compiler/header changes can alter old-glibc ABI compatibility. Reject C23/time64
imports, wrong architecture/float ABI and unexpected stock hashes.

## Repository and publication

### Source formatting

Python uses Ruff 0.14.3 from `requirements-dev.txt`; browser sources use
Prettier 3.9.10. `.editorconfig`, `pyproject.toml` and `.prettierrc.json` record
the shared rules. Format only the tracked source set, keeping private research,
vendor code, original slicer exports and third-party notices untouched:

```sh
python -m ruff check $(git ls-files '*.py')
python -m ruff format $(git ls-files '*.py')
npx --yes prettier@3.9.10 --write web-service/www/app.js web-service/www/style.css web-service/www/index.html web-service/tests/orca_headers.cjs
```

Use `ruff format --check` and Prettier's `--check` to verify formatting without
edits. These are PC development tools;
no formatter or JavaScript runtime is installed on the printer.

### Private workspace material

The local workspace also holds ignored `research/`, `backups/`, `firmware/`,
`.sessions/`, USB working folders and historical one-printer scripts. They are
not required public source and must remain private. Root `.gitignore` starts
with default deny and selectively admits public source/assets/docs.

The publication audit checks the candidate Git tree for private paths, oversized
files, credential patterns, the development device's identifiers and broken
local Markdown links. Do not stage with `git add -f` to bypass exclusions.

Review `git diff --cached --stat`, the audit result and generated archive members
before pushing. Never upload a personalized USB stick, rootfs clone, Ghidra
database, captured camera image or fixture report containing real identity.

## Release gates

Publish a small generic USB starter/payload and checksums, not an 8 GB raw image.
Keep experimental labels and link the actual [validation record](validation.md).
Build output alone does not establish hardware acceptance of new changes.
Preserve per-target identity/key/calibration bytes and external rollback.

Publish **both** generated ZIPs: the small `experimental-usb.zip` and matching
`runtime-sources.zip`, with the common `SHA256SUMS`. The manifest binds the source
companion's hash to the backend hash. The companion includes full glibc/GCC
source sets, matching Linux/binutils build provenance, Debian patches/cross-build recipes, original application source,
object files, exact static runtime inputs and a verified relink path. Do not
publish only the binary and an upstream source URL. See
[runtime materials](../licenses/runtime/README.md). Upstream source archives and
generated objects belong in release assets, never Git or the printer filesystem.

The original supplied Orca export, bed texture and bed model are copied unchanged
under `slicer/orca/`, with an import guide and model attribution/license. They are
PC-side assets, outside the firmware payload. The USB ZIP includes them alongside
the guides so extracted documentation works without repository access; the
installer never copies them to internal printer storage.
The bed STL retains CC BY-NC-ND 4.0; Unlicense does not replace that asset license.

Do not overwrite tag/release history to replace a faulty artifact. Create a new
revision, explain the actual affected profile, and withdraw misleading instructions.
No GitHub push or release publication is performed merely by building locally.

## Release handoff

After recording the installation, rollback and Factory Reset
results in [validation](validation.md), generate a final versioned package from
the reviewed source. Prepare these files for manual upload to GitHub Releases:

* `finder-revival-VERSION-experimental-usb.zip`: generic starter, overlay,
  installer status screens, guides and notices; no personalized keys or firmware clone.
* `finder-revival-VERSION-runtime-sources.zip`: the matching runtime/source and
  relink companion, published alongside the USB package.
* `SHA256SUMS` and `manifest.json` from the same output directory. Verify both
  archive hashes and review their members before handing them off.
* Release notes naming the exact supported stock build/hash, actual hardware
  results, remaining gaps and experimental status. Link the installation,
  backup/restore and storage-corruption/repair guides; state the minimum 16 GB
  FAT32 USB requirement and logging mitigation's remaining limits.

Keep the original PC-side Orca files in `slicer/orca/` with their guide and
licenses; do not repackage or add them to the printer overlay for this handoff.
Generating this deliverable does not publish it or authorize a Git push.

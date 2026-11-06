# Runtime licenses and corresponding sources

The ARM backend statically incorporates GNU libc/libm and GCC support code.
Original Finder Revival application code is Unlicense. These third-party inputs
retain their notices and licenses; they are not relicensed by the project.

| Runtime input | Exact build package | Source set supplied |
| --- | --- | --- |
| glibc/libm and libc startup | `libc6-dev-armel-cross 2.41-11cross1` | `glibc 2.41-11`, plus `cross-toolchain-base-ports 74` |
| libgcc, exception support and GCC startup | `libgcc-14-dev-armel-cross 14.2.0-19cross1` | `gcc-14 14.2.0-19`, plus `gcc-14-cross-ports 10` |
| Additional build provenance | Debian `Built-Using` metadata | `linux 6.12.38-1` and `binutils 2.44-3` |

The full Debian copyright inventories are included verbatim, along with LGPL
2.1, GPL 2 and GPL 3 texts. glibc is primarily LGPL 2.1-or-later with additional
notices for individual files. The GCC copyright inventory includes the **GCC
Runtime Library Exception 3.1** and its terms. Consult those inventories and the
source-file notices rather than treating every upstream file as the same license.

## Binary-release accompaniment

Every generated release consists of **both**:

* `finder-revival-VERSION-experimental-usb.zip`: small install package, including
  these runtime notices and the exact source lock under `licenses/runtime/`.
* `finder-revival-VERSION-runtime-sources.zip`: the matching source/relink
  companion. It contains all six complete Debian source sets (`.dsc`, original
  archives and Debian patches/build recipes), plus application source, object
  files, exact static runtime inputs, relink scripts and an input manifest.

Distributors must make the matching companion available alongside the binary,
with equivalent download access and the common `SHA256SUMS`. Publish both files
in the same release; retain the corresponding versions when replacing a release.
An upstream URL alone is not the supplied corresponding-source archive. No
payment, account-gated source access or separate source request is intended.

`sources.lock.json` pins the source archive URLs, SHA256 hashes and lengths,
plus the actual installed cross-toolchain package identities. Original upstream
source/patch files come from Debian's HTTPS pool; all are checked against the
lock, and their SHA256 entries are cross-checked with the supplied `.dsc`.
The bundler checks hashes; it does not claim to verify uploader PGP signatures.
No project modifications to glibc/libgcc are made. Debian's patches are included.

## Relink or modify

Extract the companion **on a PC**, read `relink/README.md`, and run its
`relink.sh` with the recorded ARM GCC/binutils toolchain. Packaging requires the
unmodified result and the application source rebuild to match the distributed
backend SHA256 exactly. That path
uses only the included runtime archives/startup files, not an installed libc.
Replace those inputs with a compatible modified library to relink; application
source and `rebuild.sh` are supplied too. Rebuilding the upstream runtime from
source is described through its included Debian rules but has not been rehearsed.

No vendor printer kernel, Qt, GUI, controller binary, private key, printer backup or
device identity is in this companion. The Qt/UI, logging and update adapters
use `-nostdlib` and reference the target's existing runtime rather than bundling
vendor libraries. The optional locally patched vendor kernel is outside this
generic distribution and has separate source/licensing considerations.
The upstream Linux 6.12.38 source set supplies cross-build/header provenance;
it is not the printer's Linux 3.10 kernel and must never be flashed to it.

References: [LGPL 2.1, including section 6](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html),
[GNU linking FAQ](https://www.gnu.org/licenses/gpl-faq.html#LGPLStaticVsDynamic),
[GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html).

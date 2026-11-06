# Third-party software and artwork

The root Unlicense applies to original Finder Revival code, not everything
used by or pictured in the project.

| Component | Provenance / notice |
| --- | --- |
| Flashforge stock GUI, control tools, Qt/runtime and kernel | Obtained from the owner's printer; not included in source/payload archives. Keep vendor/source obligations separate when distributing any derived firmware. |
| `stb_image.h` | Sean Barrett and contributors; retained dual MIT/public-domain notice in header. Pinned URL/hash in `web-service/vendor/stb-image-source.json`. |
| zlib 1.3.1 | Jean-loup Gailly, Mark Adler and contributors; unchanged compression source subset in `installer/vendor/zlib/`, with upstream license, README and per-file/source archive hashes. Used by the freestanding ARM splash converter. USB packages retain its license and provenance under `licenses/zlib/`. |
| Atkinson Hyperlegible | Braille Institute; SIL OFL 1.1 retained in `assets/fonts/atkinson/OFL.txt`. |
| Inter web font | SIL OFL notice retained in `web-service/www/FONT-LICENSE.txt`. |
| Flashforge logo | Identifies original hardware. Geometry referenced from Simple Icons and official visual material; see `assets/bootscreen/PROVENANCE.md`. No affiliation/trademark permission is implied. |
| Finder slicer bed model | Hydrahimera, [Flashforge Finder - Bed STL](https://www.printables.com/model/845939-flashforge-finder-bed-stl), **CC BY-NC-ND 4.0**. Supplied STL preserved unchanged; credit/full license in `slicer/orca/`. This asset is not Unlicense. |
| Finder slicer bed texture | Created/provided by pdromnt; original image contributions use Unlicense. Flashforge branding retains its respective rights. Provenance and unchanged-file hash in `slicer/orca/`. |
| Finder Orca presets | User-supplied exports based on OrcaSlicer's Guider 2s and Generic PLA @Flashforge G3U system presets. Inheritance/provenance preserved; see `slicer/orca/README.md`. Original project modifications use Unlicense without relicensing upstream contributions. |
| Flashforge Toolkit | Project author's earlier Unlicense code/behavior; upstream linked in the reverse-engineering guide. |
| K2Dash visual reference | Author's project; original dashboard hierarchy/icon geometry informed this UI. No external running dependency. |
| e2fsprogs / BusyBox / OpenSSH | Use target's installed tools for maintenance/access; their licenses and notices remain applicable. Generic payloads do not bundle these vendor binaries. |
| GNU libc/libm | Static ARM backend inputs from `libc6-dev-armel-cross 2.41-11cross1`; full Debian copyright inventory and LGPL/GPL texts in `licenses/runtime/`. Matching glibc source and cross-build recipes accompany every generated binary package. |
| GCC support runtime | `libgcc`, exception support and startup objects from `libgcc-14-dev-armel-cross 14.2.0-19cross1`; GCC copyright inventory includes Runtime Library Exception 3.1. Matching GCC source/cross-build recipes and relink inputs accompany the binary. |

The release builder requires a matching runtime-source companion, verifies its
locked Debian source archives, and proves unmodified relinking reproduces the
ARM backend exactly. Both release ZIPs and their common checksums must be made
available together. See [runtime licensing/materials](licenses/runtime/README.md)
for the exact inventory, source access and modification/relink instructions.
Source/build archives stay on the PC, not the printer. The root Unlicense does
not remove third-party obligations. Hardware release gates remain separate.

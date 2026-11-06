# ARM runtime and relinking materials

The generated source companion contains this directory as `relink/`, including
the original application source, unstripped ARM object files, exact library and
startup inputs, their licenses and `manifest.json`. It includes no printer data.

On Linux with an ARM soft-float cross compiler/linker:

```sh
cd relink
sh relink.sh
```

The output is `revival-web.relinked`, plus `relinked.map`. With the recorded
Debian compiler/linker versions, the unmodified inputs reproduce the distributed
backend's SHA256 exactly. No installed libc/libm/libgcc is selected by this
command: all runtime inputs are explicit relative paths. `CC` may select another
compatible GCC driver. A different linker can change the hash without changing
the application's intended behavior.

To modify glibc or the GCC runtime, unpack the matching `upstream/` source sets
from the companion on the PC. Each `.dsc` accompanies its original source and
Debian patch archive. `dpkg-source -x NAME.dsc` reconstructs the patched tree.
The two cross-build recipe source packages and their Debian build rules are
included too, along with matching Linux header/kernel and binutils source sets
recorded by Debian's `Built-Using`. `manifest.json` records the cross binary/source package versions;
`licenses/runtime/sources.lock.json` records all supplied archive hashes.

Build the desired runtime for **arm-linux-gnueabi, ARMv5TE, soft-float**. Replace
the corresponding files under `runtime/`, keeping that runtime's startup objects
and archives consistent, then run `relink.sh`. For glibc, these are `crt1.o`,
`crti.o`, `crtn.o`, `libc.a` and `libm.a`. The GCC inputs are `crtbeginT.o`,
`crtend.o`, `libgcc.a` and `libgcc_eh.a`. Their complete matching source/build
recipes are provided; rebuilding those upstream toolchains was not tested here.

To change the application itself, edit `source/web-service/native/` and run `rebuild.sh`.
Its compile options and relative source paths match the released backend;
packaging checks its output hash too. The application source stays
Unlicense; glibc, GCC support inputs and third-party headers keep their licenses.
Unmodified relinking is tested during packaging. A modified executable needs
fresh integration/hardware validation; no signature/DRM prevents replacement.

These materials remain on the **PC**. Do not extract library/toolchain sources
or object archives into the printer's small root partition. There is no
automatic hardware transfer in any of these scripts.

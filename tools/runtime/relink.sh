#!/bin/sh
# Uses the included runtime inputs, not the build machine's libc/libgcc archives.
set -eu
cd "$(dirname "$0")"
"${CC:-arm-linux-gnueabi-gcc}" -static -nostdlib -march=armv5te -mfloat-abi=soft \
    runtime/crt1.o runtime/crti.o runtime/crtbeginT.o \
    objects/server.o objects/gx.o runtime/libm.a \
    -Wl,--start-group runtime/libgcc.a runtime/libgcc_eh.a runtime/libc.a \
    -Wl,--end-group runtime/crtend.o runtime/crtn.o \
    -Wl,-Map,relinked.map -o revival-web.relinked
sha256sum revival-web.relinked

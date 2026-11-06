#!/bin/sh
# Recompile original application source, then relink against included/modified libraries.
set -eu
cd "$(dirname "$0")"
mkdir -p objects
# Keep original relative source paths: assert/__FILE__ strings affect the hash.
cd source
for name in server gx; do
    "${CC:-arm-linux-gnueabi-gcc}" -std=c11 -D_FILE_OFFSET_BITS=64 -Os -Wall -Wextra \
        -Wno-misleading-indentation -Wno-unused-function -march=armv5te -mfloat-abi=soft \
        -c "web-service/native/$name.c" -o "../objects/$name.o"
done
cd ..
sh relink.sh

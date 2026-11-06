"""Compile logging-only adapter against the printer's legacy runtime ABI."""

import hashlib
import json
import shlex
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "logging/build"
OUT.mkdir(exist_ok=True)
prefix = [
    "docker",
    "run",
    "--rm",
    "--entrypoint",
    "/bin/sh",
    "--mount",
    f"type=bind,source={ROOT},target=/work",
    "ffinder-web-tools:local",
    "-c",
]


def run(args):
    subprocess.run(prefix + [shlex.join(args)], check=True)


run(
    [
        "gcc",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-DLOG_TEST",
        "logging/revival_log.c",
        "-o",
        "logging/build/test-filter",
        "-ldl",
        "-pthread",
    ]
)
run(["logging/build/test-filter"])
run(
    [
        "arm-linux-gnueabi-gcc",
        "-std=gnu11",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-march=armv5te",
        "-mfloat-abi=soft",
        "-fPIC",
        "-shared",
        "-nostdlib",
        "-fno-stack-protector",
        "-fno-builtin",
        "-o",
        "logging/build/librevival_log.so",
        "logging/revival_log.c",
    ]
)
binary = OUT / "librevival_log.so"
with binary.open("rb") as f:
    elf = ELFFile(f)
    if elf.header["e_machine"] != "EM_ARM" or elf.elfclass != 32:
        raise ValueError("Logging adapter must be ELF32 ARM")
    symbols = [
        s.name
        for s in elf.get_section_by_name(".dynsym").iter_symbols()
        if s["st_shndx"] == "SHN_UNDEF" and s.name
    ]
    if any("isoc23" in s or "time64" in s for s in symbols):
        raise ValueError("Logging adapter imports unavailable runtime symbols")
    exports = {
        s.name
        for s in elf.get_section_by_name(".dynsym").iter_symbols()
        if s["st_shndx"] != "SHN_UNDEF"
    }
    if "open" not in exports or "open64" in exports:
        raise ValueError("Logging adapter must export legacy open, not open64")
manifest = {
    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    "bytes": binary.stat().st_size,
    "source_sha256": hashlib.sha256((ROOT / "logging/revival_log.c").read_bytes()).hexdigest(),
    "undefined": symbols,
}
(OUT / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))

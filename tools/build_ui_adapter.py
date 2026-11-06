"""Build the ARMv5 adapter atomically and record the exact compiled inputs."""

import hashlib
import json
import subprocess
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "ui-prototype/native/revival_ui.c"
INPUTS = [
    SOURCE,
    ROOT / "ui-prototype/native/native_capacity.c",
    ROOT / "ui-prototype/native/native_capacity.h",
    ROOT / "ui-prototype/native/flow_skin.c",
    ROOT / "ui-prototype/native/flow_skin.h",
]
OUT = ROOT / "ui-prototype/build"
OUT.mkdir(parents=True, exist_ok=True)
temporary = OUT / "librevival_ui.so.new"
args = [
    "arm-linux-gnueabi-gcc",
    "-std=gnu11",
    "-O2",
    "-Wall",
    "-Wextra",
    "-march=armv5te",
    "-mfloat-abi=soft",
    "-fPIC",
    "-shared",
    "-nostdlib",
    "-fno-stack-protector",
    "-fno-builtin",
    "-Wl,-soname,librevival_ui.so",
    "-o",
    "ui-prototype/build/librevival_ui.so.new",
    "ui-prototype/native/revival_ui.c",
    "ui-prototype/native/native_capacity.c",
    "ui-prototype/native/flow_skin.c",
]
subprocess.run(
    [
        "docker",
        "run",
        "--rm",
        "--mount",
        f"type=bind,source={ROOT},target=/work",
        "ffinder-ui-tools:local",
    ]
    + args,
    check=True,
)
with temporary.open("rb") as f:
    elf = ELFFile(f)
    if elf.header["e_machine"] != "EM_ARM" or elf.elfclass != 32:
        raise ValueError("UI adapter must be ELF32 ARM")
    undefined = [
        s.name
        for s in elf.get_section_by_name(".dynsym").iter_symbols()
        if s["st_shndx"] == "SHN_UNDEF" and s.name
    ]
    if any("isoc23" in n or "time64" in n for n in undefined):
        raise ValueError("UI adapter imports symbols unavailable on the legacy runtime")
target = OUT / "librevival_ui.so"
temporary.replace(target)
manifest = {
    "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    "input_sha256": {
        p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in INPUTS
    },
    "binary_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
    "binary_bytes": target.stat().st_size,
    "compiler_arguments": args,
    "runtime_undefined_symbols": undefined,
    "target_stock_elf_sha256": "aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375",
}
(OUT / "adapter-build.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))

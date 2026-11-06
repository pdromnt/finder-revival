"""Build a freestanding kernel-art converter; no vendor kernel input is needed."""

import hashlib
import json
import subprocess
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ZLIB_SOURCES = (
    "adler32.c",
    "crc32.c",
    "deflate.c",
    "inflate.c",
    "inffast.c",
    "inftrees.c",
    "trees.c",
    "zutil.c",
)
EXPECTED_ART = "089cb2f0aca9843f942ef1a5ccf75d096a9a7f78e3fb1a0d100026a6acf13997"
FLAGS = [
    "-std=c11",
    "-Os",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-DZ_SOLO",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-stack-protector",
    "-fno-tree-loop-distribute-patterns",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wl,--gc-sections",
]


def artwork(root=ROOT):
    image = Image.open(root / "assets/bootscreen/revival-v3-flat.png").convert("RGB")
    if image.size != (480, 320):
        raise ValueError("Unexpected boot artwork geometry")
    image = image.transpose(Image.Transpose.ROTATE_270).quantize(
        colors=220, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE
    )
    data = bytes(image.getpalette()[:660]) + bytes(value + 32 for value in image.tobytes())
    if hashlib.sha256(data).hexdigest() != EXPECTED_ART:
        raise ValueError("Artwork differs from the approved kernel profile")
    return data


def sources(root=ROOT):
    vendor = root / "installer/vendor/zlib"
    locked = json.loads((vendor / "source.json").read_text())
    for name, expected in locked["files"].items():
        if hashlib.sha256((vendor / name).read_bytes()).hexdigest() != expected:
            raise ValueError("Modified zlib source: " + name)
    return ["installer/native/kernel_logo.c"] + ["installer/vendor/zlib/" + n for n in ZLIB_SOURCES]


def main():
    output = ROOT / "installer/build"
    output.mkdir(exist_ok=True)
    inputs = sources()
    mount = ["--mount", f"type=bind,source={ROOT},target=/work"]
    for compiler, name, extra in (
        ("gcc", "kernel-logo-host", []),
        (
            "arm-linux-gnueabi-gcc",
            "kernel-logo",
            [
                "-marm",
                "-march=armv5te",
                "-mfloat-abi=soft",
                "-nostdlib",
                "-static",
                "-Wl,--build-id=none",
            ],
        ),
    ):
        subprocess.run(
            [
                "docker",
                "run",
                "--rm",
                "--entrypoint",
                compiler,
                *mount,
                "ffinder-web-tools:local",
                *FLAGS,
                *extra,
                "-o",
                "installer/build/" + name,
                *inputs,
            ],
            check=True,
        )
    (output / "kernel-logo.art").write_bytes(artwork())
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "/work/installer/build/kernel-logo-host",
            *mount,
            "ffinder-web-tools:local",
            "--self-test",
        ],
        check=True,
    )
    symbols = subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "arm-linux-gnueabi-nm",
            *mount,
            "ffinder-web-tools:local",
            "-u",
            "installer/build/kernel-logo",
        ],
        capture_output=True,
        check=True,
    )
    if symbols.stdout.strip():
        raise RuntimeError("ARM converter has unexpected external symbols")
    print("Freestanding ARM logo converter and approved artwork built")


if __name__ == "__main__":
    main()

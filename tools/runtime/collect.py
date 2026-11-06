"""Inside the build container, collect exact ARM runtime inputs and prove relinking.

Writes generated files only under web-service/build/runtime. No printer access.
"""

import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "web-service/build/runtime"
PACKAGES = (
    "libc6-armel-cross",
    "libc6-dev-armel-cross",
    "libgcc-14-dev-armel-cross",
    "libgcc-s1-armel-cross",
    "gcc-14-arm-linux-gnueabi",
    "binutils-arm-linux-gnueabi",
)
RUNTIME = (
    "crt1.o",
    "crti.o",
    "crtbeginT.o",
    "libm.a",
    "libgcc.a",
    "libgcc_eh.a",
    "libc.a",
    "crtend.o",
    "crtn.o",
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest = json.loads((ROOT / "web-service/build/build.json").read_text())
    for name, expected in manifest["inputs"].items():
        if sha(ROOT / name) != expected:
            raise RuntimeError("Stale web build input: " + name)
    expected = manifest["binaries"]["revival-web-arm"]["sha256"]
    if sha(ROOT / "web-service/build/revival-web-arm") != expected:
        raise RuntimeError("Stale ARM executable")
    for name in (
        "objects",
        "runtime",
        "licenses",
        "source/web-service/native",
        "source/web-service/vendor",
    ):
        (OUT / name).mkdir(parents=True, exist_ok=True)
    compiler = "arm-linux-gnueabi-gcc"
    flags = manifest["arm_compile_flags"]
    for name in ("server", "gx"):
        subprocess.run(
            [
                compiler,
                *flags,
                "-c",
                f"web-service/native/{name}.c",
                "-o",
                str(OUT / "objects" / (name + ".o")),
            ],
            cwd=ROOT,
            check=True,
        )
    for name in RUNTIME:
        original = Path(
            subprocess.check_output([compiler, "-print-file-name=" + name], text=True).strip()
        ).resolve()
        if not original.is_file():
            raise RuntimeError("Missing runtime input: " + name)
        shutil.copyfile(original, OUT / "runtime" / name)
    # Exact provenance/license files from the packages whose archives were used.
    notices = {
        "glibc-copyright.txt": "/usr/share/doc/libc6-dev-armel-cross/copyright",
        "gcc-copyright.txt": "/usr/share/doc/gcc-14-arm-linux-gnueabi/copyright",
        "LGPL-2.1.txt": "/usr/share/common-licenses/LGPL-2.1",
        "GPL-2.txt": "/usr/share/common-licenses/GPL-2",
        "GPL-3.txt": "/usr/share/common-licenses/GPL-3",
    }
    for name, original in notices.items():
        data = Path(original).read_bytes()
        if data != (ROOT / "licenses/runtime" / name).read_bytes():
            raise RuntimeError("Runtime notice changed; review source lock and notices: " + name)
        (OUT / "licenses" / name).write_bytes(data)
    for name in manifest["inputs"]:
        shutil.copyfile(ROOT / name, OUT / "source" / name)
    shutil.copyfile(ROOT / "LICENSE", OUT / "source/LICENSE")
    for name in ("relink.sh", "rebuild.sh", "README.md"):
        shutil.copyfile(ROOT / "tools/runtime" / name, OUT / name)
    # Do not depend on any installed libc/libm/libgcc at relink time.
    subprocess.run(["sh", "relink.sh"], cwd=OUT, check=True)
    if sha(OUT / "revival-web.relinked") != expected:
        raise RuntimeError("Relink does not reproduce distributed ARM executable")
    subprocess.run(["sh", "rebuild.sh"], cwd=OUT, check=True)
    if sha(OUT / "revival-web.relinked") != expected:
        raise RuntimeError(
            "Application source rebuild does not reproduce distributed ARM executable"
        )
    rows = subprocess.check_output(
        [
            "dpkg-query",
            "-W",
            "-f=${binary:Package}|${Version}|${source:Package}|${source:Version}|${Built-Using}\n",
            *PACKAGES,
        ],
        text=True,
    ).splitlines()
    packages = [
        dict(zip(("binary", "version", "source", "source_version", "built_using"), row.split("|")))
        for row in rows
    ]
    lock = json.loads((ROOT / "licenses/runtime/sources.lock.json").read_text())
    expected_packages = lock["binary_packages"]
    if packages != expected_packages:
        raise RuntimeError("Toolchain packages differ from reviewed source lock")
    supplied = {package["package"]: package["version"] for package in lock["packages"]}
    for package in packages:
        required = [(package["source"], package["source_version"])]
        required += re.findall(r"([a-z0-9.+-]+) \(= ([^)]+)\)", package["built_using"])
        for name, version in required:
            if supplied.get(name) != version:
                raise RuntimeError(
                    "Missing matching source/build provenance: " + name + " " + version
                )
    proof = {
        "schema": 1,
        "arm_binary_sha256": expected,
        "relinked_sha256": expected,
        "source_rebuilt_sha256": expected,
        "packages": packages,
        "compile_flags": flags,
        "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
        "linker": subprocess.check_output(
            ["arm-linux-gnueabi-ld", "--version"], text=True
        ).splitlines()[0],
        "runtime_inputs": {name: sha(OUT / "runtime" / name) for name in RUNTIME},
        "objects": {name: sha(OUT / "objects" / name) for name in ("server.o", "gx.o")},
        "source_inputs": manifest["inputs"],
    }
    (OUT / "manifest.json").write_text(json.dumps(proof, indent=2) + "\n")
    print("ARM runtime collected; relink matches " + expected)


if __name__ == "__main__":
    main()

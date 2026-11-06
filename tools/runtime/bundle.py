"""Create a checksummed corresponding-source/relink companion for an ARM release."""

import hashlib
import json
import re
import subprocess
import urllib.request
import zipfile
from concurrent.futures import ThreadPoolExecutor


def sha(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def matches(path, entry):
    return path.is_file() and path.stat().st_size == entry["bytes"] and sha(path) == entry["sha256"]


def fetch(entry, cache):
    name = entry["name"]
    if not re.fullmatch(r"[A-Za-z0-9.+~_-]+", name):
        raise ValueError("Unsafe runtime source filename")
    if not entry["url"].startswith("https://deb.debian.org/debian/pool/main/"):
        raise ValueError("Unreviewed runtime source origin")
    target = cache / name
    if matches(target, entry):
        return target
    temporary = target.with_name(target.name + ".partial")
    for attempt in range(3):
        try:
            print("Downloading matching runtime source: " + name, flush=True)
            request = urllib.request.Request(
                entry["url"], headers={"User-Agent": "Finder-Revival-source-bundler/1"}
            )
            with (
                urllib.request.urlopen(request, timeout=60) as response,
                temporary.open("wb") as output,
            ):
                total = 0
                while block := response.read(1024 * 1024):
                    total += len(block)
                    if total > entry["bytes"]:
                        raise ValueError("Source download exceeds locked length: " + name)
                    output.write(block)
            if not matches(temporary, entry):
                raise ValueError("Runtime source checksum/length mismatch: " + name)
            temporary.replace(target)
            return target
        except Exception:
            temporary.unlink(missing_ok=True)
            if attempt == 2:
                raise
    raise AssertionError("unreachable")


def verify_source_set(package, cache):
    entries = {entry["name"]: entry for entry in package["files"]}
    descriptors = [name for name in entries if name.endswith(".dsc")]
    if len(descriptors) != 1:
        raise ValueError("Expected one Debian source descriptor")
    body = (cache / descriptors[0]).read_text()
    if not re.search(r"^Source: " + re.escape(package["package"]) + r"$", body, re.M):
        raise ValueError("Source package identity mismatch")
    if not re.search(r"^Version: " + re.escape(package["version"]) + r"$", body, re.M):
        raise ValueError("Source package version mismatch")
    field = re.search(r"^Checksums-Sha256:\n((?: [^\n]+\n)+)", body, re.M)
    if not field:
        raise ValueError("Missing descriptor SHA256 field")
    described = {}
    for line in field[1].splitlines():
        checksum, length, name = line.split()
        described[name] = (checksum, int(length))
    expected = {
        name: (entry["sha256"], entry["bytes"])
        for name, entry in entries.items()
        if name not in descriptors
    }
    if described != expected:
        raise ValueError("Source lock does not match Debian descriptor")


def zip_add(archive, name, data, mode=0o644):
    info = zipfile.ZipInfo(name, (2020, 1, 1, 0, 0, 0))
    # Upstream archives are already compressed. Avoid recompressing 100+ MiB.
    info.compress_type = (
        zipfile.ZIP_STORED if name.endswith((".xz", ".gz")) else zipfile.ZIP_DEFLATED
    )
    info.external_attr = (0o100000 | mode) << 16
    archive.writestr(info, data)


def prepare(root, output, version):
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "python3",
            "--mount",
            f"type=bind,source={root},target=/work",
            "ffinder-web-tools:local",
            "tools/runtime/collect.py",
        ],
        check=True,
    )
    lock_path = root / "licenses/runtime/sources.lock.json"
    lock = json.loads(lock_path.read_text())
    generated = root / "web-service/build/runtime"
    proof = json.loads((generated / "manifest.json").read_text())
    cache = root / "dist/runtime-source-cache"
    cache.mkdir(parents=True, exist_ok=True)
    entries = [entry for package in lock["packages"] for entry in package["files"]]
    if len({entry["name"] for entry in entries}) != len(entries):
        raise ValueError("Duplicate runtime source filenames")
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(lambda entry: fetch(entry, cache), entries))
    for package in lock["packages"]:
        verify_source_set(package, cache)
    # No generated-directory glob: only known objects, inputs, notices and source.
    paths = [
        "manifest.json",
        "README.md",
        "relink.sh",
        "rebuild.sh",
        "relinked.map",
        "objects/server.o",
        "objects/gx.o",
        "source/LICENSE",
        "source/web-service/native/server.c",
        "source/web-service/native/gx.c",
        "source/web-service/native/gx.h",
        "source/web-service/vendor/stb_image.h",
    ]
    paths += ["runtime/" + name for name in proof["runtime_inputs"]]
    paths += [
        "licenses/" + name
        for name in (
            "glibc-copyright.txt",
            "gcc-copyright.txt",
            "LGPL-2.1.txt",
            "GPL-2.txt",
            "GPL-3.txt",
        )
    ]
    target = output / f"finder-revival-{version}-runtime-sources.zip"
    temporary = target.with_suffix(".zip.partial")
    files = {}
    with zipfile.ZipFile(
        temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:

        def add(name, path):
            data = path.read_bytes()
            files[name] = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
            zip_add(archive, name, data)

        add("README.md", root / "licenses/runtime/README.md")
        for name in (
            "sources.lock.json",
            "glibc-copyright.txt",
            "gcc-copyright.txt",
            "LGPL-2.1.txt",
            "GPL-2.txt",
            "GPL-3.txt",
        ):
            add("licenses/runtime/" + name, root / "licenses/runtime" / name)
        for package in lock["packages"]:
            for entry in package["files"]:
                add("upstream/" + package["package"] + "/" + entry["name"], cache / entry["name"])
        for name in paths:
            add("relink/" + name, generated / name)
        zip_add(archive, "MANIFEST.json", (json.dumps(files, indent=2) + "\n").encode())
    temporary.replace(target)
    return {
        "archive": target.name,
        "sha256": sha(target),
        "bytes": target.stat().st_size,
        "source_lock_sha256": sha(lock_path),
        "arm_binary_sha256": proof["arm_binary_sha256"],
        "unmodified_relink_verified": True,
        "application_source_rebuild_verified": True,
    }

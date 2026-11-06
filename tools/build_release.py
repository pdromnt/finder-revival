"""Build an identity-free experimental USB starter + offline Revival overlay.

No printer connection, firmware acquisition, stock binary or private key input.
The optional stock-kernel patch is a separate local-only tool.
"""

import argparse
import hashlib
import io
import json
import re
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
WWW_FILES = ("app.js", "favicon.svg", "FONT-LICENSE.txt", "index.html", "inter.woff2", "style.css")
PAYLOAD_FILES = (
    {
        "PROGRAM/revival-ui/" + name
        for name in (
            "librevival_ui.so",
            "theme.conf",
            "flow.conf",
            "bootscreen.png",
            "AtkinsonHyperlegible-Regular.ttf",
            "AtkinsonHyperlegible-Bold.ttf",
            "FONT-LICENSE.txt",
            "SHA256SUMS",
        )
    }
    | {"PROGRAM/revival-web/" + name for name in ("revival-web", "camera-watch.sh", "SHA256SUMS")}
    | {"PROGRAM/revival-web/www/" + name for name in WWW_FILES}
    | {
        "PROGRAM/revival-logging/librevival_log.so",
        "PROGRAM/revival-logging/SHA256SUMS",
        "PROGRAM/revival-network/librevival_offline.so",
        "PROGRAM/revival-network/SHA256SUMS",
        "PROGRAM/software/2.8.4/finder-revival.sh",
        "etc/init.d/S02revival-logs",
        "etc/init.d/S03revival-local-only",
        "etc/init.d/S50revival-sshd",
        "etc/init.d/S51revival-web",
        "etc/init.d/S52revival-camera",
    }
)
MANAGED_FILES = PAYLOAD_FILES | {
    "PROGRAM/ffstartup.cfg",
    "etc/revival/authorized_keys",
    "etc/revival/sshd_config",
    "etc/revival-web/key",
    "opt/flashforge/cloud-ff",
    "opt/flashforge/cloud-mymini",
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify_digest(data, expected, description):
    if digest(data) != expected:
        raise ValueError("Stale or unexpected build input: " + description)


def normalized(path):
    p = PurePosixPath(path)
    if p.is_absolute() or ".." in p.parts or "\\" in path:
        raise ValueError(path)
    return p.as_posix()


def allowed(path):
    path = normalized(path)
    return path in PAYLOAD_FILES


def archive_files(files):
    out = io.BytesIO()
    with tarfile.open(fileobj=out, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for path, (data, mode) in sorted(files.items()):
            if not allowed(path):
                raise ValueError("Forbidden payload path: " + path)
            entry = tarfile.TarInfo(normalized(path))
            entry.mode = mode
            entry.size = len(data)
            entry.mtime = 0
            t.addfile(entry, io.BytesIO(data))
    return out.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--build", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,60}", args.version):
        parser.error("Invalid version")
    if args.build:
        for tool in ["build_ui_adapter.py", "build_logging_patch.py", "build_web_service.py"]:
            subprocess.run([sys.executable, str(ROOT / "tools" / tool)], check=True)
    output = ROOT / "dist" / ("finder-revival-" + args.version)
    output.mkdir(parents=True, exist_ok=True)
    initbuild = ROOT / "installer/build"
    initbuild.mkdir(exist_ok=True)
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "arm-linux-gnueabi-gcc",
            "--mount",
            f"type=bind,source={ROOT},target=/work",
            "ffinder-web-tools:local",
            "-std=gnu11",
            "-Os",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-marm",
            "-march=armv5te",
            "-mfloat-abi=soft",
            "-nostdlib",
            "-static",
            "-ffreestanding",
            "-fno-builtin",
            "-fno-stack-protector",
            "-fno-pie",
            "-no-pie",
            "-fomit-frame-pointer",
            "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables",
            "-Wl,--build-id=none",
            "-o",
            "installer/build/init-guard",
            "installer/init-guard.c",
        ],
        check=True,
    )
    # A binary package is never generated without its matching source/relink companion.
    sys.path.insert(0, str(ROOT / "tools"))
    from build_install_screens import build as build_install_screens
    from runtime.bundle import prepare

    runtime_materials = prepare(ROOT, output, args.version)
    netbuild = ROOT / "network/build"
    netbuild.mkdir(exist_ok=True)
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "arm-linux-gnueabi-gcc",
            "--mount",
            f"type=bind,source={ROOT},target=/work",
            "ffinder-web-tools:local",
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
            "network/build/librevival_offline.so",
            "network/revival_offline.c",
        ],
        check=True,
    )
    files = {}

    def add(path, data, mode=0o600):
        if not allowed(path):
            raise ValueError(path)
        files[path] = (data, mode)

    def source(path):
        return (ROOT / path).read_bytes()

    # Refuse stale source/build combinations, not merely missing output files.
    ui = json.loads(source("ui-prototype/build/adapter-build.json"))
    for path, expected in ui["input_sha256"].items():
        verify_digest(source(path), expected, path)
    verify_digest(source("ui-prototype/build/librevival_ui.so"), ui["binary_sha256"], "UI adapter")
    web = json.loads(source("web-service/build/build.json"))
    for path, expected in web["inputs"].items():
        verify_digest(source(path), expected, path)
    verify_digest(
        source("web-service/build/revival-web-arm"),
        web["binaries"]["revival-web-arm"]["sha256"],
        "ARM web service",
    )
    log = json.loads(source("logging/build/build.json"))
    verify_digest(source("logging/revival_log.c"), log["source_sha256"], "logging source")
    verify_digest(
        source("logging/build/librevival_log.so"), log["binary_sha256"], "logging adapter"
    )
    add("PROGRAM/revival-ui/librevival_ui.so", source("ui-prototype/build/librevival_ui.so"))
    add(
        "PROGRAM/revival-ui/theme.conf",
        source("ui-prototype/theme.conf").replace(b"/run/revival-ui/", b"/PROGRAM/revival-ui/"),
    )
    add("PROGRAM/revival-ui/flow.conf", source("ui-prototype/flow.conf"))
    add("PROGRAM/revival-ui/bootscreen.png", source("assets/bootscreen/revival-v3-flat.png"))
    for name in ["AtkinsonHyperlegible-Regular.ttf", "AtkinsonHyperlegible-Bold.ttf"]:
        add("PROGRAM/revival-ui/" + name, source("assets/fonts/atkinson/" + name))
    add("PROGRAM/revival-ui/FONT-LICENSE.txt", source("assets/fonts/atkinson/OFL.txt"))
    add("PROGRAM/revival-web/revival-web", source("web-service/build/revival-web-arm"), 0o755)
    add(
        "PROGRAM/revival-web/camera-watch.sh",
        source("web-service/camera-watch.sh").replace(b"\r\n", b"\n"),
    )
    for name in WWW_FILES:
        add("PROGRAM/revival-web/www/" + name, source("web-service/www/" + name))
    add("PROGRAM/revival-logging/librevival_log.so", source("logging/build/librevival_log.so"))
    add(
        "PROGRAM/revival-network/librevival_offline.so",
        source("network/build/librevival_offline.so"),
    )
    for src, target in [
        ("logging/S02revival-logs", "etc/init.d/S02revival-logs"),
        ("network/S03revival-local-only", "etc/init.d/S03revival-local-only"),
        ("installer/S50revival-sshd", "etc/init.d/S50revival-sshd"),
        ("web-service/S51revival-web", "etc/init.d/S51revival-web"),
        ("web-service/S52revival-camera", "etc/init.d/S52revival-camera"),
        ("ui-prototype/run-installed.sh", "PROGRAM/software/2.8.4/finder-revival.sh"),
    ]:
        add(target, source(src).replace(b"\r\n", b"\n"), 0o755)
    for directory in ["revival-ui", "revival-web", "revival-logging", "revival-network"]:
        prefix = "PROGRAM/" + directory + "/"
        checks = "".join(
            f"{digest(data)}  {path[len(prefix) :]}\n"
            for path, (data, mode) in sorted(files.items())
            if path.startswith(prefix)
        )
        add(prefix + "SHA256SUMS", checks.encode())
    payload = archive_files(files)
    manifest = {
        "version": args.version,
        "status": "experimental",
        "profile": "finder-plus-2.8.4",
        "identity": "preserve target; no owner data in payload",
        "kernel_included": False,
        "runtime_materials": runtime_materials,
        "files": {
            path: {"sha256": digest(data), "bytes": len(data), "mode": oct(mode)}
            for path, (data, mode) in sorted(files.items())
        },
    }
    usb = {
        "START-HERE.md": (
            f"# Finder Revival {args.version}\n\n"
            "Read the [USB quickstart](docs/quickstart.md) before preparing the stick.\n\n"
            "Required: Linux/eMMC Finder Plus / V2, stock 2.8.4.366 F2.12 build 20210823,\n"
            "and a 16 GB minimum FAT32 USB drive. Exact binary guards remain mandatory.\n\n"
            "Start with CHECK, then verify your own backup before INSTALL.\n"
            "This package contains no armed request, owner key or vendor kernel.\n"
            "Keep the runtime-source archive on the PC.\n\n"
            "[USB operations](docs/usb-maintenance.md) | "
            "[Backup](docs/backup.md) | [Restore](docs/restore.md) | "
            "[UART recovery](docs/recovery.md)\n"
        ).encode(),
        "0007": b"",
        "flashforge_init.sh": source("installer/flashforge_init.sh").replace(b"\r\n", b"\n"),
        "revival/payload.tar": payload,
        "revival/manifest.json": (json.dumps(manifest, indent=2) + "\n").encode(),
        "README.md": source("README.md"),
        "LICENSE": source("LICENSE"),
        "THIRD_PARTY.md": source("THIRD_PARTY.md"),
        "profiles/finder-plus-2.8.4.json": source("profiles/finder-plus-2.8.4.json"),
    }
    for name in [
        "install-overlay.sh",
        "rollback-overlay.sh",
        "overlay-common.sh",
        "status-screen.sh",
        "usb-start.sh",
        "usb-pid1.sh",
        "usb-job.sh",
        "maintenance-enter.sh",
        "maintenance-ram.sh",
        "backup-emmc.sh",
        "sshd_config",
    ]:
        usb["revival/" + name] = source("installer/" + name).replace(b"\r\n", b"\n")
    usb["revival/init-guard"] = source("installer/build/init-guard")
    usb["revival/managed-paths.txt"] = ("".join(p + "\n" for p in sorted(MANAGED_FILES))).encode()
    usb["revival/payload-paths.txt"] = ("".join(p + "\n" for p in sorted(PAYLOAD_FILES))).encode()
    usb["revival/REQUEST.example.txt"] = b"CHECK\n"
    for name, data in build_install_screens(ROOT / "installer/build/status-screens").items():
        usb["revival/status-screens/" + name] = data
    for doc in sorted((ROOT / "docs").glob("*.md")):
        usb["docs/" + doc.name] = doc.read_bytes()
    for name in ["touchscreen-home.png", "print-sources.png", "settings.png"]:
        usb["docs/images/" + name] = source("docs/images/" + name)
    usb["docs/images/README.md"] = source("docs/images/README.md")
    for name in [
        "README.md",
        "sources.lock.json",
        "glibc-copyright.txt",
        "gcc-copyright.txt",
        "LGPL-2.1.txt",
        "GPL-2.txt",
        "GPL-3.txt",
    ]:
        usb["licenses/runtime/" + name] = source("licenses/runtime/" + name)
    usb["revival/SHA256SUMS"] = "".join(
        f"{digest(data)}  {path[len('revival/') :]}\n"
        for path, data in sorted(usb.items())
        if path.startswith("revival/")
    ).encode()
    for name in usb:
        if any(
            x in name.lower()
            for x in ["authorized_keys", "host_key", "private", "rootfs", "datafs", "mmcblk"]
        ):
            raise ValueError("Forbidden release member: " + name)
    target = output / (output.name + "-experimental-usb.zip")
    with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path, data in sorted(usb.items()):
            info = zipfile.ZipInfo(path, (2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, data)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (output / "SHA256SUMS").write_text(
        digest(target.read_bytes())
        + "  "
        + target.name
        + "\n"
        + runtime_materials["sha256"]
        + "  "
        + runtime_materials["archive"]
        + "\n"
    )
    print(
        json.dumps(
            {
                "artifact": str(target),
                "bytes": target.stat().st_size,
                "runtime_sources": runtime_materials,
                "payload_files": len(files),
                "kernel_included": False,
                "private_identity_included": False,
                "validation_record": "docs/validation.md",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

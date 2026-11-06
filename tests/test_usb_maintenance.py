"""Linux fixtures exercise file-level install/rollback; hardware/mount guards are separate."""

import hashlib
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("release_usb", ROOT / "tools/build_release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux shell fixture")
class UsbMaintenance(unittest.TestCase):
    def test_default_password_hash_matches_the_documented_login(self):
        import ctypes

        library = ctypes.CDLL("libcrypt.so.1")
        library.crypt.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        library.crypt.restype = ctypes.c_char_p
        expected = (ROOT / "installer/default-root-password.hash").read_bytes().strip()
        self.assertEqual(library.crypt(b"flashforge_revival", expected), expected)
        self.assertNotEqual(library.crypt(b"wrong-password", expected), expected)

    def test_sshd_starts_for_password_or_owner_key_but_not_unconfigured(self):
        for mode in ("password", "key", "no-key", "no-config"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                account = base / "etc/revival"
                account.mkdir(parents=True)
                called = base / "sshd-called"
                daemon = base / "sshd"
                daemon.write_text('#!/bin/sh\nprintf "called\\n" >> "' + str(called) + '"\n')
                daemon.chmod(0o755)
                (account / "ssh_host_ed25519_key").write_text("fixture-host-key")
                if mode != "no-config":
                    name = "sshd_password_config" if mode == "password" else "sshd_config"
                    (account / "sshd_config").write_bytes((ROOT / "installer" / name).read_bytes())
                if mode == "key":
                    (account / "authorized_keys").write_text("fixture-owner-key")
                text = (ROOT / "installer/S50revival-sshd").read_text()
                text = text.replace("/etc/revival/", str(account) + "/")
                text = text.replace("/run/revival-sshd.pid", str(base / "sshd.pid"))
                text = text.replace("/run/revival-sshd.log", str(base / "sshd.log"))
                text = text.replace("/usr/sbin/sshd", str(daemon))
                script = base / "start"
                script.write_text(text)
                subprocess.run(["sh", str(script), "start"], check=True, capture_output=True)
                self.assertEqual(called.exists(), mode in ("password", "key"))

    def test_init_guard_contexts_and_failed_write_restoration(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(Path(tmp) / "guard-test")
            subprocess.run(
                [
                    "gcc",
                    "-std=c11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DINIT_GUARD_TEST",
                    str(ROOT / "installer/init-guard.c"),
                    "-o",
                    binary,
                ],
                check=True,
                capture_output=True,
            )
            result = subprocess.run([binary], check=True, capture_output=True)
            self.assertIn(b"I/O failures passed", result.stdout)

    def test_shutdown_targets_only_positive_userspace_pids(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            for pid, value in {
                "1": "100 50",
                "2": "0 0",
                "474": "0 0",
                "600": "108 5",
                "601": "0 0",
                "602": "bad 0",
                "603": "12 2",
                "604": "",
            }.items():
                directory = base / pid
                directory.mkdir()
                (directory / "statm").write_text(value + "\n")
            source = (ROOT / "installer/usb-pid1.sh").read_text()
            function = source.split("stop_userspace() {", 1)[1].split("\n}", 1)[0]
            # Fixture replaces only /proc; production has no alternative proc path.
            function = function.replace("/proc/[0-9]*", str(base) + "/[0-9]*")
            script = (
                'kill() { printf "SIGNAL:%s:%s\\n" "$1" "$2"; }\n'
                + "stop_userspace() {"
                + function
                + "\n}\n"
                + "stop_userspace TERM\nstop_userspace KILL\n"
            )
            result = subprocess.run(["sh", "-c", script], check=True, capture_output=True)
            calls = [
                line for line in result.stdout.decode().splitlines() if line.startswith("SIGNAL:")
            ]
            self.assertEqual(
                calls,
                ["SIGNAL:-TERM:600", "SIGNAL:-TERM:603", "SIGNAL:-KILL:600", "SIGNAL:-KILL:603"],
            )

    def test_real_offline_guard_refuses_host(self):
        run = subprocess.run(
            [
                "sh",
                "-c",
                '. "$1"; offline_profile',
                "guard",
                str(ROOT / "installer/overlay-common.sh"),
            ],
            capture_output=True,
        )
        self.assertNotEqual(run.returncode, 0)

    def test_managed_scope_rejects_identity_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp)
            (target / "PROGRAM").mkdir()
            (target / "PROGRAM/revival-ui").symlink_to("/etc")
            for path in (
                "etc/shadow",
                "opt/flashforge/serial-number",
                "../etc/shadow",
                "PROGRAM/revival-ui/../secret",
                "PROGRAM/revival-ui/passwd",
            ):
                run = subprocess.run(
                    [
                        "sh",
                        "-c",
                        'ROOT=$1; . "$2"; safe_path "$3"',
                        "guard",
                        tmp,
                        str(ROOT / "installer/overlay-common.sh"),
                        path,
                    ],
                    capture_output=True,
                )
                self.assertNotEqual(run.returncode, 0, path)

    def test_duplicate_managed_paths_refused_even_in_conditional(self):
        with tempfile.TemporaryDirectory() as tmp:
            paths = Path(tmp) / "paths"
            paths.write_text("PROGRAM/revival-ui/theme.conf\n" * 2)
            run = subprocess.run(
                [
                    "sh",
                    "-c",
                    '. "$1"; ROOT=$2; if validate_paths "$3"; then exit 0; else exit 9; fi',
                    "guard",
                    str(ROOT / "installer/overlay-common.sh"),
                    tmp,
                    str(paths),
                ],
                capture_output=True,
            )
            self.assertEqual(run.returncode, 9)

    def fixture(self, base, upgrade, fault=None):
        package = base / "usb/revival"
        package.mkdir(parents=True)
        storage = base / "storage"
        storage.mkdir()
        old = {}

        def save(path, data):
            full = storage / path
            full.parent.mkdir(parents=True, exist_ok=True)
            full.write_bytes(data)
            old[path] = data

        save(
            "PROGRAM/ffstartup.cfg",
            (
                "AppName = ./software/%VERSION%/finder-"
                + ("revival.sh" if upgrade else "plus-arm")
                + "\nLogDir = /data/log\nLogLevel = 0\nLogConsole = true\n"
            ).encode(),
        )
        save("opt/flashforge/serial-number", b"fixture-serial-not-real\n")
        save("opt/flashforge/extruder-calibration", b"fixture-calibration\n")
        save("opt/flashforge/cloud-ff", b"OFF\r\n" if upgrade else b"ON\r\n")
        save("opt/flashforge/cloud-mymini", b"OFF\r\n" if upgrade else b"ON\r\n")
        save("data/user-job.gx", b"fixture-print-content-never-modified")
        save("PROGRAM/kernel/1.0.366/uImage-finder-367-20190814", b"STOCK001")
        boot = base / "boot-storage"
        boot.mkdir()
        (boot / "uImage").write_bytes(b"STOCK001")
        if fault == "unknown-boot":
            (boot / "uImage").write_bytes(b"UNKNOWN1")
        save("etc/passwd", b"root:x:0:0:root:/root:/bin/sh\ndaemon:x:1:1:daemon:/:/bin/false\n")
        save("etc/shadow", b"root:fixture-owner-hash:0:0:99999:7:::\ndaemon:*:10000:0:99999:7:::\n")
        if upgrade:
            for path in release.PAYLOAD_FILES:
                if path.endswith("SHA256SUMS"):
                    continue
                save(path, ("old " + path).encode())
            save("etc/revival-web/key", b"a" * 64)
            save("etc/revival/authorized_keys", b"fixture-owner-key-preserve")
            save("etc/revival/sshd_config", b"fixture-owner-ssh-config")
            save("PROGRAM/revival-ui/user-art.txt", b"owner-content-outside-managed-files")
        payload = {
            p: (("new " + p).encode(), 0o600)
            for p in release.PAYLOAD_FILES
            if not p.endswith("SHA256SUMS")
        }
        for directory in ("revival-ui", "revival-web", "revival-logging", "revival-network"):
            prefix = "PROGRAM/" + directory + "/"
            sums = "".join(
                hashlib.sha256(data).hexdigest() + "  " + p[len(prefix) :] + "\n"
                for p, (data, mode) in sorted(payload.items())
                if p.startswith(prefix)
            )
            payload[prefix + "SHA256SUMS"] = (sums.encode(), 0o600)
        (package / "payload.tar").write_bytes(release.archive_files(payload))
        (package / "managed-paths.txt").write_text(
            "".join(p + "\n" for p in sorted(release.MANAGED_FILES))
        )
        (package / "payload-paths.txt").write_text(
            "".join(p + "\n" for p in sorted(release.PAYLOAD_FILES))
        )
        common = (ROOT / "installer/overlay-common.sh").read_text()
        # Explicit fixture-only replacements: no production bypass flag or device is used.
        common += """
offline_profile() { :; }
verify_stock() { :; }
mount() {
    if [ "$1" = -t ]; then
        for destination; do :; done
        rmdir "$destination"
        case "$*" in
            *mmcblk0p1*) ln -s "$FIXTURE_BOOT" "$destination" ;;
            *) ln -s "$FIXTURE_STORAGE" "$destination" ;;
        esac
    fi
}
umount() { :; }
e2fsck() { :; }
sync() { :; }
df() { printf 'fixture 100000 0 100000\\n'; }
"""
        if fault == "bad-staging":
            common += """
cp() {
    for destination; do :; done
    case "$destination" in */boot/uImage.revival-new) printf BROKEN11 > "$destination" ;; *) command cp "$@" ;; esac
}
"""
        if fault == "boot-rename":
            common += """
mv() {
    case "$1" in */boot/uImage.revival-new) return 1 ;; *) command mv "$@" ;; esac
}
"""
        (package / "overlay-common.sh").write_text(common)
        (package / "status-screen.sh").write_text("revival_screen() { :; }\n")
        kernel = (ROOT / "installer/kernel-install.sh").read_text()
        kernel = (
            kernel.replace(
                "ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87",
                hashlib.sha256(b"STOCK001").hexdigest(),
            )
            .replace(
                "651ded763ab91ea7b8584ba8d7f9cfb80b53a1c57835d8c62b855b893efdbdb0",
                hashlib.sha256(b"LOGO0001").hexdigest(),
            )
            .replace("KERNEL_SIZE=3199416", "KERNEL_SIZE=8")
        )
        # Only fixture kernel constants/mounts differ; staging/commit/rollback is real.
        (package / "kernel-install.sh").write_text(kernel)
        (package / "kernel-logo").write_text('#!/bin/sh\nprintf LOGO0001 > "$3"\n')
        if fault == "converter":
            (package / "kernel-logo").write_text("#!/bin/sh\nexit 9\n")
        (package / "kernel-logo").chmod(0o755)
        for name in (
            "ssh-password.sh",
            "sshd_config",
            "sshd_password_config",
            "default-root-password.hash",
        ):
            (package / name).write_bytes((ROOT / "installer" / name).read_bytes())
        for name in ("install-overlay.sh", "rollback-overlay.sh"):
            text = (ROOT / "installer" / name).read_text()
            # The copied fixture uses temporary files and mocked device/mount guards.
            # It must also run as CI's ordinary user; production retains this check.
            text = text.replace('[ "$(id -u)" = 0 ]', ": # Fixture requires no root privileges.")
            text = text.replace("/run/revival-install", str(base / "install-stage")).replace(
                "/run/revival-rollback", str(base / "rollback-stage")
            )
            # The fake USB is an ordinary temporary directory, never a host mount.
            text = (
                "\n".join(
                    ":" if line.startswith('awk -v p="$BASE" ') else line
                    for line in text.splitlines()
                )
                + "\n"
            )
            (package / name).write_text(text)
        sums = "".join(
            hashlib.sha256(p.read_bytes()).hexdigest() + "  " + p.name + "\n"
            for p in sorted(package.iterdir())
            if p.is_file()
        )
        (package / "SHA256SUMS").write_text(sums)
        for run in ("usb-fixture-install", "usb-fixture-rollback"):
            (package / "reports" / run).mkdir(parents=True)
        commands = base / "bin"
        commands.mkdir()
        (commands / "fsck.fat").write_text("#!/bin/sh\nexit 0\n")
        (commands / "fsck.fat").chmod(0o755)
        env = {
            **os.environ,
            "PATH": str(commands) + ":" + os.environ["PATH"],
            "FIXTURE_STORAGE": str(storage),
            "FIXTURE_BOOT": str(boot),
            "REVIVAL_RUN_ID": "usb-fixture-install",
        }
        run = subprocess.run(
            ["sh", str(package / "install-overlay.sh"), "--apply"], env=env, capture_output=True
        )
        if fault:
            self.assertNotEqual(run.returncode, 0)
            self.assertFalse(
                (package / "reports/usb-fixture-install/install/COMPLETE.txt").exists()
            )
            return package, storage, old, env
        self.assertEqual(run.returncode, 0, run.stderr.decode())
        self.assertTrue((package / "reports/usb-fixture-install/install/COMPLETE.txt").exists())
        self.assertIn(b"finder-revival.sh", (storage / "PROGRAM/ffstartup.cfg").read_bytes())
        self.assertEqual((boot / "uImage").read_bytes(), b"LOGO0001")
        self.assertEqual(
            (storage / "PROGRAM/kernel/1.0.366/uImage-finder-367-20190814").read_bytes(),
            b"LOGO0001",
        )
        shadow = (storage / "etc/shadow").read_bytes()
        self.assertIn(b"daemon:*:10000:0:99999:7:::\n", shadow)
        if upgrade:
            self.assertEqual(shadow, old["etc/shadow"])
        else:
            expected = (ROOT / "installer/default-root-password.hash").read_text().strip()
            self.assertEqual(shadow.splitlines()[0].decode().split(":")[1], expected)
            self.assertIn(
                b"PasswordAuthentication yes", (storage / "etc/revival/sshd_config").read_bytes()
            )
            self.assertTrue(
                (package / "reports/usb-fixture-install/install/ssh-root-before.private").exists()
            )
        return package, storage, old, env

    def rollback(self, package, env):
        return subprocess.run(
            ["sh", str(package / "rollback-overlay.sh"), "--apply", "usb-fixture-install"],
            env={**env, "REVIVAL_RUN_ID": "usb-fixture-rollback"},
            capture_output=True,
        )

    def test_fresh_and_upgrade_round_trip(self):
        for upgrade in (False, True):
            with self.subTest(upgrade=upgrade), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), upgrade)
                key = (storage / "etc/revival-web/key").read_bytes()
                shadow = (storage / "etc/shadow").read_bytes()
                run = self.rollback(package, env)
                self.assertEqual(run.returncode, 0, run.stderr.decode())
                self.assertEqual((Path(env["FIXTURE_BOOT"]) / "uImage").read_bytes(), b"STOCK001")
                for path, content in old.items():
                    if path == "etc/shadow":
                        content = shadow  # Credentials are retained across managed-file rollback.
                    if path.startswith("opt/flashforge/cloud-"):
                        content = b"OFF\r\n"
                    self.assertEqual((storage / path).read_bytes(), content, path)
                for path in release.PAYLOAD_FILES - old.keys():
                    self.assertFalse((storage / path).exists(), path)
                self.assertEqual((storage / "etc/revival-web/key").read_bytes(), key)
                self.assertTrue(
                    (package / "reports/usb-fixture-rollback/rollback/RESTORED.txt").exists()
                )

    def test_owner_password_change_survives_upgrade_and_rollback(self):
        with tempfile.TemporaryDirectory() as tmp:
            package, storage, old, env = self.fixture(Path(tmp), True)
            shadow = (
                b"root:fixture-new-owner-hash:20000:0:99999:7:::\ndaemon:*:10000:0:99999:7:::\n"
            )
            (storage / "etc/shadow").write_bytes(shadow)
            result = self.rollback(package, env)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((storage / "etc/shadow").read_bytes(), shadow)

    def test_splash_preparation_failures_leave_internal_files_unchanged(self):
        for fault in ("unknown-boot", "converter"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), False, fault)
                for path, data in old.items():
                    self.assertEqual((storage / path).read_bytes(), data, path)
                report = package / "reports/usb-fixture-install/install/FAILED.txt"
                self.assertIn("Install writes started: 0", report.read_text())

    def test_splash_stage_and_commit_failures_preserve_backups_without_completion(self):
        for fault in ("bad-staging", "boot-rename"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), False, fault)
                report = package / "reports/usb-fixture-install/install"
                self.assertEqual((report / "kernel-boot-before.img").read_bytes(), b"STOCK001")
                self.assertEqual((report / "kernel-cache-before.img").read_bytes(), b"STOCK001")
                self.assertIn("Install writes started: 1", (report / "FAILED.txt").read_text())
                self.assertEqual((Path(env["FIXTURE_BOOT"]) / "uImage").read_bytes(), b"STOCK001")
                expected = b"STOCK001" if fault == "bad-staging" else b"LOGO0001"
                self.assertEqual(
                    (storage / "PROGRAM/kernel/1.0.366/uImage-finder-367-20190814").read_bytes(),
                    expected,
                )
                self.assertEqual(
                    (storage / "PROGRAM/ffstartup.cfg").read_bytes(), old["PROGRAM/ffstartup.cfg"]
                )

    def test_password_setup_refuses_shadow_symlink_or_duplicate_root(self):
        for issue in ("symlink", "duplicate"):
            with self.subTest(issue=issue), tempfile.TemporaryDirectory() as tmp:
                base = Path(tmp)
                target = base / "target"
                package = base / "package"
                (target / "etc").mkdir(parents=True)
                package.mkdir()
                (target / "etc/passwd").write_text("root:x:0:0:root:/root:/bin/sh\n")
                original = base / "owner-shadow"
                content = "root:fixture-owner-hash:10000:0:99999:7:::\n"
                original.write_text(content)
                if issue == "symlink":
                    (target / "etc/shadow").symlink_to(original)
                else:
                    (target / "etc/shadow").write_text(content * 2)
                result = subprocess.run(
                    [
                        "sh",
                        "-c",
                        'ROOT=$1; BASE=$2; . "$3"; prepare_ssh_setup',
                        "fixture",
                        str(target),
                        str(package),
                        str(ROOT / "installer/ssh-password.sh"),
                    ],
                    capture_output=True,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(original.read_text(), content)

    def test_owner_public_key_selects_key_setup_without_password_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            target = base / "target"
            package = base / "package"
            (target / "etc").mkdir(parents=True)
            package.mkdir()
            shadow = target / "etc/shadow"
            shadow.write_text("fixture-owner-account-bytes\n")
            (package / "authorized_keys").write_text("fixture-owner-public-key\n")
            result = subprocess.run(
                [
                    "sh",
                    "-c",
                    'ROOT=$1; BASE=$2; . "$3"; prepare_ssh_setup; printf "%s" "$SSH_SETUP"',
                    "fixture",
                    str(target),
                    str(package),
                    str(ROOT / "installer/ssh-password.sh"),
                ],
                capture_output=True,
                check=True,
            )
            self.assertEqual(result.stdout, b"key")
            self.assertEqual(shadow.read_text(), "fixture-owner-account-bytes\n")

    def test_wrong_device_or_edited_file_or_corrupt_archive_refused(self):
        for issue in ("serial", "edited", "archive"):
            with self.subTest(issue=issue), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), True)
                if issue == "serial":
                    (storage / "opt/flashforge/serial-number").write_bytes(
                        b"another-fixture-device"
                    )
                elif issue == "edited":
                    (storage / "PROGRAM/revival-ui/theme.conf").write_bytes(
                        b"owner-edited-after-install"
                    )
                else:
                    archive = package / "reports/usb-fixture-install/install/rollback-private.tar"
                    archive.write_bytes(archive.read_bytes() + b"corruption")
                current = {
                    p.relative_to(storage).as_posix(): p.read_bytes()
                    for p in storage.rglob("*")
                    if p.is_file()
                }
                run = self.rollback(package, env)
                self.assertNotEqual(run.returncode, 0)
                self.assertEqual(
                    current,
                    {
                        p.relative_to(storage).as_posix(): p.read_bytes()
                        for p in storage.rglob("*")
                        if p.is_file()
                    },
                )
                self.assertFalse((package / "reports/usb-fixture-rollback/rollback").exists())


if __name__ == "__main__":
    unittest.main()

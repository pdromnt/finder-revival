import importlib.util
import io
import json
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("release", ROOT / "tools/build_release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseContract(unittest.TestCase):
    def test_logo_gzip_header_is_independent_of_python_platform_metadata(self):
        import gzip
        from unittest.mock import patch

        spec = importlib.util.spec_from_file_location("logo", ROOT / "tools/patch_boot_logo.py")
        logo = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(logo)
        payload = b"fixture kernel payload" * 20
        compressed = bytearray(gzip.compress(payload, compresslevel=9, mtime=0))
        canonical = None
        for platform_byte in (0, 3, 10, 255):
            compressed[9] = platform_byte
            with patch.object(logo.gzip, "compress", return_value=bytes(compressed)):
                result = logo.compress_payload(payload)
            self.assertEqual(result[9], 255)
            self.assertEqual(gzip.decompress(result), payload)
            if canonical is None:
                canonical = result
            self.assertEqual(result, canonical)

    def test_digest_guard_still_refuses_stale_input_with_python_optimization(self):
        code = (
            "import runpy; module = runpy.run_path("
            + repr(str(ROOT / "tools/build_release.py"))
            + "); "
            + 'module["verify_digest"](b"stale fixture", "0" * 64, "fixture")'
        )
        result = subprocess.run([sys.executable, "-O", "-c", code], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Stale or unexpected build input: fixture", result.stderr)

    @unittest.skipUnless(
        sys.platform.startswith("linux") and Path("/proc/sys/net/ipv6").exists(),
        "requires an IPv6-enabled Linux test kernel",
    )
    def test_ipv6_kernel_refused_before_route_changes(self):
        result = subprocess.run(
            ["sh", str(ROOT / "network/S03revival-local-only"), "start"], capture_output=True
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Unsupported IPv6 stack", result.stderr)

    @unittest.skipUnless(
        sys.platform.startswith("linux"), "shell fixture runs in Linux CI/container"
    )
    def test_unknown_elf_refused_before_disabled_theme_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            app = base / "app"
            marker = base / "wrong-app-ran"
            theme = base / "theme"
            theme.mkdir()
            app.write_text("#!/bin/sh\ntouch " + str(marker) + "\n")
            app.chmod(0o755)
            (theme / "DISABLED").touch()
            (base / "policy.so").touch()
            (base / "logger.so").touch()
            source = (ROOT / "ui-prototype/run-installed.sh").read_text()
            for key, value in {
                "APP": app,
                "ROOT": theme,
                "STATE": base / "state",
                "POLICY": base / "policy.so",
                "LOGGER": base / "logger.so",
            }.items():
                import re

                source = re.sub(r"^" + key + r"=.*$", key + "=" + str(value), source, flags=re.M)
            launcher = base / "launcher"
            launcher.write_text(source)
            run = subprocess.run(["sh", str(launcher)], capture_output=True)
            self.assertEqual(run.returncode, 126)
            self.assertFalse(marker.exists())
            self.assertIn("Unsupported stock ELF", (base / "state/failed").read_text())

    def test_identity_and_vendor_paths_denied(self):
        for path in [
            "opt/flashforge/serialNo",
            "etc/wpa_supplicant.conf",
            "etc/shadow",
            "etc/ssh/ssh_host_ed25519_key",
            "data/test.gx",
            "PROGRAM/software/2.8.4/finder-plus-arm",
            "PROGRAM/kernel/1.0.366/uImage",
            "PROGRAM/revival-web/www/.env",
            "PROGRAM/revival-ui/owner.key",
        ]:
            self.assertFalse(release.allowed(path), path)

    def test_path_traversal_denied(self):
        for path in [
            "/etc/shadow",
            "../etc/shadow",
            "PROGRAM/revival-ui/../../identity",
            "PROGRAM\\revival-ui\\key",
        ]:
            with self.assertRaises(ValueError):
                release.normalized(path)

    def test_archive_is_regular_allowlisted_and_deterministic(self):
        files = {"PROGRAM/revival-ui/theme.conf": (b"fixture", 0o600)}
        first = release.archive_files(files)
        self.assertEqual(first, release.archive_files(files))
        with tarfile.open(fileobj=io.BytesIO(first)) as t:
            entry = t.getmembers()[0]
            self.assertTrue(entry.isfile())
            self.assertEqual(entry.uid, 0)
            self.assertEqual(entry.mtime, 0)
        with self.assertRaises(ValueError):
            release.archive_files({"etc/shadow": (b"no", 0o600)})

    def test_profile_has_no_owner_identifiers(self):
        profile = json.loads((ROOT / "profiles/finder-plus-2.8.4.json").read_text())
        for field in ["serial", "uuid", "cid", "private_key", "registration_code", "wifi_password"]:
            self.assertNotIn(field, profile)


if __name__ == "__main__":
    unittest.main()

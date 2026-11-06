import contextlib
import hashlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_bundle", ROOT / "tools/runtime/bundle.py")
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


class RuntimeMaterials(unittest.TestCase):
    def entry(self, data=b"source fixture"):
        return {
            "name": "example_1.tar.xz",
            "url": "https://deb.debian.org/debian/pool/main/e/example/example_1.tar.xz",
            "sha256": hashlib.sha256(data).hexdigest(),
            "bytes": len(data),
        }

    def test_valid_cache_needs_no_network(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            entry = self.entry()
            (cache / entry["name"]).write_bytes(b"source fixture")
            with patch.object(
                bundle.urllib.request, "urlopen", side_effect=AssertionError("network called")
            ):
                self.assertEqual(bundle.fetch(entry, cache), cache / entry["name"])

    def test_corrupt_cache_is_replaced_only_by_verified_download(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            entry = self.entry()
            (cache / entry["name"]).write_bytes(b"corrupted")
            with patch.object(
                bundle.urllib.request, "urlopen", return_value=io.BytesIO(b"source fixture")
            ):
                self.assertTrue(bundle.matches(bundle.fetch(entry, cache), entry))

    def test_bad_download_is_never_published_as_source(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            entry = self.entry()
            with patch.object(
                bundle.urllib.request,
                "urlopen",
                side_effect=lambda *a, **kw: io.BytesIO(b"wrong bytes"),
            ):
                with self.assertRaisesRegex(ValueError, "checksum/length"):
                    bundle.fetch(entry, cache)
            self.assertFalse((cache / entry["name"]).exists())
            self.assertFalse(list(cache.glob("*.partial")))

    def test_source_path_and_origin_are_restricted(self):
        with tempfile.TemporaryDirectory() as directory:
            for field, value in [("name", "../secret"), ("url", "https://example.org/source")]:
                entry = self.entry()
                entry[field] = value
                with self.assertRaises(ValueError):
                    bundle.fetch(entry, Path(directory))

    def test_descriptor_cannot_silently_substitute_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            entry = self.entry()
            descriptor = (
                "Source: example\nVersion: 1\nChecksums-Sha256:\n "
                + "0" * 64
                + " 14 example_1.tar.xz\n"
            )
            (cache / "example_1.dsc").write_text(descriptor)
            package = {
                "package": "example",
                "version": "1",
                "files": [entry, {"name": "example_1.dsc", "sha256": "unused", "bytes": 0}],
            }
            with self.assertRaisesRegex(ValueError, "does not match"):
                bundle.verify_source_set(package, cache)

    def test_reviewed_lock_has_complete_source_sets(self):
        lock = json.loads((ROOT / "licenses/runtime/sources.lock.json").read_text())
        self.assertEqual(
            {p["package"] for p in lock["packages"]},
            {
                "glibc",
                "gcc-14",
                "cross-toolchain-base-ports",
                "gcc-14-cross-ports",
                "linux",
                "binutils",
            },
        )
        for package in lock["packages"]:
            self.assertEqual(sum(f["name"].endswith(".dsc") for f in package["files"]), 1)
            self.assertTrue(any(f["name"].endswith((".xz", ".gz")) for f in package["files"]))
            for entry in package["files"]:
                self.assertRegex(entry["sha256"], r"^[0-9a-f]{64}$")
                self.assertGreater(entry["bytes"], 0)


if __name__ == "__main__":
    with contextlib.redirect_stdout(io.StringIO()):
        unittest.main()

"""Converter refusal/self-test checks without distributing a vendor kernel fixture."""

import hashlib
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_installer", ROOT / "tools/build_installer.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class ArtworkContract(unittest.TestCase):
    def test_approved_pixels_and_unchanged_upstream_sources(self):
        data = builder.artwork()
        self.assertEqual(len(data), 154260)
        self.assertEqual(hashlib.sha256(data).hexdigest(), builder.EXPECTED_ART)
        self.assertEqual(len(builder.sources()), 9)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux native compiler fixtures")
class KernelConverter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temporary.name) / "kernel-logo"
        subprocess.run(
            ["gcc", *builder.FLAGS, "-o", str(cls.binary), *builder.sources()],
            cwd=ROOT,
            capture_output=True,
            check=True,
        )

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_hash_crc_and_division_self_tests(self):
        result = subprocess.run([self.binary, "--self-test"], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"self-test passed", result.stdout)

    def test_invalid_kernel_refused_before_any_output(self):
        for size in (0, 3199415, 3199416, 3199417):
            with self.subTest(size=size), tempfile.TemporaryDirectory() as tmp:
                folder = Path(tmp)
                original = folder / "input"
                original.write_bytes(b"X" * size)
                art = folder / "art"
                art.write_bytes(builder.artwork())
                output = folder / "output"
                result = subprocess.run(
                    [self.binary, original, art, output, folder / "work"],
                    capture_output=True,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output.exists())
                self.assertEqual(len(list(folder.iterdir())), 2)

    def test_fat_dirty_flag_exception_refuses_structural_damage(self):
        dirty = (
            "CP437: Invalid argument\nfsck.fat 3.0.28 (2015-05-16)\n"
            "0x25: Dirty bit is set. Fs was not properly unmounted and some data may be corrupt.\n"
            " Automatically removing dirty bit.\nLeaving filesystem unchanged.\n"
            "/dev/mmcblk0p1: 1 files, 1563/10211 clusters\n"
        )
        for extra, expected in (("", 0), ("Cluster chain broken\n", 1)):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as tmp:
                folder = Path(tmp)
                (folder / "diagnostic").write_text(dirty + extra)
                source = (ROOT / "installer/kernel-install.sh").read_text()
                script = "set -eu\n" + source + '\ncheck_boot_fat "$1"\n'
                commands = folder / "bin"
                commands.mkdir()
                (commands / "fsck.fat").write_text('#!/bin/sh\ncat "$DIAGNOSTIC"\nexit 1\n')
                (commands / "fsck.fat").chmod(0o755)
                path = folder / "test.sh"
                path.write_text(script)
                import os

                result = subprocess.run(
                    ["sh", path, folder / "report"],
                    capture_output=True,
                    env={
                        **os.environ,
                        "DIAGNOSTIC": str(folder / "diagnostic"),
                        "PATH": str(commands) + ":" + os.environ["PATH"],
                    },
                )
                self.assertEqual(result.returncode, expected, result.stderr.decode())


if __name__ == "__main__":
    unittest.main()

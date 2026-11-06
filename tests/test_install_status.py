"""PC render checks and isolated shell fault injection; never touches printer devices."""

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("screens", ROOT / "tools/build_install_screens.py")
screens = importlib.util.module_from_spec(spec)
spec.loader.exec_module(screens)


class ScreenRendering(unittest.TestCase):
    def test_all_frames_decode_in_the_panel_orientation(self):
        for state in screens.STATES:
            with self.subTest(state=state):
                image, raw = screens.render(state)
                self.assertEqual(len(raw), 307200)
                decoded = Image.frombytes("RGB", (320, 480), raw, "raw", "BGR;16")
                decoded = decoded.transpose(Image.Transpose.ROTATE_90)
                # Quantization is the only permitted difference from the logical image.
                for original, actual in zip(image.getdata(), decoded.getdata()):
                    self.assertTrue(
                        all(abs(a - b) <= limit for a, b, limit in zip(original, actual, (7, 3, 7)))
                    )


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux shell fixtures")
class ShellStatus(unittest.TestCase):
    def run_exit_fixture(self, body, cleanup_fails=False):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            prefix = (
                (ROOT / "installer/install-overlay.sh").read_text().split('[ "$(id -u)" = 0 ]')[0]
            )
            harness = (
                prefix
                + """
revival_screen() { echo "SCREEN:$2"; }
sync() { :; }
kernel_close() { :; }
umount() { echo "UNMOUNT"; return """
                + ("1" if cleanup_fails else "0")
                + """; }
ROOT="$BASE/target"
REPORT="$BASE/report"
mkdir -p "$ROOT" "$REPORT"
"""
                + body
            )
            script = base / "fixture.sh"
            script.write_text(harness)
            run = subprocess.run(["sh", str(script), "--apply"], capture_output=True, text=True)
            failed = base / "report/FAILED.txt"
            return run, failed.read_text() if failed.exists() else ""

    def test_check_failure_retains_exit_and_reports_no_writes(self):
        run, report = self.run_exit_fixture("status checking\nexit 7\n")
        self.assertEqual(run.returncode, 7)
        self.assertIn("SCREEN:failed\n", run.stdout)
        self.assertNotIn("UNMOUNT", run.stdout)
        self.assertIn("Install writes started: 0", report)

    def test_write_failure_cleans_up_and_does_not_claim_success(self):
        run, report = self.run_exit_fixture(
            "MOUNTED=1\nWRITES_STARTED=1\nstatus installing\nexit 9\n"
        )
        self.assertEqual(run.returncode, 9)
        self.assertIn("UNMOUNT\nSCREEN:failed-writes", run.stdout)
        self.assertNotIn("SCREEN:complete", run.stdout)
        self.assertIn("Failed stage: installing", report)

    def test_cleanup_failure_is_a_failure_not_completion(self):
        run, report = self.run_exit_fixture(
            "MOUNTED=1\nWRITES_STARTED=1\nstatus verifying\nfinish\nstatus complete\n", True
        )
        self.assertNotEqual(run.returncode, 0)
        self.assertNotIn("SCREEN:complete", run.stdout)
        self.assertIn("SCREEN:failed-writes", run.stdout)
        self.assertIn("storage cleanup failed", run.stderr)

    def test_term_cleans_up_and_retains_signal_exit(self):
        run, report = self.run_exit_fixture("MOUNTED=1\nWRITES_STARTED=1\nkill -TERM $$\n")
        self.assertEqual(run.returncode, 143)
        self.assertIn("UNMOUNT\nSCREEN:failed-writes", run.stdout)

    def test_final_check_failure_after_unmount_still_requires_recovery(self):
        run, report = self.run_exit_fixture(
            "MOUNTED=1\nWRITES_STARTED=1\nstatus verifying\nfinish\nexit 4\nstatus complete\n"
        )
        self.assertEqual(run.returncode, 4)
        self.assertEqual(run.stdout.count("UNMOUNT"), 1)
        self.assertIn("SCREEN:failed-writes", run.stdout)
        self.assertNotIn("SCREEN:complete", run.stdout)

    def test_success_does_not_unmount_twice_or_show_failure(self):
        run, report = self.run_exit_fixture(
            "MOUNTED=1\nWRITES_STARTED=1\nstatus verifying\nfinish\nstatus complete\n"
        )
        self.assertEqual(run.returncode, 0)
        self.assertEqual(run.stdout.count("UNMOUNT"), 1)
        self.assertIn("SCREEN:complete", run.stdout)
        self.assertNotIn("SCREEN:failed", run.stdout)
        self.assertEqual(report, "")

    def test_framebuffer_guards_and_console_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            fb = base / "fb"
            fb.touch()
            sysfs = base / "sys"
            sysfs.mkdir()
            for name, value in {
                "virtual_size": "320,480",
                "bits_per_pixel": "16",
                "stride": "640",
                "pan": "0,0",
            }.items():
                (sysfs / name).write_text(value + "\n")
            frames = base / "status-screens"
            frames.mkdir()
            raw = screens.render("checking")[1]
            frame = frames / "checking.raw"
            frame.write_bytes(raw)
            source = (ROOT / "installer/status-screen.sh").read_text()
            # Only fixture device/path substitutions; all guard and copy logic is real.
            source = (
                source.replace("/dev/fb0", str(fb))
                .replace("/sys/class/graphics/fb0", str(sysfs))
                .replace("[ -c ", "[ -f ")
            )
            script = base / "test.sh"
            script.write_text("set -eu\n" + source + '\nrevival_screen "$1" checking\n')

            def invoke():
                return subprocess.run(["sh", str(script), str(base)], capture_output=True)

            self.assertEqual(invoke().returncode, 0)
            self.assertEqual(fb.read_bytes(), raw)
            for name, value in {
                "virtual_size": "480,320",
                "bits_per_pixel": "32",
                "stride": "1280",
                "pan": "0,480",
            }.items():
                original = (sysfs / name).read_text()
                (sysfs / name).write_text(value)
                fb.write_bytes(b"untouched")
                run = invoke()
                self.assertEqual(run.returncode, 0)
                self.assertIn(b"follow the console", run.stderr)
                self.assertEqual(fb.read_bytes(), b"untouched")
                (sysfs / name).write_text(original)
            frame.write_bytes(raw[:-1])
            self.assertEqual(invoke().returncode, 0)
            self.assertEqual(fb.read_bytes(), b"untouched")
            frame.unlink()
            self.assertEqual(invoke().returncode, 0)
            self.assertEqual(fb.read_bytes(), b"untouched")
            frame.write_bytes(raw)
            # Simulate an I/O error copying a frame without failing the install shell.
            script.write_text(
                "set -eu\n"
                + source
                + '\ncat() { if [ "$1" = "'
                + str(frame)
                + '" ]; then return 1; else command cat "$@"; fi; }\nrevival_screen "$1" checking\n'
            )
            run = invoke()
            self.assertEqual(run.returncode, 0)
            self.assertIn(b"follow the console", run.stderr)


if __name__ == "__main__":
    unittest.main()

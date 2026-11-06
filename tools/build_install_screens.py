"""Render offline installer screens on the PC; no image decoder runs on the printer."""

import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, PngImagePlugin

ROOT = Path(__file__).resolve().parents[1]
STATES = {
    "checking": (
        "Checking printer",
        "Checking offline filesystems.",
        "This step can take several minutes.",
    ),
    "package": (
        "Checking USB files",
        "Verifying the installation package.",
        "No install writes have started.",
    ),
    "backup": (
        "Saving rollback files",
        "Saving files and preparing the splash.",
        "This can take several minutes.",
    ),
    "installing": (
        "Installing Revival",
        "Writing the local firmware overlay.",
        "Do not switch off or remove USB.",
    ),
    "verifying": (
        "Verifying installation",
        "Checking files and storage.",
        "Do not switch off or remove USB.",
    ),
    "complete": (
        "Installation verified",
        "Files verified; storage unmounted.",
        "Finishing USB maintenance.",
    ),
    "failed": (
        "Installation stopped",
        "No install writes were started.",
        "Read the USB report or console.",
    ),
    "failed-writes": (
        "Installation incomplete",
        "Do not reboot yet.",
        "Read the USB report and restore guide.",
    ),
    "rolling-back": (
        "Restoring previous files",
        "Restoring this printer\u2019s own backup.",
        "Do not switch off or remove USB.",
    ),
    "usb-enter": (
        "Entering maintenance",
        "Preparing the RAM environment.",
        "No boot settings will be saved.",
    ),
    "usb-backup": (
        "Backing up storage",
        "Saving this printer to USB.",
        "This can take an hour or more.",
    ),
    "usb-stopped": (
        "Maintenance stopped",
        "Keep power and USB connected.",
        "Review the USB maintenance report.",
    ),
    "usb-check-done": (
        "Checks passed",
        "Offline storage checks passed.",
        "No firmware files were installed.",
    ),
    "usb-backup-done": (
        "Backup complete",
        "Storage images saved to USB.",
        "Verify their checksums on the PC.",
    ),
    "usb-install-done": (
        "Installation complete",
        "Revival files verified.",
        "Review the USB report before use.",
    ),
    "usb-rollback-done": (
        "Rollback complete",
        "Previous managed files restored.",
        "Owner keys kept; cloud stays off.",
    ),
}


def render(state):
    title, detail, hint = STATES[state]
    image = Image.new("RGB", (480, 320), "#101820")
    draw = ImageDraw.Draw(image)
    fonts = ROOT / "assets/fonts/atkinson"

    def text(x, y, value, size, fill, bold=False):
        face = "AtkinsonHyperlegible-" + ("Bold" if bold else "Regular") + ".ttf"
        font = ImageFont.truetype(str(fonts / face), size)
        bounds = draw.textbbox((x, y), value, font=font, anchor="lt")
        if bounds[2] > 456 or bounds[3] > 308:
            raise ValueError(f"Installer text exceeds screen: {value}")
        draw.text((x, y), value, font=font, fill=fill, anchor="lt")

    text(24, 19, "FINDER", 24, "#f5f8fa", True)
    text(124, 26, "REVIVAL", 16, "#ffad60", True)
    draw.line((24, 55, 456, 55), fill="#2a3e4c")
    accent = "#ffad60" if state.startswith("failed") else "#f5f8fa"
    text(24, 86, title, 32, accent, True)
    text(24, 139, detail, 23, "#f5f8fa", state == "failed-writes")
    text(24, 180, hint, 21, "#9bb0be")
    draw.line((24, 236, 456, 236), fill="#2a3e4c")
    footer = "Keep power and USB connected."
    if state.endswith("-done"):
        footer = "Remove USB, then power-cycle."
    text(24, 259, footer, 21, "#f5f8fa", True)
    # The panel's scan order is portrait, rotated clockwise from logical output.
    native = image.transpose(Image.Transpose.ROTATE_270)
    raw = b"".join(
        struct.pack("<H", (r >> 3) << 11 | (g >> 2) << 5 | b >> 3) for r, g, b in native.getdata()
    )
    if len(raw) != 307200:
        raise ValueError("Unexpected native framebuffer size")
    return image, raw


def build(output):
    output.mkdir(parents=True, exist_ok=True)
    files = {}
    for state in STATES:
        image, raw = render(state)
        files[state + ".raw"] = raw
        (output / (state + ".raw")).write_bytes(raw)
        meta = PngImagePlugin.PngInfo()
        meta.add_text(
            "Description",
            "Finder Revival installer preview; rendered from "
            "tools/build_install_screens.py using project Atkinson fonts. No device capture.",
        )
        image.save(output / (state + ".png"), pnginfo=meta)
    return files


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "installer/build/status-screens")
    build(parser.parse_args().output)

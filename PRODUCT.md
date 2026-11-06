# Finder Revival

## Platform

Native embedded Linux touchscreen (Qt 4.8.6/QWS) plus a printer-hosted browser
dashboard. ARM926/ARMv5TE soft-float, 64 MiB RAM, Linux 3.10.32+, glibc 2.18.
This is not an iOS/Android app; web-only design tooling must not reinterpret it.

## Purpose

Modernize the exact supported Linux Finder while preserving its native printer
control, identity, calibration and stored files. Replace abandoned cloud/tooling
dependencies with local operation. docs/compatibility.md defines the exact profile.

## Current behavior

Themed native home and subscreens, neutral maintenance copy, GX previews, printer-
hosted dashboard, Orca metadata/thumbnail conversion, local file listing/deletion,
camera, per-owner SSH, optional splash and bounded RAM logging. Cloud/update
controls are intentionally deprecated; installation disables their switches while
preserving account/registration data. Network protection has explicit limits.

## Constraints and evidence

480x320 logical display; 320x480 RGB565 framebuffer. Stock GUI/control share a
process, so hooks are pinned and guarded. Do not change motion/planning implicitly.
One physical printer was tested; community installation is experimental. Real
printing/heating/motion and packaged-install acceptance are separate gates.
docs/validation.md distinguishes hardware, fixtures and unresolved work.

## Design

Preserve root DESIGN.md and the current dark Finder blue/orange visual system.
Status must reflect real state. Keep touch controls clear, plain and consistent.
Use the original approved bootscreen lettering for the header subtitle.

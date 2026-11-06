# Compatibility

Only one exact baseline is supported. Similar names are insufficient.

**Before installing, the printer must already run stock firmware
2.8.4.366 F2.12, build 20210823 (23 August 2021).** Check the full version in
About, then verify the executable hash below. Revival patches addresses in that
specific stock executable; it is not a version-independent firmware replacement.
An older/newer/different binary must not be forced through the guards.

The official [Finder downloads page](https://eu.flashforge.com/en-eu/blogs/download-software/finder-series)
currently lists `Firmware_2.8.4`. That listing covers multiple Finder families;
the exact full build and hash here determine compatibility, not whichever file
is described as “latest.” Bringing an older supported printer to the correct
stock baseline is a separate vendor-firmware operation before Revival installation.
Do not use an arbitrary Finder-family update or bypass compatibility checks.

| Field | Observed baseline |
| --- | --- |
| Physical PCB | Finder Plus V2.0.2; About says board 2.X |
| Machine family | Linux-based Finder Plus / Finder V2 |
| Stock GUI firmware | 2.8.4.366 F2.12, dated 20210823 |
| Actual kernel | Linux 3.10.32+; About's “Linux Kernel 1.0.366” is a package version |
| CPU/runtime | ARM926EJ-S, ARMv5TE, soft-float, glibc 2.18 |
| RAM | 64 MiB physical; roughly 58 MiB usable |
| Display | 480×320 logical; framebuffer 320×480 RGB565 |
| Storage | Approximately 7.28 GiB eMMC; FAT boot + ext4 root/data |
| Control | Vendor GUI/controller retained; raw TCP 8899 |

Stock `/PROGRAM/software/2.8.4/finder-plus-arm` SHA256:

```text
aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375
```

Stock kernel SHA256:

```text
ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87
```

The adapter contains addresses/instruction guards for that ELF. Another binary
is unsupported even if the version string looks similar. Do not remove guards
to force installation; add a separately audited profile.

The packaged installer accepts the known stock cached kernel or the approved
logo-only kernel hash recorded in the profile. Different custom artwork/kernel
bytes require an explicitly audited profile rather than bypassing that check.

Original Finder, Finder Lite, Finder 3, Adventurer, Guider and other models are
not covered. A board described as “NAND” or “V2” online is not proof of this eMMC
layout. Check `/proc/partitions`, PCB markings and actual firmware hashes.

`profiles/finder-plus-2.8.4.json` records the baseline. Common software hashes
are not owner identifiers; no printer serial, CID or filesystem UUID is embedded.

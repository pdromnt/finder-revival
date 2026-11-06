# Identity preservation

The release is an allowlisted overlay. **Never distribute or install the
development printer's full rootfs/data/eMMC image.** It would clone identity,
accounts, keys, settings and potentially old filesystem defects.

| Category | Behavior |
| --- | --- |
| Serial, registration, machine identity | Preserve existing bytes; read live via stock M115 |
| Calibration, offsets, motion-controller firmware | Preserve; no controller flash or factory reset |
| Wi-Fi profiles, MAC, network settings | Preserve; local-only routing policy is separate |
| Print files | Preserve; installation does not clear `/data` |
| Cloud account/PIN/registration credentials | Preserve; never publish or clear for deprecation |
| Flashforge Cloud / Polar switch files | Save externally, explicitly set to `OFF` |
| Dashboard key | Preserve on upgrade; generate fresh entropy on first install |
| SSH authorization | First setup uses documented root password or an owner public key; preserve existing configuration on upgrades |
| Root password | Initialize `flashforge_revival` only for first password-based SSH setup; retain later owner changes and save the previous root record externally |
| SSH host identity | Preserve existing Revival setup; generate a unique key for a new install |
| UI/dashboard/startup | Replace only named Revival files; save rollback externally |
| Splash | Patch the target's hash-verified kernel; save both originals externally and verify both installed copies |

No default `/opt/flashforge`, `/etc`, `/data` or `/PROGRAM` tree from somebody
else's dump belongs in a release. The deliberate vendor configuration changes
are one launcher field in `ffstartup.cfg` and two cloud switch files. First-time
password-based SSH setup also initializes the root password; supplying an owner
public key selects key-only setup without changing that password.

The prior root record is saved privately as `install/ssh-root-before.private`
when password setup is initialized. The full account database is never included
in a generic release. Managed-file rollback retains the current root password,
including later owner changes, just as it retains owner keys.

On the baseline, `/opt/flashforge/cloud-ff` and `cloud-mymini` store plain-text
`OFF\r\n`. Account/registration files are separate and remain untouched.

Before/after checks compare M115 and protected configuration hashes privately.
Do not paste real serials, credentials, Wi-Fi profiles or registration data into
public issues. Even hashes may identify a device; share only what is needed.

These preservation rules describe Revival installation/updates. The vendor's
deliberate **Factory Reset** changes the display name and clears registration,
among other settings. Its code does not directly remove Revival keys/programs;
see [Factory Reset](factory-reset.md) for the audited scope, completed one-printer
reset/power-cycle test and remaining coverage limits.

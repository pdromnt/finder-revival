# Validation and release gates

## Demonstrated on one physical Finder

* USB startup-hook execution, temporary root SSH and verified eMMC acquisition.
* USB-only CHECK without UART: guarded init re-exec, selective userspace shutdown,
  RAM pivot and unmount of root/data, followed by complete read-only offline
  checks of both ext4 filesystems, each exiting 0. The operator saw Checks passed
  after USB unmount, then power-cycled without the stick. Normal services returned;
  all 45 protected baseline files matched, both stored ext4 error counts remained
  zero and the log directory remained valid and empty. The RAM root used about
  5.2 MiB of its 16 MiB cap. No overlay was installed by CHECK.
* Packaged USB-only INSTALL and a subsequent normal power cycle passed on the
  development printer. All 27 payload files verified; 32 existing
  managed files were saved externally and 33 existed afterward. The rollback
  archive's bytes matched the pre-install baseline. Live checks matched all 46
  expected protected/payload files, retaining identity, calibration, Wi-Fi and
  owner credentials, print content and control/library bytes. UI/dashboard and
  cloud/update/network guards loaded; RAM logging remained bounded and both
  stored ext4 error counts stayed zero. This trial exercised packaged
  upgrade/reinstallation. A live camera image
  was unavailable during this trial; earlier camera validation is separate.
* USB-only ROLLBACK and a subsequent normal power cycle passed for that install.
  The pre-rollback archive verified all 33 installed managed files; rollback
  restored the previous 32 and removed the one introduced checksum manifest.
  Both unmounted filesystem checks passed afterward. All 45 protected baseline
  files matched byte-for-byte after normal boot, including identity, calibration,
  Wi-Fi, keys and print content. UI/dashboard, cloud/update/network guards and
  capped RAM logging remained active; the log directory was valid and empty,
  with zero stored filesystem errors. PID 1 had its original instructions and
  no maintenance bind remained. This restored the previous Revival setup.
* UART/Pico U-Boot recovery after an unbootable splash kernel; original kernel
  restored and internal-storage boot recovered.
* Corrected splash RAM boot matched the intended framebuffer; persistent SSH
  and splash subsequently booted without the USB stick.
* Themed home/subscreens/dialogs, status icons, native file preview and manual
  menu navigation. Inert fixtures covered hazardous/printing screens separately.
* Orca-style upload/conversion to GX with original G-code bytes retained; the
  owner saw its thumbnail and estimate on the stock touchscreen.
* A real user-sliced model uploaded from Orca, converted to valid GX, and appeared
  in the embedded Device tab with its selected preview and matching estimate.
  The owner also confirmed the list thumbnail appeared. The embedded-header
  regression was corrected and verified in both ordinary and injected-fetch
  contexts.
* Printer-hosted dashboard, file identity/deletion guards, camera hotplug and
  a valid 640×480 JPEG, identity footer, dark/light/system appearance.
* A physical cold boot of the combined setup before storage repair.
* Logging patch with stock FD→`/dev/null`, capped RAM diagnostics, persistent
  log directory empty under repeated status queries.
* Both filesystems repaired unmounted in RAM, first rehearsed on PC clones;
  final unmounted checks exit 0, protected file hashes unchanged, normal reboot
  with zero stored ext4 errors and working UI/dashboard/camera.
* IPv4 reject-route trial on hardware: off-LAN traffic returned “Network is
  unreachable” while LAN SSH, TCP8899 status and dashboard requests worked;
  trial rollback removed only the installed reject routes.
* Cloud queue/update menu removal captured through safe live navigation,
  with retained Settings actions over two pages. The guarded cloud/update
  entry-point suppression loaded on hardware; full reconnect/boot policy
  acceptance remains a separate check below.
* A subsequent controlled normal restart brought up the installed WAN reject
  routes automatically before network startup; LAN access returned. Production
  validation checks both patched ARM return instructions, absence of the audit
  library, retained identity/file IDs, camera and zero recorded filesystem errors.
  This software-restart evidence is recorded separately from physical cold boots.
* A later operator-performed cold boot after storage repair retained zero root
  and data error counters, no ext4/I/O kernel error indicators, and a valid empty
  `/data/log` directory: correct dot entries, record lengths and link count.
  The 256 KiB RAM log cap remained mounted. This is an online recurrence check,
  not a fresh offline consistency check or proof that the original cause is fixed.
* An operator-performed Factory Reset and subsequent power cycle passed immediate
  and new-boot checks. Revival, SSH host identity/owner authorization, dashboard
  key, serial and calibration survived. Cloud/update guards, WAN reject routes,
  camera and capped RAM logging remained active. Vendor checksum flags stayed
  absent; protected files and on-disk control/library bytes matched across boot.
  Root/data error counts remained zero and the log directory remained valid and
  empty. Earlier test jobs had been deleted by the owner before this test; the
  remaining job listing/identity matched across boot, but no byte-for-byte
  pre-reset job-content comparison was made. See
  [reset behavior and limits](factory-reset.md).

## Local / simulated evidence

The combined rc.1 installer adds first-setup password SSH and two-file splash
installation/rollback to the previously exercised overlay workflow. Forty local
unit tests cover fresh/upgrade credential preservation, kernel staging/rollback,
unknown inputs, corrupt staging and failed rename cases. The freestanding ARM
converter also ran on the development printer in a **20 MiB-capped tmpfs**:
the stock image converted to the exact approved, previously boot-tested splash
hash. Its scratch files were removed and the RAM mount released; this trial did
not write internal storage. Password hashing was verified with the printer's own
libcrypt. These converter/account checks are separate from an end-to-end physical
trial of the newly combined INSTALL/ROLLBACK package.

BusyBox 1.24.1 was rehearsed in an isolated PID namespace: configuration reload
acknowledgement preceded PID 1 re-exec; the temporary bind was removed and
the original inittab remained unchanged. File fixtures exercise fresh/upgrade
install/rollback and refuse wrong-device, edited-file, corrupt-archive and
symlink cases. These do not qualify the physical USB-only path.

The volatile init guard has host fixtures for mismatched instruction contexts,
probe-only operation, partial writes, readback failure and restoration failure.
Its ARM build has no interpreter or external runtime symbols; a read-only probe
matched the actual PID 1 instructions on the development printer. A separate
fixture checks that selective shutdown excludes PID 1 and zero-address-space
workers. These fixtures supplement the physical CHECK above; they do not
establish packaged install/rollback acceptance on their own.

Installer status screens are PC-rendered and checked for native RGB565 size,
rotation and quantization. Linux shell fixtures inject prerequisite/write/cleanup
failures and termination into the actual installer exit/status functions; they
check failure classification, cleanup, exit codes and no false completion.
Framebuffer fixtures cover layout/size rejection and console fallback. These
are isolated fixtures, not an executed eMMC installation or live display test.

The backend has 18 fake-printer integration groups covering readiness/movement,
unavailable/busy states, uploads/cancellation/replacement, stale identities,
path/key checks, conversion limits, camera framing/exclusion, print-command
acknowledgements and notifications. Earlier parity builds passed ASan/UBSan;
each new build must be tested again before inheriting that claim.

Logger tests cover exact noise matches, retained nonmatching errors/fatal
severity, legacy export binding, double slashes and capped rotation.
Sparse transport fixtures compare reconstructed output against ordinary
e2image and check ARM framing. Build/fixture success is not device acceptance.
Runtime-source packaging verifies exact toolchain/source identities and checks
that both application source rebuilding and relinking from the supplied inputs
reproduce the distributed ARM backend hash. Upstream runtime/toolchain rebuilding
from its supplied source sets has not been rehearsed.

Factory Reset's supported touchscreen handler and file-writing helpers were
also inspected separately from the later hardware test. The audit defines its
named mutations and conditional branches; the physical result above establishes
the tested development printer's reset/power-cycle behavior, not every branch.

## Release handoff

Generate the final versioned GitHub Release handoff from the reviewed source:
generic USB package, matching runtime-source companion, checksums, manifest and
release notes. Review archive contents for private data and name remaining
limitations. See [release handoff](development.md#release-handoff).
The existing development packages are build evidence, not this final handoff.

## Additional coverage before generalizing

* Deliberate Wi-Fi reconnect, DHCP/static-address transitions and routed LAN
  clients under the cloud/update/network policy.
* A second supported printer, including one previously cloud-enabled, if a
  community tester becomes available. This is broader compatibility coverage,
  not a requirement to obtain another printer for the development unit's checks.
* Longer-term storage observations.
* Reset with active cloud clients/hotspot and a pre-reset job-content baseline;
  the completed reset used cloud switches OFF and connected Wi-Fi client mode.

No claim of universal Finder compatibility, completely eliminated attack
surface, fixed root cause of corruption, or guaranteed recovery is warranted.
Release notes must name the exact artifacts and distinguish these evidence levels.

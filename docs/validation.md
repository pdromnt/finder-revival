# Validation and release gates

## Demonstrated on one physical Finder

* USB startup-hook execution, temporary root SSH and verified eMMC acquisition.
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
  contexts. Physical printing of this model remains pending.
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

## Local / simulated evidence

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
inspected without invoking the reset. Its named mutations do not directly target
Revival programs, startup selection or owner keys. This is a code/file-footprint
audit, not live reset or post-reset reboot acceptance. See [reset scope](factory-reset.md).

## Remaining release-candidate work on this printer

1. Packaged USB bootstrap and overlay installation on a clean supported target.
2. Upgrade and rollback with identity/calibration/credentials demonstrably intact.
3. Complete the tested Orca slice → upload → G-code/GX conversion → dashboard
   preview path with native touchscreen inspection and a supervised physical
   print. This includes actual heating/motion;
   READY and an acknowledged start are insufficient.
4. Test the authorized stock Factory Reset and subsequent normal boot. Confirm
   the audited settings changes, retained Revival installation/owner access and
   behavior after vendor checksum-flag deletion. See [reset scope](factory-reset.md).
5. Generate the final versioned GitHub Release handoff after recording these
   results: generic USB package, matching runtime-source companion, checksums,
   manifest and release notes. Review archive contents for private data and name
   remaining limitations. See [release handoff](development.md#release-handoff).
   The existing development packages are build evidence, not this final handoff.

## Additional coverage before generalizing

* Deliberate Wi-Fi reconnect, DHCP/static-address transitions and routed LAN
  clients under the cloud/update/network policy.
* A second supported printer, including one previously cloud-enabled, if a
  community tester becomes available. This is broader compatibility coverage,
  not a requirement to obtain another printer for the development unit's checks.
* Print interruption/recovery behavior and longer-term storage observations.

No claim of universal Finder compatibility, completely eliminated attack
surface, fixed root cause of corruption, or guaranteed recovery is warranted.
Release notes must name the exact artifacts and distinguish these evidence levels.

# Reverse-engineering notes

The stock executable retains useful C++ symbols. Much DWARF describes bundled
libraries; debug symbols do not imply original application source is available.
Use a locally extracted copy in Ghidra; do not publish the stock ELF/decompilation
dump as project source. Public helper scripts are under `tools/ghidra/`.

Selected addresses for the pinned baseline only:

| Symbol | Address / finding |
| --- | --- |
| `onShowMsg(QtMsgType,const char*)` | `0x1198fc`; stock Qt logging handler |
| `Log::saveLog` | `0xaa15c`; persistent writes and ~1 MiB rotation |
| `MainWindow::initDevice` | `0x1340f0`; creates local server independently of cloud switches |
| `myServer::serverSocketStart` | `0x1b1eec`; raw IPv4 TCP listener |
| `myServer::updateStatus` | `0x1b15f0`; accept/select/read/parse loop |
| `myServer::cmd_M27` | `0x1a882c`; cached progress out of 1000 |
| `MainWindow::updateProcessEmit` | `0x12b750`; cloud/update trigger path |
| `Setting::initData` | `0x154bb0`; menu action identities |

The stock TCP listener is separate from outbound Flashforge and Polar clients.
Hiding cloud controls originally preserved switch bytes; installation policy
now deliberately sets both switches OFF while retaining registration/account data.

## Lessons worth keeping

* Returning from an internal ARM function requires exact ABI/prologue guards
  and instruction-cache flushing. ELF file bytes remain untouched by RAM hooks.
* Qt pages delete themselves on Back. Caching live widget pointers across page
  transitions caused a reproduced SIGSEGV; resolve live objects instead.
* New toolchain headers may redirect `open`→`open64`, `sscanf`→C23, or clocks to
  time64 symbols absent from glibc 2.18. Bind/check the legacy ABI explicitly.
* A valid gzip/uImage CRC did not prevent the first splash image from failing.
  Preserve fixed-position gzip CRC/inflated-size trailer words and RAM-test.
* Stopping only the UI can trigger its launcher's stock fallback while another
  supervisor starts a second UI. Stop the launcher/supervisor as a unit, idle only.
* Old DHCP helpers can retain deleted preload-library mappings after live file
  replacement, keeping root busy for a read-only remount. Prefer offline updates;
  a controlled shutdown must terminate/reap those helpers too. Do not force fsck.
* Sparse core logical sizes exaggerate allocated flash usage. Measure both.

## Primary references

* [flashforge-toolkit](https://github.com/pdromnt/flashforge-toolkit): prior protocol,
  Orca metadata/thumbnail conversion and dashboard behavior; not a required bridge.
* [moonglow firmware tool](https://github.com/moonglow/flashforge_firmware_tool):
  decrypt/encrypt utility listing Finder; not proof of Linux-board package compatibility.
* [Historical Finder protocol capture](https://github.com/ztripez/flashforgefinder-protocol/blob/master/wireshark/connect-disconnect.pcapng):
  an old client repeatedly queried status about once per second; exact FlashPrint
  version was not established. No captured commands were replayed onto the printer.
* [Linux ext4 superblock fields](https://www.kernel.org/doc/html/latest/filesystems/ext4/super.html).
* [e2fsck manual](https://man7.org/linux/man-pages/man8/e2fsck.8.html).
* [eMMC health register interpretation](https://developer.toradex.com/software/linux-resources/linux-features/emmc-linux/).

The development printer's full images, accounts, serial, Ghidra databases,
screenshots showing identifiers and raw crash dumps remain private.

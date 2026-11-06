# OrcaSlicer files for Finder 1 / 2 / Plus

The three original supplied files are copied here **unchanged**:

* [Flashforge Finder.orca_printer](<Flashforge Finder.orca_printer>) — printer
  preset plus optional **Generic PLA @ Finder AllMetal** filament preset.
* [Printable Bed Texture.png](<Printable Bed Texture.png>) — bed texture by pdromnt.
* [Printable Bed Model.stl](<Printable Bed Model.stl>) — Hydrahimera's bed
  visualization, not a part to print. See [credits/license](ATTRIBUTION.md).

## Compatibility and dependencies

The slicer profile is for **classic Finder 1, Finder 2 / V2 and Finder Plus only**,
with a 0.4 mm nozzle. This does not expand Revival firmware compatibility.

It is based on **Flashforge Guider 2s 0.4 nozzle**, with bed size and start/end
G-code changes for the Finder. **Enable/select that Guider 2s system preset in
Orca before importing.** The inherited Guider 2s identifier is retained.

The optional PLA preset is **for AllMetal-modified hotends**, not a stock-hotend
PLA recommendation. It inherits `Generic PLA @Flashforge G3U`, which must be
available in your Orca installation. It supplies 190 °C nozzle temperature,
0.5 mm retraction and 0.4 mm Z hop; other settings inherit from that parent.
No process preset is included. No tuning was changed or flattened for this copy.

## Import and bed assets

1. Enable the **Flashforge Guider 2s 0.4 nozzle** system preset first.
2. Import **Flashforge Finder.orca_printer** through
   **File → Import → Preset Configs**, then select **Flashforge Finder**.
3. **Keep the texture and model in a stable, permanent folder on the PC before
   selecting them in Orca.** Orca remembers their file paths. Do not move, rename
   or delete them afterward; if you do, reselect the new paths in Orca. A temporary
   extraction/download folder that you later clean up will break those references.
4. In the printer's printable-area/bed-shape settings, choose
   **Printable Bed Texture.png** as the texture and **Printable Bed Model.stl**
   as the model, then save the preset.
5. Configure your own OctoPrint-compatible host URL/API key using the dashboard's
   Slicer setup. The export does not configure another owner's connection/serial.
6. Choose the AllMetal PLA preset only for a suitable modified Finder. Review the
   slice and preview before a supervised print.

The supplied export already has no `bundle_structure.json`, so its presets
import as ordinary user presets rather than a bundle-ID subfolder. Its fresh
import was tested by the owner. Slicing/upload/GX conversion and previews were
also tested; physical printing with the Revival setup remains pending.

The exported bed coordinates are X **−90…51.3 mm**, Y **−70…71.3 mm**, with
height **140 mm**. Orca's rectangular dialog represents those as size
**141.3 × 141.3 mm**, origin **X 90 / Y 70 mm**. These are supplied profile values,
not newly measured travel limits.

These files remain on the PC. Nothing here is added to the printer's firmware
payload. Original project contributions use Unlicense; the bed STL retains
**CC BY-NC-ND 4.0**. See [attribution](ATTRIBUTION.md).

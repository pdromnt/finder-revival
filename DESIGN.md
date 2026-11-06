---
name: Finder Revival home prototype
description: Native Qt 4/QWS visual system captured from the temporary Finder home build.
colors:
  primary: "#007bb5"
  secondary: "#1d2c39"
  background: "#101820"
  foreground: "#f5f8fa"
  subtitle: "#ffad60"
  border: "#3e5769"
  divider: "#2a3e4c"
  primary-pressed: "#006a9e"
  secondary-pressed: "#30485b"
  border-pressed: "#7292a9"
  foreground-disabled: "#9bb0be"
typography:
  title:
    fontFamily: "Atkinson Hyperlegible"
    fontSize: "24px"
  label:
    fontFamily: "Atkinson Hyperlegible"
    fontSize: "24px"
  subtitle:
    fontFamily: "Atkinson Hyperlegible"
    fontSize: "13px"
rounded:
  control: "12px"
spacing:
  outer: "16px"
  task-gap: "12px"
  control-padding: "10px"
components:
  task-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.foreground}"
    typography: "{typography.label}"
    rounded: "{rounded.control}"
    padding: "{spacing.control-padding}"
    width: "448px"
    height: "130px"
  task-primary-pressed:
    backgroundColor: "{colors.primary-pressed}"
    textColor: "{colors.foreground}"
  task-secondary:
    backgroundColor: "{colors.secondary}"
    textColor: "{colors.foreground}"
    typography: "{typography.label}"
    rounded: "{rounded.control}"
    padding: "{spacing.control-padding}"
    width: "218px"
    height: "98px"
  task-secondary-pressed:
    backgroundColor: "{colors.secondary-pressed}"
    textColor: "{colors.foreground}"
  task-disabled:
    backgroundColor: "{colors.secondary}"
    textColor: "{colors.foreground-disabled}"
  brand:
    textColor: "{colors.foreground}"
    typography: "{typography.title}"
  subtitle:
    textColor: "{colors.subtitle}"
    typography: "{typography.subtitle}"
---

# Design System: Finder Revival home prototype

## Overview

**Creative North Star: "Revival"**

The approved Revival identity uses a dark blue-black field, blue task emphasis,
orange identity detail, and clear light lettering. Flat native controls give a
small physical touchscreen an obvious task hierarchy with generous hit areas.

This records the built, temporary home prototype only. Its authority is
`ui-prototype/theme.conf`, the native adapter in
`ui-prototype/native/revival_ui.c`, and the actual framebuffer capture
`.impeccable/review/finder-home.png`. The selected Print dock sketch at
`.impeccable/mocks/decision/print-dock.html` explains the chosen composition;
native output determines the recorded values. The user tested navigation and
reported working flows with about the same responsiveness. Independent finish
review approved this bounded temporary home prototype without material fixes.
That is neither a whole-printer review nor a performance-release verdict.

**Key Characteristics:**

- Fixed physical-screen geometry.
- Flat tonal hierarchy with one blue primary task.
- Readable Atkinson lettering and familiar stock pictograms.
- Native stock task flows behind the restyled home.

## Colors

Cool dark surfaces carry light task labels, with blue reserved for the primary
home action and orange used for the project subtitle. Frontmatter values are
normative; state colors come directly from the Qt stylesheets.

### Primary

- **Finder Blue** (`primary`): Print files fill and its resting border.
- **Pressed Finder Blue** (`primary-pressed`): primary press feedback.

### Secondary

- **Revival Orange** (`subtitle`): the small Revival Project identity line.

### Neutral

- **Blue-black Field** (`background`): home background.
- **Deep Slate** (`secondary`): Preheat and Tools surfaces; disabled task fill.
- **Cool White** (`foreground`): brand and task lettering.
- **Slate Edge** (`border`): resting secondary and disabled borders.
- **Header Seam** (`divider`): the thin header separator.
- **Pressed Slate** (`secondary-pressed`): secondary press feedback.
- **Light Slate Edge** (`border-pressed`): pressed secondary border.
- **Muted Slate Text** (`foreground-disabled`): disabled task labels.

**The Task Emphasis Rule.** On this home, Print files carries the blue fill;
the two supporting tasks share the secondary surface.

## Typography

Atkinson Hyperlegible is registered through Qt's application font database.
Static Regular and Bold files and their SIL Open Font License are retained at
`assets/fonts/atkinson/`. Upstream is
`https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegible`.

The task-label and brand roles share the large size; the subtitle is smaller.
The current stylesheets do not specify weight, tracking, or line height. Bold
is available but is not a separately styled role in this home. Do not infer a
web font stack or a display/body type scale from the decision sketch.

## Layout

This is a fixed logical canvas (480 x 320), rendered to the native portrait
framebuffer (320 x 480 RGB565) through the existing Qt/QWS display path. There
are no desktop/mobile breakpoints or responsive browser targets.

The Print dock home has a header (48 px) and an outer task inset from the
spacing tokens. Preheat is at (16,64), Tools at (246,64), and Print files at
(16,174). Their sizes are recorded in the component tokens. The paired tasks
and the primary task are separated by the task-gap token. Everything fits in
one view without scrolling.

The brand occupies (16,0,100,30); the subtitle occupies (118,0,108,30). Existing
status indicators remain at the top right. The divider spans (0,47,480,1).
These are measurements of this home composition, not a proposed layout for
unbuilt screens. Fonts and geometry apply only to home; secondary screens
retain their stock appearance.

## Elevation & Depth

No shadows or gradients are introduced. Filled surfaces, restrained borders,
and the header seam establish hierarchy. Press feedback is an immediate color
change; there are no added animations or easing tokens.

## Shapes

Task controls use gently rounded rectangular corners from `rounded.control`
and thin solid borders (1 px). Icon boxes are square (48 x 48). The header
separator is a plain horizontal line. Geometry is expressed in device logical
pixels, not density-independent mobile units.

## Components

### Task controls

The native QToolButtons retain their original signal connections: Print files
opens the stock Build flow; Preheat and Tools open their stock flows. Icons
sit beside their labels using Qt's text-beside-icon mode. The internal control
padding is captured in frontmatter; Qt owns the actual icon/text arrangement.

Primary, secondary, pressed, and disabled fills are tokenized above. Secondary
resting and disabled borders use Slate Edge; a secondary press uses Light
Slate Edge. Primary borders follow the current primary fill. These are native
touch controls with no added hover animation or custom keyboard-focus style.
The existing 300 ms repeat-click gate is retained.

### Identity and status

FINDER and Revival Project are native QLabels over a transparent background.
The adapter makes the new labels transparent to mouse events. Original
connection/status icons retain their existing behavior and resource origin;
no fabricated telemetry or new status semantics are introduced.

### Asset provenance

Task pictograms and status icons remain loaded by the original Qt resource
system. The extraction inventory is `research/ui-resources/manifest.json`,
whose entries retain resource paths, ELF offsets, and individual SHA256 hashes.
The original ELF SHA256 is
`aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375`.
There are no new shipping raster assets in this home prototype. Browser
decision sketches and sidecar component previews are documentation aids;
they are not a web implementation or replacement asset pipeline.
The sidecar's synthesized tonal strips are panel-only illustrations, not
additional colors used by the printer. Its text-only previews embed the
licensed Regular font and omit original native resource pictograms.

## Do's and Don'ts

### Do:

- **Do** preserve the recorded native home tokens and physical-screen geometry.
- **Do** keep original task handlers, stock pictograms, and status semantics.
- **Do** retain font license and resource provenance with the prototype.
- **Do** verify visual and navigation changes on the actual printer screen.

### Don't:

- **Don't** apply this home-only styling to secondary screens without separate work.
- **Don't** treat browser previews as device-runtime evidence.
- **Don't** claim responsiveness improved: timings show stock-handler delays and a long timer, and the user reports about the same feel.
- **Don't** describe this temporary /run prototype, with automatic rollback, as a persistent UI installation or a validated printer-wide release.

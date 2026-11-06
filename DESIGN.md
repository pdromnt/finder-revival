---
name: Finder Revival native touchscreen
description: Native Qt 4/QWS visual system and installed touchscreen source references.
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
    asset: "assets/bootscreen/revival-v3-flat.png"
    sourceRect: "112,216,256,24"
    displaySize: "216x20"
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

# Design System: Finder Revival native touchscreen

## Overview

**Creative North Star: "Revival"**

The approved Revival identity uses a dark blue-black field, blue task emphasis,
orange identity detail, and clear light lettering. Flat native controls give a
small physical touchscreen an obvious task hierarchy with generous hit areas.

The installed home is defined by [theme.conf](ui-prototype/theme.conf) and
the [native adapter](ui-prototype/native/revival_ui.c). Subscreens and dialogs
are defined by [flow.conf](ui-prototype/flow.conf) and
[flow_skin.c](ui-prototype/native/flow_skin.c). The historical `ui-prototype`
directory name is retained for stable build paths. Published hardware captures
are in [docs/images](docs/images/README.md); installation and navigation evidence
is recorded in [validation](docs/validation.md).

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

Task labels and the brand use 24 px type; FINDER explicitly uses weight 700.
The Revival Project subtitle reuses the approved bootscreen lettering as a
cropped pixmap, rather than approximating it with another font. Its source
rectangle and displayed size are recorded above. These are native Qt roles;
the dashboard has its own browser typography.

## Layout

This is a fixed logical canvas (480 x 320), rendered to the native portrait
framebuffer (320 x 480 RGB565) through the existing Qt/QWS display path. There
are no desktop/mobile breakpoints or responsive browser targets.

The Print dock home has a header (48 px) and an outer task inset from the
spacing tokens. Preheat is at (16,64), Tools at (246,64), and Print files at
(16,174). Their sizes are recorded in the component tokens. The paired tasks
and the primary task are separated by the task-gap token. Everything fits in
one view without scrolling.

The brand occupies (16,0,100,48); the subtitle occupies (102,19,216,20). Existing
status indicators remain at the top right. The divider spans (0,47,480,1).
These coordinates describe the home composition. Secondary screens use their
own geometry and shared styling from `flow.conf` and `flow_skin.c`, retaining
their original printer actions.

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
system. Private extraction inventories retain resource paths, ELF offsets and
individual SHA256 hashes; vendor resource dumps are not redistributed.
The original ELF SHA256 is
`aa73d2932cd13aff93124d82c2ac304b41adc7d52fd206cc23b874cdbe065375`.
The subtitle artwork comes from the approved
[bootscreen asset](assets/bootscreen/revival-v3-flat.png), with
[provenance](assets/bootscreen/PROVENANCE.md). Browser sketches are design aids;
the installed Qt output and source configurations determine the native UI.

## Do's and Don'ts

### Do:

- **Do** preserve the recorded native home tokens and physical-screen geometry.
- **Do** keep original task handlers, stock pictograms, and status semantics.
- **Do** retain font license and resource provenance with the installed assets.
- **Do** verify visual and navigation changes on the actual printer screen.

### Don't:

- **Don't** copy home geometry onto secondary screens; use their own flow rules.
- **Don't** treat browser previews as device-runtime evidence.
- **Don't** claim responsiveness improved: timings show stock-handler delays and a long timer, and the user reports about the same feel.
- **Don't** equate a new preview or RAM trial with an installed, hardware-tested change.

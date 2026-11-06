# Touchscreen flow and design boundaries

The existing root DESIGN.md remains the visual authority: dark #101820,
slate #1d2c39, Finder blue #007bb5, orange #ffad60, Atkinson Hyperlegible,
24 px primary tasks and 12 px control corners. No new design world is introduced.

Home uses a Print dock with Tools and Preheat retained. Local Print files now
offers printer storage and USB; Cloud queue is hidden. Settings removes only
cloud action IDs 3/4 and update action ID 17 from the owned native list,
recomputing pages through normal QList ownership. Other actions keep their IDs.

The 48 px header aligns real Wi-Fi/temperature indicators. The subtitle crops
the actual bootscreen lettering, matching the approved compact lockup. Child
screens mirror native status rather than forcing connected/ready values.
Existing pages delete themselves on Back: resolve live widgets, never cache them.

The full 480x320 keyboard grid and native printing/pause/resume visibility remain.
Homing uses neutral waiting/completion copy. Active print/heater/motion routes
were reviewed and previewed with inert fixtures; fixture captures are not proof
of a successful physical operation. See ../docs/validation.md.

The two retained source choices fill the previous cloud row's space. The Back
control remains separate. Saved cloud state is changed only by explicit install
policy, never by merely rendering a menu. ../docs/security.md explains the layers.

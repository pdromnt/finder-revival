# Backend fixtures

`integration.py` runs against disposable storage, a fake TCP printer and a fake
camera on loopback test ports. It must not be pointed at real motion/heater
hardware. Fake acknowledgements are not evidence of a completed physical print.

The small `.gcode` files are metadata/upload fixtures, not recommended printable
jobs. Use a real sliced model for printing. Never print a fixture simply because
it has a GX thumbnail/estimate.

Run `node web-service/tests/orca_headers.cjs` on the PC to exercise the actual
dashboard API helper with ordinary fetch and Orca's API-key injection. The
credential must remain a single value after HTTP header-name normalization;
mutation identity headers must survive too. This test never contacts a printer.

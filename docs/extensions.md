# Possible extensions

Ideas once the core sniff → store → blast loop works. None are required for the
acceptance criteria.

- **One button per device function.** Define a template button per TV / soundbar /
  AC function once the codes are captured.
- **Scenes / macros.** Add Home Assistant scripts or scenes that chain multiple
  transmits (e.g. "Movie mode" = TV on + input + soundbar on + dim lights).
- **Web-server macros.** Expose macros via the built-in web server for phone control
  without the Home Assistant app.
- **Aim, don't add.** If range to a specific device is marginal, reorient the 5990
  (it has one vertical and one horizontal LED) rather than adding a second blaster.
- **Pin a known-good ESPHome version** in `requirements.txt` once the build is
  confirmed, so future flashes are reproducible.

# Changelog

## Unreleased

<<<<<<< HEAD
- Attempts to run on any game or D2RLoader version.
- Crashes are written to the plugin's log.
=======
- Life and mana drain now go through the game's own regeneration (as negative regen), so the orb drains smoothly every
  frame. Life drain works now and never kills; the aura turns off when less than a second of drain is left.
- Reservation no longer lowers max life/mana. The current value is capped at max minus what is reserved, so the orb
  shows the reserved part empty and its text reads unreserved/max. Turning the aura off keeps the value you have
  instead of refilling the orb.
- New hook on the player regen tick (`0x42e600`, image `0x6ab3782c`); engine notes moved to
  `docs/reverse-engineering/better-auras.md`.
>>>>>>> 7ad78a7 (better-auras: drain through the game's regen, reservation as a cap on current life/mana)

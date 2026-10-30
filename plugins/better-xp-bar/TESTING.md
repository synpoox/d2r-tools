# In-game test checklist

Nothing in this plugin has run in the game before your first test. Each step says what to do, what you should see,
and which log lines to paste back. The plugin log is
`<game>\mods\<Mod>\d2rloader\logs\better-xp-bar.log` (mod-scoped install) or the game-wide
`<game>\d2rloader\logs\` folder (game-wide install). Every line starts with `better-xp-bar:`.

The console is D2RLoader's console (open it with its key; type the command and Enter).

## 0. Install

1. Download the `better-xp-bar` artifact from the latest green CI run of the monorepo (Actions > ci > Artifacts > better-xp-bar), unzip it.
2. Copy `better-xp-bar.dll` to `<game>\d2rloader\plugins\` (works with every mod) or to
   `<game>\mods\<Mod>\d2rloader\plugins\`.
3. Start the game through D2RLoader.

**Paste back:** every `better-xp-bar:` line of the log up to this point. Expected:

```
better-xp-bar: 0.1.0 loaded (mod ..., D2R 3.3.<build>, image 0x6ab3782c (expects 0x6ab3782c, match), file ...)
better-xp-bar: config: visible 1, anchor bottom-center, offset 0,-160, ...
better-xp-bar: envguard ok ...
better-xp-bar: xp on: 9 byte checks passed; game events registered
better-xp-bar: overlay on: overlay frame callback registered
better-xp-bar: hotkeys on: 2/2 actions (...)
```

If any part says `off: ...`, stop and paste the log: the rest depends on it.

## 0b. Native panel (part `hud`, findings (f))

1. Start the game; the log must show `hud on: 4 byte checks passed; panel better-xp-bar/XpPanel with 6 lines` and
   the generated layout.
2. Enter a game. Expected log: `hud: panel opened`, then `hud: native panel better-xp-bar/XpPanel found`.
3. The lines appear in D2R's font above the bottom centre. Open/close inventory, character screen, Esc menu; walk and
   click under the text (it must not block clicks).
4. `better-xp-bar` in the console: the `hud` line shows `panel open, native found, text sets N` with N growing as
   XP changes.

**Paste back:** the `hud` log lines, the console `hud` line, a screenshot, and whatever closed or moved the panel.

## 1. Display appears (needs live check: findings (a), (b), (d))

1. Enter a game with a character that is **not** level 1 (so the level percentage is visible).
2. Within a second a small box with 6 lines appears above the bottom-centre of the screen:
   `Level N`, `XP <total>  <percent>`, `Session 0  +0.00%`, `XP/h --  (avg --)`, `Time 00:00:0x`, `Next level --`.
3. Open the character screen (C). Compare **level** and **experience** with lines 1 and 2.
4. Hover the stock XP bar: its tooltip percentage should match line 2's percentage (same formula as the game).
5. Type `better-xp-bar` in the console.

**Paste back:** the log lines `first read in this game: ...` and `exp table: ...`, the console output of
`better-xp-bar`, the level, experience and XP-bar tooltip from the character screen, and a screenshot of the box.

## 2. XP updates on a kill

1. Kill one monster that gives XP.
2. Line 2 and line 3 change within half a second. `Session` shows `+<xp>` and a percentage.
3. After 60 s in the game, the `XP/h` line and `Next level` show numbers instead of `--`.

**Paste back:** `better-xp-bar show` output before and after the kill, and after 60 s.

## 3. Level-up carries over

1. Get a level-up (a low character is easiest).
2. Line 1 shows the new level; line 2's percentage restarts near 0 %; `Session` keeps counting (flat XP is the total
   gained, the percentage passes 100 % when more than a level was gained).

**Paste back:** the log lines `level up: now N ...` and `level changed event: A -> B`, and `better-xp-bar show`.

## 4. Death (XP loss)

1. In Nightmare or Hell, die (lose XP), then pick up the corpse.
2. `Session` goes down by the lost amount; `XP/h` goes down (and may become negative); `Next level` shows `--`
   while the rate is zero or negative. No crash.

**Paste back:** the log line `XP lost: ...` and `better-xp-bar show`.

## 5. Session reset (hotkey and command)

1. Press **Ctrl+R** (default; see Options > Controls > "Better XP Bar").
2. All session lines restart from 0 (`Session 0`, `Time 00:00:00`, rates `--`); level and XP stay.
3. Type `better-xp-bar reset` in the console: same result.
4. Press **Ctrl+X**: the box hides; again: it shows.

**Paste back:** the log lines `session (reset by the player): ...`, `session reset`, `display hidden`, `display shown`.

## 6. Quitting the game resets the session

1. Save and Exit to the main menu: the box disappears.
2. Enter a game again with the same character: `Session 0`, `Time` starts from 0.

**Paste back:** the log lines `game left`, `session (game left): ...`, then `game joined`, `local player ready`,
`first read in this game`.

## 7. Character switch

1. Exit, choose another character (different class, or same class with different XP), enter a game.
2. The box shows the new character's level and XP; session from 0.

**Paste back:** the `first read in this game` and `exp table` lines (a different class makes a new `exp table`
line).

## 8. Config

1. Close the game. Open `<game>\d2rloader\config\better-xp-bar.toml` (or the mod's `d2rloader\config\` folder).
2. Change `anchor = "top-right"`, `offset_y = 40`, `lines = ["level", "rate", "eta"]`, `window_seconds = 300`.
3. Start the game, enter a game: the box is at the top right with 3 lines. (`better-xp-bar reload` in the console
   also applies changes without a restart.)

**Paste back:** the `config: ...` log line and a screenshot.

## 9. Off-switch (only if something is wrong)

Create `better-xp-bar-off.txt` next to the DLL with the word(s) `overlay`, `hotkeys` or `xp`, restart, and send the
log. `xp` turns off all game reads.

## Live-check items this covers

| Item (docs/reverse-engineering/better-xp-bar.md) | Step |
|---|---|
| XP/level reads equal the character screen | 1 |
| Experience table values / level % equals the stock bar | 1, 3 |
| No assert or crash in town, wilderness, death, level-up, leaving | 1-6 |
| Overlay renders, position, look | 1, 8 |
| Session reset on leaving; character switch | 6, 7 |
| Stock XP bar widget name (optional anchoring) | not used yet |

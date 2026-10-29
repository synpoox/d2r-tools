# Metrics: definitions and decisions

Implemented in `core/` (pure C++, no game or Windows code) and covered by `tests/`.

| Line | Definition |
|---|---|
| Level | `L` = the last level whose start threshold is <= current XP (from the game's experience table) |
| XP, % | Total XP, and Level % = (xp - start[L]) / (start[L+1] - start[L]). At the maximum level: "--" |
| Session XP | (session = the current game by default) **Net**: current XP - XP at session start. Correct across level-ups because it uses totals. % = Position(now) - Position(start), Position = level + Level %; e.g. half of level 80 plus a quarter of level 81 = +75.00 % |
| XP/hour | Live: gains in the last *window* seconds of in-game time (default 600, configurable) / window. Avg: gains / in-game session time. % = level-equivalents gained per hour, x100. "--" until 60 s of in-game time exist |
| Time | In-game time of the session, hh:mm:ss |
| Next level | XP to the next level / live XP per second. "--" at a zero rate or at the maximum level |

## Decisions (please confirm or overrule)

1. **Session scope: per game** (decided). A session starts when you enter a game and is cleared when you leave it
   (back to the main menu / lobby); the next game starts a new session from its first XP read. The reset hotkey /
   command starts a new session at once. Time counts only while in the game. `session_scope = character` keeps one
   session per character across games instead (time between games not counted; resets on a character switch).
2. **Deaths.** XP/hour counts **gains only** (gross). A death's XP loss goes to its own counter (shown in the status
   command) and never lowers the rate, so the rate and the time-to-level can never be negative and averages stay
   meaningful. Session XP (flat and %) is **net**, so a loss is visible there and may go below zero. XP won back after
   a loss counts as a gain again.
3. **XP/hour %** uses level-equivalents (the same unit as Session %), not "% of the current level per hour". The two
   differ only around a level-up; tell me if you prefer the other.
4. **Rolling window edge.** The window is the half-open interval (now - window, now]: a gain exactly at the window
   start is outside. Before the session is one window long, the window is the session itself.
5. **First read of every game is a baseline.** A changed value between games (or a read before the character's stats
   arrive) is never counted as gained or lost. The plugin only reads XP after D2RLoader's `LocalPlayerReady` event.
6. **Thresholds are never in the code.** The plugin reads them from the running game's experience table at run time;
   the tests use synthetic tables.

## Flagged in the brief

- *"Show '--' until 60 s of data exists"* is applied to both rates, and to the time-to-level that depends on them.
- *"ETA = remaining / XP-per-hour"*: with the live rate the ETA jumps around after idle time (it shows "--" once the
  window holds no gains). `eta_from_session_rate = true` uses the session average instead.

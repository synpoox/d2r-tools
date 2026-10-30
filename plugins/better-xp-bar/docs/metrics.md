# Metrics: definitions and decisions

Implemented in `core/` (pure C++, no game or Windows code) and covered by `tests/`.

| Line | Definition |
|---|---|
| Level | `L` = the last level whose start threshold is <= current XP (from the game's experience table) |
| XP, % | Total XP, and Level % = (xp - start[L]) / (start[L+1] - start[L]). At the maximum level: "--" |
| Session XP | (session = the current game by default) **Net**: current XP - XP at session start. Correct across level-ups because it uses totals. % = Position(now) - Position(start), Position = level + Level %; e.g. half of level 80 plus a quarter of level 81 = +75.00 % |
| XP/hour | **Net** (a death's loss lowers it; it can go negative). Live: net XP change in the last *window* seconds of in-game time (default 600, configurable) / window. Avg: net change / in-game session time. % = net level-equivalents per hour, x100. "--" until 60 s of in-game time exist |
| Time | In-game time of the session, hh:mm:ss |
| Next level | XP to the next level / live XP per second. "--" when the rate is zero or negative, or at the maximum level (never negative) |

## Decisions 

1. **Session scope: per game** (decided). A session starts when you enter a game and is cleared when you leave it
   (back to the main menu / lobby); the next game starts a new session from its first XP read. The reset hotkey /
   command starts a new session at once. Time counts only while in the game. `session_scope = character` keeps one
   session per character across games instead (time between games not counted; resets on a character switch).
2. **Deaths** (decided). XP/hour is **net**: a death's XP loss lowers it and can make it negative until gains
   outweigh it (the live rate recovers once the loss leaves the window). The time to next level shows "--" while the
   rate is zero or negative, so it is never negative. Gains and losses are also counted separately in the status
   command. Session XP (flat and %) is net as well.
3. **XP/hour %** uses level-equivalents (the same unit as Session %), not "% of the current level per hour". The two
   differ only around a level-up; (decided)
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

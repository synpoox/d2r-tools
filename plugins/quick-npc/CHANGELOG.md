# Changelog

## 1.0.0 - 2026-10-30

First public release, for Diablo II: Resurrected 3.3 under D2RLoader 1.3.1.

- Hold Left Ctrl and left-click an NPC to skip its dialog menu. You go to the NPC and the game's own menu option is
  chosen for you, so prices, range, quest and server checks work as if you had clicked it.
- Vendors open the trade screen. Deckard Cain identifies your items. Mercenary NPCs resurrect a dead mercenary.
  Warriv and Meshif travel to the next act. When none of the enabled options is on offer, the normal menu opens.
- NPCs are recognised from the game's own menu data, not a list of names. Monsters, objects, players and the user
  interface are untouched.
- A global plugin: works with every mod and the unmodded game. Settings are in `d2rloader\config\quick-npc.toml`
  (hotkey, per-option switches, timeout); console command `quick-npc` shows status and `quick-npc on|off|reload`.
- D2RLoader's plugin list shows the name Quick NPC, the author Synpoo and a description that covers every NPC option.
- Built for D2R 3.3.0 (Battle.net build 93847, Steam build 94995) with D2RLoader 1.3.1. On any other game or
  D2RLoader version it logs that and changes nothing.

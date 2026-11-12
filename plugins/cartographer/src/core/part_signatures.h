// The byte signature of each engine part (D2R 3.3 build 93847). Each starts at the function's first byte; "??" masks a
// relative branch or call target, a RIP-relative or table offset, or a record size. Recorded with their guard, max patch
// length and evidence in docs/reverse-engineering/cartographer.md section 3; re-checked by tools/check_portal_signatures.py.
// signatures.txt (beside maps.txt) can replace any of them without a rebuild. Pure text, so the tests can parse them.
#pragma once

namespace carto::sig {

inline constexpr const char* ItemSpellCheck = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 48 8B EA 48 8B F1";
inline constexpr const char* ItemSpellUse = "48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 48 8B EA 48 8B F1";
inline constexpr const char* ItemRecord = "40 57 48 83 EC 30 8B FA E8 ?? ?? ?? ?? 3B B8 ?? ?? ?? ?? 72 ?? 33 C0 48 83 C4 30";
inline constexpr const char* UnitClassOf = "48 83 EC 28 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC B8 FF FF FF FF 48 83 C4 28 C3 8B 41 04";
inline constexpr const char* UnitRoom = "40 53 48 83 EC 20 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9 74 ?? 48 83 C4 20";
// A 17-byte wrapper that other wrappers share: unique only with its padding and the next function's first bytes.
inline constexpr const char* RoomLevelId = "48 85 C9 75 ?? 33 C0 C3 48 8B 49 18 E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 85 C9";
inline constexpr const char* ClientMessage = "48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20 49 8B F8 0F B7 F2 48 8B D9";
inline constexpr const char* OpenRedPortal = "48 89 5C 24 08 48 89 6C 24 10 44 89 44 24 18 56 57 41 56 48 83 EC 70 48 8B E9";
// One of three identical hash-map lookups: told apart only by the distance (A4 3F 00 00) to its rehash helper, kept unmasked.
inline constexpr const char* GameCounter = "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 41 08 4C 8B F2 48 8B D9 4D 85 C0 75 ?? 48 8B 11 E8 A4 3F 00 00";
// D2RLoader's thunk (jmp [rip+x], then the dead original body); the thunk is the callable entry.
inline constexpr const char* UnitStat = "FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA";
inline constexpr const char* UnitCoords = "48 89 5C 24 10 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 75 ?? 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 48 85 FF 75 ?? 48 8D 4C 24 30 40 88 7C 24 30 E8 ?? ?? ?? ?? 84 C0 74 ?? CC 8B 0B 83 E9 02 74 ?? 83 E9 02 74 ?? 83 F9 01 74 ?? 48 8B 4B 38 48 85 C9";
inline constexpr const char* FindFreeSpot = "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 90 00 00 00 48 8B 05 ?? ?? ?? ??";
// The player mana step (game, player), run once per frame per live player: only the console command waits on it. Past the
// prologue into the body so no other function matches by register allocation alone; the hook cuts the first 14 bytes.
inline constexpr const char* PlayerTick = "48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA 4C 8B F9 48 85 D2 0F 84 ?? ?? ?? ?? 45 33 C0 48 8B CF 41 8D 50 08 E8 ?? ?? ?? ?? 48 8B CF 8B F0 E8 ?? ?? ?? ?? 33 DB 48 8B CF 8B E8 8D 53 55 E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ??";

// Map stats (cartographer.md section 6). The stat list functions are only called. SetListStat and AttachStatList are
// D2RLoader thunks (jmp [rip+x], then the dead original body): call the thunk, never hook it.
inline constexpr const char* NewStatList = "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 89 4C 24 08 55 41 54 41 55 41 56 41 57 48 8D 6C 24 C9 48 81 EC B0 00 00 00";
inline constexpr const char* SetListStat = "FF 25 ?? ?? ?? ?? 90 90 90 90 66 89 44 24 28";
inline constexpr const char* AttachStatList = "FF 25 ?? ?? ?? ?? 90 90 90 90 90 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 A0 00 00 00";
inline constexpr const char* DetachStatList = "40 53 48 83 EC 20 48 8B DA E8 ?? ?? ?? ?? 0F B6 C8 48 8B D3 48 83 C4 20 5B E9 ?? ?? ?? ??";
inline constexpr const char* FreeStatList = "48 85 D2 74 ?? 83 7A 1C 00 0F 8D ?? ?? ?? ?? C3 FF 25";
// The monster setup (params, monster) called once from the spawn core after the unit exists and is in its room. The
// 14-byte prefix alone matches 11 sites, so the signature runs on to 21 bytes; the hook cuts the first 14.
inline constexpr const char* MonsterSetup = "48 89 5C 24 18 48 89 6C 24 20 56 57 41 57 48 83 EC 30 8B 69 18";

// The maze steering parts (build 93847; docs/reverse-engineering/cartographer.md section 1a), re-checked by tools/check_maze_signatures.py.
// The first four are hooked (the hook cuts the first 14, 18, 17 and 14 bytes: whole instructions, all exact); the last three are only called.
inline constexpr const char* LevelBuild = "48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2";
inline constexpr const char* MazeWalk = "48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 81 C8 01 00 00 48 8B F9 48 8B 41 18";
inline constexpr const char* MazeEntranceExit = "41 56 48 83 EC 30 44 8B 0A 4D 8B F0 48 89 5C 24 40 4C 8B D2 48 8B 59 10";
inline constexpr const char* MazeRingGrower = "89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 48 48 8B 49 10";
inline constexpr const char* MazeAddRoom = "40 55 56 57 41 56 41 57 48 83 EC 30 48 63 EA 48 8B F9 48 8B 89 90 00 00 00";
inline constexpr const char* MazeLinkRooms = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 8B D8 48 8B F2 48 8B F9 E8 ?? ?? ?? ?? 44 8D 43 FE";
inline constexpr const char* MazePiecePicker = "48 89 5C 24 18 48 89 6C 24 20 57 41 56 41 57 48 83 EC 20 4C 8B 09 33 DB";
// The arrival room (cartographer.md section 5, "Arrival room"). All three are only checked, never called or hooked: the level-entry
// finder, its first-room-with-flags helper, and the flag test (room +0x50 and 0x30000). They guard the hook that gives the
// planned entrance cell's room those flags; if one is not found the arrival is left to the game. The finder's signature runs on
// through its "levels.txt Position == 0" branch (the call of the room finder at 0x3da718), so a build that changes that branch
// is refused. The flag test's signature ends at its own `ret`; the code after it is another function.
inline constexpr const char* ArrivalFinder = "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B FA 41 8B F1 41 8B D0 41 8B D8 44 0F B6 E1 E8 ?? ?? ?? ?? 4C 8B AC 24 80 00 00 00 44 8B C3 4C 8B B4 24 88 00 00 00 48 8B D7 41 0F B6 CC 48 8B E8 41 C7 45 00 FF FF FF FF 41 C7 06 FF FF FF FF E8 ?? ?? ?? ?? 4C 8B F8 48 83 78 10 00 75 0C 48 8B D0 41 0F B6 CC E8 ?? ?? ?? ?? 83 BD 90 00 00 00 00 49 8B D7 41 0F B6 CC 74 ?? 83 FE 0D 75 0D 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ?? EB 10 4D 8B CD 4C 89 74 24 20 44 8B C6 E8 ?? ?? ?? ?? 48 8B D0 41 0F B6 CC 48 8B D8 E8 ?? ?? ?? ?? 48 8B 43 58 E9 ?? ?? ?? ?? 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ??";
inline constexpr const char* ArrivalRoomFinder = "48 89 6C 24 18 57 41 56 41 57 48 83 EC 20 48 8B 7A 10 4D 8B F1 4D 8B F8 0F B6 E9 48 85 FF 74 15 48 8B CF E8 ?? ?? ?? ?? 85 C0 75 1A 48 8B 7F 48";
inline constexpr const char* ArrivalFlagTest = "8B 41 50 25 00 00 03 00 C3";

// The outdoor slot parts (build 93847; cartographer.md sections 4 and 8), re-checked by tools/check_maze_signatures.py. Nothing uses them yet:
// the outdoor builder (issue #19, phase 5) will hook OutdoorEntry (the spike hooked 19 bytes) and OutdoorAct5Builder (16 bytes) and only call
// PieceStamp and VoidCell. The hook lengths are the spike's, from section 4; each must be re-derived when a hook is added. LevelBuild above is
// the fifth part (the spike hooked 14 bytes of it).
inline constexpr const char* OutdoorEntry = "48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48 83 EC 50 4C 8B 7A 18 33 C0 4C 8B F2 44 0F B6 E1 49 89 87 A8 00 00 00 49 8D 4F 08 8B 42 2C 99 83";
inline constexpr const char* OutdoorAct5Builder = "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 48 81 EC C0 00 00 00 83 BA F8 01 00 00 6E 48 8B F2 44 0F B6 F1";
inline constexpr const char* PieceStamp = "48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 4C 8B 62 18 48 8B FA 48 63 B4 24 90 00 00 00 45 8B E9 8B D6 4C 89 64 24";
// (level, x, y): sets a grid cell void (piece id 0, flag 0x100). Its twin 0x3edda0 differs only in the last constant, which this signature includes.
inline constexpr const char* VoidCell = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 41 B9 00 01 00 00";

// Map events (cartographer.md section 9). PopulateRoom (game, serverRoom) is hooked: it runs once per not-yet-populated
// room, from both activation paths, so the original is called first and the event spawns after the room is populated.
// The 12-byte prefix alone matches 3 sites, so the signature runs on to 43 bytes; the hook cuts the first 12.
inline constexpr const char* PopulateRoom = "4C 8B DC 53 56 48 81 EC B8 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 80 00 00 00 83 3D ?? ?? ?? ?? 00 48 8B F2 48 8B D9";
// The two spawn functions are only called. The shorter prefixes match a neighbour (0x492aa0, 0x499ba1), so these run longer.
inline constexpr const char* SpawnMonster = "4C 8B DC 48 83 EC 68 8B 84 24 90 00 00 00 45 33 D2 4D 89 53 F0 45 89 53 D8 89 44 24 38 8B 84 24 98 00 00 00 89 44 24 3C 8B 84 24 A0 00 00 00 49 89 53 C0 89 44 24 4C";
inline constexpr const char* SpawnSuperUnique = "40 55 53 56 57 41 56 41 57 48 8D AC 24 48 FF FF FF 48 81 EC B8 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4";

// Event Spawn names from the game's loaded tables (cartographer.md section 11); both only called. DataTablesBase (dataFlag) returns the
// data tables base, three sibling accessors share its first 40 bytes so the signature runs to the tail. NameToRow (map, name, logOnMiss)
// returns the row, exactly -1 on a miss.
inline constexpr const char* DataTablesBase = "48 83 EC 28 0F B6 C1 48 89 44 24 38 48 83 F8 04 72 19 48 8D 44 24 38 48 8D 4C 24 40 48 89 44 24 40 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B 44 24 38 48 8D 0D ?? ?? ?? ?? 48 03 C0 48 8B 04 C1 48 83 C4 28 C3";
inline constexpr const char* NameToRow = "40 55 56 57 41 54 48 8B EC 48 83 EC 78 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 E8 44 89 45 AC";

// Client sync of player stats (cartographer.md section 7); all only called. FindStateList (unit, state) returns the first stat list
// of the unit whose state is `state`, or null. SetListState (list, state) writes the list's state; it is a 9-byte leaf, so the
// signature is the whole function. SetStateBit (unit, state, on) turns the state on or off, which is what makes the game send it.
// StateOn (unit, state) is nonzero while the state is on.
inline constexpr const char* FindStateList = "48 89 5C 24 10 57 48 83 EC 20 8B DA 48 8B F9 48 85 C9 75 13 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B CF E8 ?? ?? ?? ??";
inline constexpr const char* SetListState = "48 85 C9 74 03 89 51 20 C3";
inline constexpr const char* SetStateBit = "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 20 41 8B E8 8B DA 48 8B F1 85 D2 78 ?? E8 ?? ?? ?? ??";
inline constexpr const char* StateOn = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B DA 48 8B F1 E8 ?? ?? ?? ?? 85 C0 74 ??";
// Phase 6b: the Shrine object and the Boss Drop (cartographer.md section 13). DeathDrop (game, victim, arg3, info) is hooked
// (the hook cuts the first 15 bytes: whole instructions, no relative operand); the rest are only called. SpawnObject is the
// handler of the debug command `SpawnObject`: it is only read, to find the free-spot search (0x402f50), which has no signature of
// its own: it is byte-identical to a sibling for 452 bytes, so it is the target of the call at SpawnObject + 0xff.
inline constexpr const char* DeathDrop = "48 8B C4 44 89 40 18 53 55 56 57 41 54 41 56 41 57 48 83 EC 70 4D 8B F9 48 8B EA 4C 8B F1";
inline constexpr const char* DropRoller = "40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 98 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 88 00 00 00 4C 8B BC 24 18 01 00 00";
inline constexpr const char* DropLevel = "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 E8 ?? ?? ?? ?? 83 F8 01 75 ?? 45 33 C0 8D 50 0B";
inline constexpr const char* SpawnObject = "40 55 41 56 41 57 48 83 EC 60 4C 8B F1 41 BF 01 00 00 00 41 8B CF 8B EA E8 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ??";
inline constexpr const char* CreateObject = "40 55 53 57 48 8D AC 24 70 FC FF FF 48 81 EC 90 04 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48";

// The map's own Name on the tooltip, the automap title and the entry banner (cartographer.md section 10). LevelsRow (dataFlag, levelId)
// is the game's levels row getter, only called; its first 46 bytes are shared by three sibling getters, so the signature runs to 82.
inline constexpr const char* LevelsRow = "48 89 5C 24 08 55 56 57 48 83 EC 30 48 63 DA 0F B6 F1 E8 ?? ?? ?? ?? 48 8B E8 85 DB 7E 34 40 0F B6 CE E8 ?? ?? ?? ?? 48 8B B8 68 14 00 00 4C 63 C7 4C 3B C7 75 04 85 FF 79 14 48 8D 4C 24 58 C6 44 24 58 00 E8 ?? ?? ?? ?? 84 C0 74 01 CC 3B DF 7C 18";

// Phase 6b second half: Ambush and Treasure (cartographer.md section 13). OperateObject (game, activator, object) is hooked (the hook cuts
// the first 16 bytes: four whole instructions, no relative operand; the signature runs to 23 so no sibling matches). The rest are only
// called: FleeSetup is the engine's scare setup; FleeBlock and FleeTarget are the two checks it makes first (called only to log what they
// say); TcLookup gives the compiled treasure class for an index and TcRoller rolls it.
inline constexpr const char* OperateObject = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 48 8B F1";
inline constexpr const char* FleeSetup = "48 89 5C 24 10 48 89 74 24 18 57 48 81 EC 80 00 00 00 48 8B F1";
inline constexpr const char* FleeBlock = "48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 4D";
inline constexpr const char* FleeTarget = "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 20 45 0F B6 F1";
inline constexpr const char* TcLookup = "48 89 5C 24 08 55 56 57 48 83 EC 30 41 8B D8";
inline constexpr const char* TcRoller = "4C 8B DC 55 53 49 8D AB 08 F7 FF FF";

}  // namespace carto::sig

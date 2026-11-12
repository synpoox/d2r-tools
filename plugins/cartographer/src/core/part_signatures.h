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

}  // namespace carto::sig

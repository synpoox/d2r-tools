#pragma once
#include <cstddef>
namespace recap::native {
// Identification-only witnesses. Never hook the D2RCore thunks or their slots.
inline constexpr const char* GetterSig = "FF 25 ?? ?? ?? ?? 90 90 90 90 48 89 74 24 20 57 48 83 EC 20 41 0F B7 E8 8B FA 48 8B D9";
inline constexpr const char* SetterSig = "FF 25 ?? ?? ?? ?? 90 90 90 90 56 57 41 54 41 56 41 57 48 83 EC 40 45 0F B7 E1 45 8B F0 8B F2 48 8B E9";
inline constexpr const char* AdderSig = "FF 25 ?? ?? ?? ?? 57 41 56 48 83 EC 40 45 0F B7 F1 41 8B F8 8B EA 48 8B F1";
inline constexpr const char* ClientSig = "40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9 0F 85 ?? ?? ?? ?? 48 89 7C 24 70 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 48 8B F8 48 85 C0";
inline constexpr const char* FindPanelSig = "48 8B D1 48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 85 ?? ?? ?? ?? 33 C0 C3";
inline constexpr const char* FindChildSig = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 59 58 48 8B F2 48 8B";
inline constexpr const char* SetTextSig = "48 81 C1 88 00 00 00 E8 ?? ?? ?? ?? 48 8B CB 48 83 C4 20 5B E9";
inline constexpr const char* NameLayoutSig = "48 C7 45 48 18 00 00 00 48 C7 45 50 36 00 00 00";
inline constexpr const char* HasStateSig = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B DA 48 8B F1 E8 ?? ?? ?? ?? 85 C0 74 0E 83 E8 01";
inline constexpr const char *WidgetScreenPosSig = "48 8B C4 48 89 70 20 57 48 81 EC A0 00 00 00 0F 29 70 E8 48 8B F2 0F 29 78 D8 0F 57 FF 48 8B F9";
inline constexpr const char *ImageClipSig = "0F 10 83 ?? ?? ?? ?? 0F 29 45 ?? 84 C0 74 ?? 0F 10 8B ?? ?? ?? ??";
inline constexpr const char *AreaLayoutSig = "C7 85 E8 10 00 00 01 00 00 00 C7 85 EC 10 00 00 27 00 00 00 48 C7 85 F0 10 00 00 FD 00 00 00";
struct Signature { const char* name; const char* bytes; size_t callOffset; size_t entrySize; };
inline constexpr Signature Hit {"Hit", "40 55 53 56 57 41 56 41 57 48 8D AC 24 E8 FE FF FF 48 81 EC 18 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 00 01 00 00 48 8B BD 70 01 00 00 41 8B D9", 0, 17};
inline constexpr Signature Effects {"Effects", "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 68 FD FF FF 48 81 EC 98 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 80 02 00 00 4C 8B EA 48 89 4C", 0, 21};
inline constexpr Signature LifeDebit {"LifeDebit", "45 33 C0 48 8B CE 41 8D 50 06 E8 ?? ?? ?? ?? 2B C3 45 33 C9 45 33 C0 48 8B CE 3D 00 01 00 00 44 0F 4D C0 41 8D 51 06 E8 ?? ?? ?? ??", 39, 0};
inline constexpr Signature ManaDebit {"ManaDebit", "45 33 C0 48 8B CE 41 8D 50 08 E8 ?? ?? ?? ?? 2B C3 45 33 C9 45 33 C0 48 8B CE 3D 00 01 00 00 44 0F 4D C0 41 8D 51 08 E8 ?? ?? ?? ??", 39, 0};
inline constexpr Signature Heal {"Heal", "48 89 5C 24 18 56 48 83 EC 20 8B F2 48 8B D9 85 D2 7E 7C E8 ?? ?? ?? ?? 85 C0 75 73 8D 50 5C", 0, 15};
inline constexpr Signature LifeRegen {"LifeRegen", "B8 00 01 00 00 48 8B CB 3B F8 0F 4C F8 45 33 C9 44 8B C7 41 8D 51 06 E8 ?? ?? ?? ??", 23, 0};
inline constexpr Signature ManaRegen {"ManaRegen", "45 33 C9 44 8B C6 48 8B CF 41 8D 51 08 E8 ?? ?? ?? ??", 13, 0};
inline constexpr Signature Mode {"Mode", "4C 8B DC 4D 89 43 18 53 55 56 57 41 54 41 55 41 56 48 83 EC 40 45 8B F1 49 8B C0 48 8B DA 48 8B F9", 0, 17};
}

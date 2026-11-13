// The decision behind the AutomapRestore skip, with no game or SDK in it so it can be unit tested (issue #354).
// The skip is armed when a slot map is built and must land on the read made when the player enters the slot level. It relies on the player's level feed (the server tick, or the LevelChanged event on a TCP/IP client)
// reporting that level before the layer is read back; this state also makes a skip that was missed (the order flipped) impossible to fire later.
#pragma once

namespace carto {

struct SkipState {
	bool armed = false;
	int armedKey = -1;     // the layer key that was current when the skip was armed (the layer being left)
	int armedLevel = -1;   // the slot level (levels.txt id)
	bool entered = false;  // the player's level feed has reported the slot level since the skip was armed
};

enum class SkipVerdict { Keep, Skip };

// A slot map is built.
inline void SkipArm(SkipState& s, int currentKey, int slotLevel) noexcept {
	s = {true, currentKey, slotLevel, false};
}

// The player's level feed (server tick, or LevelChanged on a TCP/IP client) reports `level`. Once the player has been in the slot level and is reported anywhere else, the entry read has come and gone
// without being skipped, so the skip is dropped: it must not land on a later return from town and discard the map's own reveal.
inline void SkipTick(SkipState& s, int level) noexcept {
	if (!s.armed || level < 0) {  // an unknown level (-1) says nothing about where the player is
		return;
	}
	if (level == s.armedLevel) {
		s.entered = true;
	} else if (s.entered) {
		s.armed = false;
	}
}

// The game reads a layer's cells back; `key` is the layer now current, `level` the player's level from the last report. Skips (once) only for another layer
// than the one left, with the player already reported in the slot level.
inline auto SkipOnRead(SkipState& s, int key, int level) noexcept -> SkipVerdict {
	if (s.armed && key >= 0 && key != s.armedKey && level == s.armedLevel) {
		s.armed = false;
		return SkipVerdict::Skip;
	}
	return SkipVerdict::Keep;
}

}  // namespace carto

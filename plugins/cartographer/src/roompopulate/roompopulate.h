// The subscriber list behind cartographer.room-populate.v1. Pure C++ (no engine, no SDK): the service in engine/ wraps it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace cartographer::roompopulate {

// The flag `before` returns to skip the game's own populate of this room (the same value as RoomPopulateSkipNative).
inline constexpr uint32_t SkipNative = 1;

// before(game, room, levelId) -> flags, and after(game, room, levelId, skipped). An empty function is not called.
using BeforeFn = std::function<uint32_t(void*, void*, int32_t)>;
using AfterFn = std::function<void(void*, void*, int32_t, bool)>;

class Subscribers {
public:
	// A new handle (from 1, never reused). The plugin id names the subscriber in the log when it is dropped.
	auto add(std::string pluginId, BeforeFn before, AfterFn after) -> uint32_t;
	// False for an unknown handle (or one already removed).
	auto remove(uint32_t handle) -> bool;
	auto size() const -> std::size_t;

	// Calls every subscriber in subscribe order and returns the OR of their flags. A subscriber that throws is removed, its id
	// goes into `dropped` (when not null), and the others still run. Holds the lock for the whole run, so an unsubscribe from
	// another thread returns only after it; an unsubscribe from inside a callback takes effect at once.
	auto runBefore(void* game, void* room, int32_t level, std::vector<std::string>* dropped) -> uint32_t;
	void runAfter(void* game, void* room, int32_t level, bool skipped, std::vector<std::string>* dropped);

private:
	struct Entry {
		uint32_t handle;
		std::string pluginId;
		BeforeFn before;
		AfterFn after;
	};
	auto find(uint32_t handle) -> Entry*;
	void drop(uint32_t handle, std::vector<std::string>* dropped);

	mutable std::recursive_mutex lock_;
	std::vector<Entry> entries_;
	uint32_t next_ = 1;
};

}  // namespace cartographer::roompopulate

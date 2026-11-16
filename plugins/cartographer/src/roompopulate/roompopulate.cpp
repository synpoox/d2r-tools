#include "roompopulate.h"

namespace cartographer::roompopulate {

auto Subscribers::add(std::string pluginId, BeforeFn before, AfterFn after) -> uint32_t {
	std::lock_guard<std::recursive_mutex> guard(lock_);
	const uint32_t handle = next_++;
	entries_.push_back(Entry{handle, std::move(pluginId), std::move(before), std::move(after)});
	return handle;
}

auto Subscribers::remove(uint32_t handle) -> bool {
	std::lock_guard<std::recursive_mutex> guard(lock_);
	for (auto it = entries_.begin(); it != entries_.end(); ++it) {
		if (it->handle == handle) {
			entries_.erase(it);
			return true;
		}
	}
	return false;
}

auto Subscribers::size() const -> std::size_t {
	std::lock_guard<std::recursive_mutex> guard(lock_);
	return entries_.size();
}

auto Subscribers::find(uint32_t handle) -> Entry* {
	for (Entry& e : entries_) {
		if (e.handle == handle) {
			return &e;
		}
	}
	return nullptr;
}

void Subscribers::drop(uint32_t handle, std::vector<std::string>* dropped) {
	Entry* e = find(handle);
	if (e == nullptr) {
		return;
	}
	if (dropped != nullptr) {
		dropped->push_back(e->pluginId);
	}
	remove(handle);
}

auto Subscribers::runBefore(void* game, void* room, int32_t level, std::vector<std::string>* dropped) -> uint32_t {
	std::lock_guard<std::recursive_mutex> guard(lock_);
	std::vector<uint32_t> handles;
	handles.reserve(entries_.size());
	for (const Entry& e : entries_) {
		handles.push_back(e.handle);
	}
	uint32_t flags = 0;
	for (const uint32_t handle : handles) {
		Entry* e = find(handle);  // a callback may have removed it already
		if (e == nullptr || !e->before) {
			continue;
		}
		const BeforeFn fn = e->before;  // a copy: the callback may remove its own entry while it runs
		try {
			flags |= fn(game, room, level);
		} catch (...) {
			drop(handle, dropped);
		}
	}
	return flags;
}

void Subscribers::runAfter(void* game, void* room, int32_t level, bool skipped, std::vector<std::string>* dropped) {
	std::lock_guard<std::recursive_mutex> guard(lock_);
	std::vector<uint32_t> handles;
	handles.reserve(entries_.size());
	for (const Entry& e : entries_) {
		handles.push_back(e.handle);
	}
	for (const uint32_t handle : handles) {
		Entry* e = find(handle);
		if (e == nullptr || !e->after) {
			continue;
		}
		const AfterFn fn = e->after;
		try {
			fn(game, room, level, skipped);
		} catch (...) {
			drop(handle, dropped);
		}
	}
}

}  // namespace cartographer::roompopulate

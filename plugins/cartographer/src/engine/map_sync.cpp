// See map_sync.h. The messages and decisions are the pure core in src/mapsync; this file is the SDK glue.
#include "map_sync.h"

#include "log.h"
#include "map_names.h"
#include "maze_hooks.h"
#include "portal.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace ms = cartographer::mapsync;
namespace net = D2RL::Network;

namespace {

const D2RL::NetworkService* g_net = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PluginContext* g_ctx = nullptr;  // for sends made outside a callback (the game thread)
net::ChannelHandle g_channel = net::InvalidChannelHandle;
std::string g_state = "off: not installed";
std::atomic<bool> g_on{false};

std::mutex g_lock;
// Host side.
ms::PeerBook g_peers;
std::optional<ms::MapIdentity> g_hostMap;
// Client side.
ms::ClientMap g_clientMap;
ms::TableHashes g_hashes;
std::vector<std::string> g_names;
ms::ConnectSchedule g_schedule;
uint32_t g_generation = 0;  // changes at every game entered and left: a queued step from an old game does nothing
bool g_clientActive = false;  // the connect loop of this game is running or done; this process asked to connect
bool g_helloSent = false;
std::string g_clientNote;  // why a client stopped, for cartographer check; empty while connecting or connected
std::shared_ptr<const Tables> g_tables;  // the client's own tables, to place the host's map
ms::AppliedOpening g_applied;            // the opening the client applied last in this game
std::string g_applyNote;                 // what the last apply did, for cartographer check

auto NowMs() noexcept -> uint64_t {
	return GetTickCount64();
}

auto Name(const MapDef& map) -> std::string {
	return map.name;
}

auto Describe(const ms::MapIdentity& id, const std::string& name) -> std::string {
	char text[200];
	std::snprintf(text, sizeof(text), "row %u '%s', slot level %u, opening %u, %s seed %llu", static_cast<unsigned>(id.mapIndex), name.c_str(), static_cast<unsigned>(id.slotLevel),
	              static_cast<unsigned>(id.opening), id.maze ? "maze" : "preset", static_cast<unsigned long long>(id.seed));
	return text;
}

auto ResultName(net::Result r) -> const char* {
	switch (r) {
	case net::Result::Success: return "Success";
	case net::Result::InvalidArgument: return "InvalidArgument";
	case net::Result::Unsupported: return "Unsupported";
	case net::Result::Unavailable: return "Unavailable";
	case net::Result::Conflict: return "Conflict";
	case net::Result::NotFound: return "NotFound";
	case net::Result::Busy: return "Busy";
	case net::Result::OwnerInactive: return "OwnerInactive";
	case net::Result::OwnerMismatch: return "OwnerMismatch";
	case net::Result::StaleHandle: return "StaleHandle";
	case net::Result::CallbackFault: return "CallbackFault";
	case net::Result::PayloadTooLarge: return "PayloadTooLarge";
	case net::Result::NotConnected: return "NotConnected";
	case net::Result::WrongRole: return "WrongRole";
	case net::Result::Incompatible: return "Incompatible";
	case net::Result::RateLimited: return "RateLimited";
	}
	return "?";
}

auto ReasonName(net::RejectionReason r) -> const char* {
	switch (r) {
	case net::RejectionReason::None: return "none";
	case net::RejectionReason::UnknownPlugin: return "the host does not run this plugin";
	case net::RejectionReason::UnknownChannel: return "the host has no such channel";
	case net::RejectionReason::ScopeMismatch: return "scope mismatch";
	case net::RejectionReason::Incompatible: return "the compatibility token differs (another Cartographer version)";
	case net::RejectionReason::ProtocolMismatch: return "protocol mismatch";
	case net::RejectionReason::HostUnavailable: return "host unavailable";
	case net::RejectionReason::RegistrationClosed: return "registration closed";
	}
	return "?";
}

// A peer that is gone is dropped from the book; any other failure leaves it (it may work next time).
auto PeerGone(net::Result r) -> bool {
	return r == net::Result::NotConnected || r == net::Result::StaleHandle || r == net::Result::NotFound;
}

// Sends the host's current map to one peer. Safe to call on any thread the SDK allows sendToClient on.
void SendMapTo(const D2RL::PluginContext* context, net::PeerHandle peer, const ms::MapIdentity& id) noexcept {
	uint8_t buf[ms::kMaxMessageSize];
	const size_t size = ms::encodeMapOpen(id, buf, sizeof(buf));
	if (size == 0) {
		plog::Warn("map sync: the open map cannot be sent (not a valid identity)");
		return;
	}
	const net::Result r = g_net->sendToClient(context, g_channel, peer, ms::kMsgMapOpen, buf, static_cast<uint32_t>(size));
	if (r == net::Result::Success) {
		plog::Info("map sync: told client %llu about the open map", static_cast<unsigned long long>(peer));
		return;
	}
	plog::Warn("map sync: could not tell client %llu: %s", static_cast<unsigned long long>(peer), ResultName(r));
	if (PeerGone(r)) {
		std::lock_guard<std::mutex> lock(g_lock);
		g_peers.remove(peer);
	}
}

// Host, game thread: a client said hello; send it the open map if there is one.
void __cdecl SendCurrentCallback(const D2RL::PluginContext* context, void* userData) noexcept {
	try {
		const auto peer = static_cast<net::PeerHandle>(reinterpret_cast<uintptr_t>(userData));
		std::optional<ms::MapIdentity> map;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (!g_peers.has(peer)) {
				return;
			}
			map = g_hostMap;
		}
		if (map) {
			SendMapTo(context, peer, *map);
		}
	} catch (...) {
		plog::Error("map sync: sending the open map failed");
	}
}

void __cdecl OnHostMessage(const D2RL::PluginContext* context, net::ChannelHandle, net::PeerHandle peer, uint16_t messageId, const void* data, uint32_t size, void*) noexcept {
	try {
		if (context == nullptr || !g_on.load()) {
			return;
		}
		if (messageId != ms::kMsgHello) {
			static std::atomic<uint32_t> unknown{0};
			if (plog::Sample(unknown)) {
				plog::Warn("map sync: host got message id %u from a client; ignored", static_cast<unsigned>(messageId));
			}
			return;
		}
		if (const ms::DecodeError e = ms::decodeHello(data, size); e != ms::DecodeError::None) {
			plog::Warn("map sync: a client's hello was refused: %s", ms::describe(e));
			return;
		}
		ms::PeerBook::Add added;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			added = g_peers.add(peer);
		}
		if (added == ms::PeerBook::Add::AddedDroppedOldest) {
			plog::Warn("map sync: %zu clients known; the oldest was dropped to make room for client %llu", ms::kMaxPeers, static_cast<unsigned long long>(peer));
		}
		plog::Info("map sync: client %llu said hello (%s)", static_cast<unsigned long long>(peer), added == ms::PeerBook::Add::Added ? "new" : "again");
		// The send goes through the game thread, as the SDK sample does for a reply.
		if (g_threads == nullptr || g_threads->runOnGameThread(context, SendCurrentCallback, reinterpret_cast<void*>(static_cast<uintptr_t>(peer))) != D2RL::Threads::Result::Success) {
			plog::Warn("map sync: could not queue the reply to client %llu on the game thread; sending from the callback", static_cast<unsigned long long>(peer));
			SendCurrentCallback(context, reinterpret_cast<void*>(static_cast<uintptr_t>(peer)));
		}
	} catch (...) {
		plog::Error("map sync: handling a client's hello failed");
	}
}

// Client, UI thread: applies the host's map once per opening (phase 7 part 2). The map's name goes into this client's own copy of the slot
// level's row (MapNames), and the host's layout seed becomes the maze job of the next slot build. Each half is checked against this
// client's own tables and skipped, with the reason logged, when it does not fit; nothing here ever blocks or changes the game otherwise.
void __cdecl ApplyStep(const D2RL::PluginContext*, void* userData) noexcept {
	try {
		if (!g_on.load() || ServerTickSeen()) {
			return;  // a host or single player process applies nothing from a message
		}
		const auto generation = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(userData));
		ms::MapIdentity id;
		std::shared_ptr<const Tables> tables;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (generation != g_generation || !g_clientMap.current()) {
				return;
			}
			id = *g_clientMap.current();
			if (!g_applied.take(id.opening)) {
				return;
			}
			tables = g_tables;
		}
		if (tables == nullptr || id.mapIndex >= tables->maps.size() || ms::nameHash(tables->maps[id.mapIndex].name) != id.nameHash) {
			plog::Warn("map sync: the host's map (row %u, opening %u) is not in this client's tables any more; nothing applied", static_cast<unsigned>(id.mapIndex), static_cast<unsigned>(id.opening));
			return;
		}
		const MapDef& map = tables->maps[id.mapIndex];
		const bool maze = MapIsMaze(map);
		const int slotLevel = SlotLevel(tables->slots, maze ? MazeSlot : PresetSlot);
		ms::ApplyFacts facts;
		facts.slotLevelMatches = slotLevel > 0 && slotLevel == static_cast<int>(id.slotLevel);
		facts.clientMazeMap = maze;
		facts.namesSetting = tables->settings.mapNames != 0;
		facts.mazeSteeringLive = MazeSteeringLive();
		facts.tilesetFits = !maze || MazeTilesetProblem(map, *tables).empty();
		facts.slotBuilt = MazeSlotRanAny();
		const ms::ApplyPlan plan = ms::decideApply(id, facts);
		std::string note;
		if (plan.names) {
			const int flag = MazeLevelDataFlag();
			if (flag < 0) {
				note += "name skipped: no level build has been seen here yet, so the data flag of this client's tables is unknown; ";
				plog::Warn("map sync: '%s' keeps the slot level's name: no level build has been seen yet, so the data flag is unknown", map.name.c_str());
			} else if (MapNamesBeforeOpen(map, *tables, slotLevel, static_cast<uint32_t>(flag))) {
				note += "name applied; ";
				plog::Info("map sync: the host's map '%s': its name is set on this client's slot level %d (data flag %d)", map.name.c_str(), slotLevel, flag);
			} else {
				note += "name not applied (see the map names lines); ";
			}
		} else {
			note += std::string("name skipped: ") + ms::describe(plan.namesWhy) + "; ";
			plog::Info("map sync: the host's map '%s': name not applied here: %s", map.name.c_str(), ms::describe(plan.namesWhy));
		}
		if (plan.maze) {
			if (MazeRemoteMapOpened(map, *tables, id.seed)) {
				note += "layout seed applied";
				plog::Info("map sync: the host's map '%s': the next build of the Maze slot level here uses the host's layout seed", map.name.c_str());
			} else {
				note += "layout skipped: the slot level was built here first, or steering went off";
				plog::Warn("map sync: the host's map '%s': the layout seed was not applied (the slot level was built first, or steering went off)", map.name.c_str());
			}
		} else {
			note += std::string("layout skipped: ") + ms::describe(plan.mazeWhy);
			if (plan.mazeWhy == ms::Skip::NotMaze) {
				plog::Info("map sync: the host's map '%s' is a preset map: no layout to apply", map.name.c_str());
			} else {
				plog::Warn("map sync: the host's map '%s': the layout is not applied here: %s", map.name.c_str(), ms::describe(plan.mazeWhy));
			}
		}
		std::lock_guard<std::mutex> lock(g_lock);
		if (generation == g_generation) {
			g_applyNote = note;
		}
	} catch (...) {
		plog::Error("map sync: applying the host's map failed");
	}
}

void __cdecl OnClientMessage(const D2RL::PluginContext*, net::ChannelHandle, uint16_t messageId, const void* data, uint32_t size, void*) noexcept {
	try {
		if (!g_on.load()) {
			return;
		}
		if (ServerTickSeen()) {
			return;  // a host or single player process never keeps a remote map
		}
		if (messageId != ms::kMsgMapOpen) {
			static std::atomic<uint32_t> unknown{0};
			if (plog::Sample(unknown)) {
				plog::Warn("map sync: client got message id %u from the host; ignored", static_cast<unsigned>(messageId));
			}
			return;
		}
		ms::MapIdentity id;
		if (const ms::DecodeError e = ms::decodeMapOpen(data, size, &id); e != ms::DecodeError::None) {
			plog::Warn("map sync: the host's map message was refused: %s", ms::describe(e));
			return;
		}
		ms::Verdict verdict;
		std::string name;
		uint32_t generation;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			verdict = g_clientMap.offer(id, g_hashes);
			generation = g_generation;
			if (id.mapIndex < g_names.size()) {
				name = g_names[id.mapIndex];
			}
		}
		if (verdict == ms::Verdict::Accepted || verdict == ms::Verdict::Unchanged) {
			plog::Info("map sync: the host's open map: %s (%s)", Describe(id, name).c_str(), ms::describe(verdict));
		} else {
			plog::Warn("map sync: the host's open map (row %u, opening %u) was not kept: %s", static_cast<unsigned>(id.mapIndex), static_cast<unsigned>(id.opening), ms::describe(verdict));
		}
		if (verdict == ms::Verdict::Accepted) {
			// Applied on the UI thread, which a remote client has (runOnGameThread is Unavailable there). The step reads the kept identity.
			if (g_threads == nullptr || g_ctx == nullptr ||
			    g_threads->runOnUiThread(g_ctx, ApplyStep, reinterpret_cast<void*>(static_cast<uintptr_t>(generation))) != D2RL::Threads::Result::Success) {
				plog::Warn("map sync: could not queue applying the host's map; its name and layout are not applied");
			}
		}
	} catch (...) {
		plog::Error("map sync: handling the host's map message failed");
	}
}

// Client: connected. Says hello once so the host knows this peer and sends the open map.
void SendHello(const D2RL::PluginContext* context) noexcept {
	if (ServerTickSeen()) {
		return;  // a host or single player process never acts as a client, even if it connected to itself
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_helloSent) {
			return;
		}
		g_helloSent = true;
		ms::finish(g_schedule);
	}
	uint8_t buf[ms::kHelloSize];
	const size_t size = ms::encodeHello(buf, sizeof(buf));
	const net::Result r = g_net->sendToHost(context, g_channel, ms::kMsgHello, buf, static_cast<uint32_t>(size));
	if (r == net::Result::Success) {
		plog::Info("map sync: connected to the host, hello sent");
	} else {
		plog::Warn("map sync: connected, but the hello was not sent: %s", ResultName(r));
		std::lock_guard<std::mutex> lock(g_lock);
		g_helloSent = false;  // the next connection event or attempt tries again
	}
}

void __cdecl OnConnectionState(const D2RL::PluginContext* context, const net::ConnectionEvent* event, void*) noexcept {
	try {
		if (context == nullptr || !g_on.load() || !net::HasConnectionEventField(event, net::ConnectionEventRequiredSize)) {
			return;
		}
		bool client;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			client = g_clientActive;
		}
		switch (event->state) {
		case net::ConnectionState::Connected:
			plog::Info("map sync: connection event: connected (peer %llu, %s)", static_cast<unsigned long long>(event->peer), client ? "this process asked to connect" : "host side");
			if (client) {
				SendHello(context);
			}
			break;
		case net::ConnectionState::Rejected:
			plog::Warn("map sync: the host refused the connection: %s", ReasonName(event->reason));
			if (client) {
				const bool retry = event->reason == net::RejectionReason::HostUnavailable;
				std::lock_guard<std::mutex> lock(g_lock);
				if (ms::onRejected(g_schedule, retry)) {
					plog::Info("map sync: the host is not available yet; the connect loop keeps retrying");
				} else {
					g_clientActive = false;
					g_clientNote = std::string("the host refused the connection: ") + ReasonName(event->reason);
				}
			}
			break;
		case net::ConnectionState::Disconnected: {
			std::lock_guard<std::mutex> lock(g_lock);
			if (event->peer != net::InvalidPeerHandle && g_peers.remove(event->peer)) {
				plog::Info("map sync: client %llu left", static_cast<unsigned long long>(event->peer));
			}
			if (client) {
				g_helloSent = false;
			}
			break;
		}
		case net::ConnectionState::Connecting:
			break;
		}
	} catch (...) {
		plog::Error("map sync: handling a connection event failed");
	}
}

// Client, UI thread: one step of the connect loop, re-queued each frame until it connects, is refused or runs out of attempts.
void __cdecl ConnectStep(const D2RL::PluginContext* context, void* userData) noexcept {
	try {
		const auto generation = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(userData));
		ms::Step step;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (generation != g_generation || g_schedule.done) {
				return;
			}
			step = ms::step(g_schedule, NowMs());
		}
		if (step == ms::Step::Wait && ServerTickSeen()) {
			// A server tick means this process is the host or a single player game: no connection is needed, and none is made.
			std::lock_guard<std::mutex> lock(g_lock);
			ms::finish(g_schedule);
			g_clientActive = false;
			plog::Info("map sync: this process has the server, so it does not connect to a host");
			return;
		}
		if (step == ms::Step::GiveUp) {
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_clientActive) {
				plog::Warn("map sync: could not connect to a host after %d attempts; maps opened by a host will not be known here", g_schedule.attempts);
				g_clientActive = false;
				g_clientNote = "gave up connecting to a host";
			}
			return;
		}
		if (step == ms::Step::Try) {
			{
				// The server-tick check and the flag share one lock, so a tick seen after the check cannot be missed by the flag.
				std::lock_guard<std::mutex> lock(g_lock);
				if (ServerTickSeen()) {
					ms::finish(g_schedule);
					g_clientActive = false;
					return;
				}
				g_clientActive = true;
			}
			net::ChannelInfo info{};
			info.structSize = net::ChannelInfoSize;
			if (g_net->getChannelInfo(context, g_channel, &info) == net::Result::Success && info.connectionState == net::ConnectionState::Connected) {
				SendHello(context);  // connected, and the event was missed or not delivered
				return;
			}
			const net::Result r = g_net->connectToHost(context, g_channel);
			int attempts;
			{
				std::lock_guard<std::mutex> lock(g_lock);
				attempts = g_schedule.attempts;
			}
			if (attempts == 1 || r != net::Result::Busy) {
				plog::Info("map sync: connectToHost attempt %d: %s", attempts, ResultName(r));
			}
			if (r == net::Result::Incompatible || r == net::Result::WrongRole || r == net::Result::Unsupported || r == net::Result::OwnerInactive) {
				plog::Warn("map sync: the host cannot be reached for map sync (%s); giving up for this game", ResultName(r));
				std::lock_guard<std::mutex> lock(g_lock);
				ms::finish(g_schedule);
				g_clientActive = false;
				g_clientNote = std::string("the host cannot be reached (") + ResultName(r) + ")";
				return;
			}
		}
		if (g_threads == nullptr || g_threads->runOnUiThread(context, ConnectStep, userData) != D2RL::Threads::Result::Success) {
			plog::Warn("map sync: could not queue the next connect step; the client will not connect");
		}
	} catch (...) {
		plog::Error("map sync: the connect step failed");
	}
}

}  // namespace

auto InstallMapSync(const D2RL::PluginContext* context) -> bool {
	try {
		if (context == nullptr) {
			return false;
		}
		if (context->QueryService(&g_net) != D2RL::ServiceQueryResult::Success || !D2RL::HasNetworkServiceField(g_net, D2RL::NetworkServiceRequiredSize)) {
			g_net = nullptr;
			g_state = "off: D2RLoader has no Network service of the expected version";
			plog::Warn("map sync %s", g_state.c_str());
			return false;
		}
		if (context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success || !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize)) {
			g_net = nullptr;
			g_threads = nullptr;
			g_state = "off: D2RLoader has no Thread service of the expected version";
			plog::Warn("map sync %s", g_state.c_str());
			return false;
		}
		const net::ChannelRegistration registration{
		    .structSize = net::ChannelRegistrationSize,
		    .localChannelId = ms::kChannelId,
		    .compatibilityToken = ms::kChannelToken,
		    .hostMessage = OnHostMessage,
		    .clientMessage = OnClientMessage,
		    .connectionState = OnConnectionState,
		};
		const net::Result r = g_net->registerChannel(context, &registration, &g_channel);
		if (r != net::Result::Success) {
			g_net = nullptr;
			g_state = std::string("off: D2RLoader refused the channel (") + ResultName(r) + ")";
			plog::Warn("map sync %s", g_state.c_str());
			return false;
		}
		g_ctx = context;
		g_state = "on";
		g_on = true;
		plog::Info("map sync: channel registered (wire version %u)", static_cast<unsigned>(ms::kWireVersion));
		return true;
	} catch (...) {
		g_on = false;
		g_state = "off: installing failed";
		plog::Error("map sync: installing failed; it stays off");
		return false;
	}
}

void MapSyncSetTables(const std::shared_ptr<const Tables>& tables) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_hashes.clear();
	g_names.clear();
	g_tables = tables;
	if (tables == nullptr) {
		return;
	}
	for (const MapDef& map : tables->maps) {
		g_names.push_back(map.name);
		g_hashes.push_back(ms::nameHash(map.name));
	}
}

void MapSyncMapOpened(const MapDef& map, const Tables& tables, int slotLevel, int openings, bool maze, uint64_t seed) {
	if (!g_on.load()) {
		return;
	}
	try {
		const std::ptrdiff_t row = &map - tables.maps.data();
		if (row < 0 || static_cast<size_t>(row) >= tables.maps.size() || row > 0xffff || slotLevel <= 0 || slotLevel > 0xffff) {
			plog::Warn("map sync: '%s' is not sent (row %lld, slot level %d is out of range)", Name(map).c_str(), static_cast<long long>(row), slotLevel);
			return;
		}
		ms::MapIdentity id;
		id.mapIndex = static_cast<uint16_t>(row);
		id.nameHash = ms::nameHash(map.name);
		id.slotLevel = static_cast<uint16_t>(slotLevel);
		id.opening = static_cast<uint16_t>(openings < 1 ? 1 : openings > 0xffff ? 0xffff : openings);
		id.maze = maze;
		id.seed = maze ? seed : 0;
		std::vector<uint64_t> peers;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			g_hostMap = id;
			peers = g_peers.peers();
		}
		plog::Info("map sync: %s is open (%s); %zu client(s) to tell", Name(map).c_str(), Describe(id, map.name).c_str(), peers.size());
		for (const uint64_t peer : peers) {
			SendMapTo(g_ctx, peer, id);
		}
	} catch (...) {
		plog::Error("map sync: telling the clients about '%s' failed", map.name.c_str());
	}
}

void MapSyncGameEntered() {
	if (!g_on.load()) {
		return;
	}
	try {
		uint32_t generation;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			generation = ++g_generation;
			g_peers.clear();
			g_hostMap.reset();
			g_clientMap.clear();
			g_clientActive = false;
			g_clientNote.clear();
			g_applied.clear();
			g_applyNote.clear();
			g_helloSent = false;
			ms::begin(g_schedule, NowMs());
		}
		if (g_threads->runOnUiThread(g_ctx, ConnectStep, reinterpret_cast<void*>(static_cast<uintptr_t>(generation))) != D2RL::Threads::Result::Success) {
			plog::Warn("map sync: could not queue the connect step; this client will not connect to a host");
		}
	} catch (...) {
		plog::Error("map sync: starting for the new game failed");
	}
}

void MapSyncGameLeft() {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		++g_generation;
		g_peers.clear();
		g_hostMap.reset();
		g_clientMap.clear();
		g_applied.clear();
		g_applyNote.clear();
		g_clientActive = false;
		g_helloSent = false;
		ms::finish(g_schedule);
	} catch (...) {
	}
}

void UnloadMapSync() noexcept {
	try {
		g_on = false;
		net::ChannelHandle channel;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			g_state = "off: unloaded";
			channel = g_channel;
			g_channel = net::InvalidChannelHandle;
		}
		if (g_net != nullptr && g_ctx != nullptr && channel != net::InvalidChannelHandle && D2RL::HasNetworkServiceField(g_net, D2RL::NetworkServiceUnregisterChannelFieldEnd)) {
			(void)g_net->unregisterChannel(g_ctx, channel);
		}
	} catch (...) {
	}
}

auto MapSyncRemoteMap() -> std::optional<RemoteMap> {
	std::lock_guard<std::mutex> lock(g_lock);
	if (!g_clientMap.current()) {
		return std::nullopt;
	}
	RemoteMap out;
	out.identity = *g_clientMap.current();
	if (out.identity.mapIndex < g_names.size()) {
		out.name = g_names[out.identity.mapIndex];
	}
	return out;
}

auto MapSyncSummary() -> std::string {
	std::string text = "map sync " + g_state;
	if (!g_on.load()) {
		return text;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_hostMap) {
		text += "; host: map open, " + std::to_string(g_peers.peers().size()) + " client(s) known";
	} else if (!g_peers.peers().empty()) {
		text += "; host: " + std::to_string(g_peers.peers().size()) + " client(s) known, no map open";
	}
	if (g_clientMap.current()) {
		const ms::MapIdentity& id = *g_clientMap.current();
		text += "; client: the host's map is " + Describe(id, id.mapIndex < g_names.size() ? g_names[id.mapIndex] : std::string("?"));
		if (!g_applyNote.empty()) {
			text += " (" + g_applyNote + ")";
		}
	} else if (g_clientActive) {
		text += "; client: connecting to the host, no map told yet";
	} else if (!g_clientNote.empty()) {
		text += "; client: " + g_clientNote;
	}
	return text;
}

}  // namespace carto

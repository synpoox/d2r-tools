// Experimental read-only Armory diagnostics. No native function is invoked.
#include <D2RLPlugin/api.h>
#include "crash_guard.h"
#include "loadouts.h"
#include "native_image.h"
#include <cstdio>

#ifndef ARMORY_VERSION
#define ARMORY_VERSION "0.0.1"
#endif

namespace {
constexpr D2RL::PluginInfo Info{
    .infoSize = D2RL::PluginInfoSize,
    .abiVersion = D2RL_PLUGIN_ABI_VERSION,
    .id = "armory",
    .name = "Armory",
    .version = ARMORY_VERSION,
    .author = "Synpoo",
    .description = "Experimental read-only equipment loadout diagnostics.",
    .flags = D2RL::PluginFlags::Shared,
    .reserved = {},
};

const D2RL::InventoryService* g_inventory{};
const D2RL::ItemService* g_items{};
const D2RL::ThreadService* g_threads{};
armory::native::ResolvedImage g_image{};

bool ReadModule(void* opaque, uint32_t offset, std::span<uint8_t> out) {
    const auto base = *static_cast<const uintptr_t*>(opaque);
    const auto start = base + offset;
    if (start < base || out.size() > (std::numeric_limits<uintptr_t>::max)() - start) return false;
    const auto end = start + out.size();
    for (auto address = start; address < end;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) ||
            reinterpret_cast<uintptr_t>(info.AllocationBase) != base || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto region = reinterpret_cast<uintptr_t>(info.BaseAddress);
        if (info.RegionSize > (std::numeric_limits<uintptr_t>::max)() - region) return false;
        const auto next = region + info.RegionSize;
        if (next <= address) return false;
        address = next;
    }
    SIZE_T count{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(start),
                             out.data(), out.size(), &count) && count == out.size();
}
bool Executable(uintptr_t address, uintptr_t base) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) ||
        reinterpret_cast<uintptr_t>(info.AllocationBase) != base || info.State != MEM_COMMIT ||
        (info.Protect & PAGE_GUARD)) return false;
    const DWORD protection = info.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}
void Scan(const D2RL::PluginContext* context) noexcept {
    try {
        uintptr_t base = context->exeBase;
        g_image = armory::native::ResolveImage(base, ReadModule, &base);
        for (auto& address : g_image.addresses) if (!Executable(address, base)) address = 0;
    } catch (...) {
        g_image = {};
    }
    if (!g_image.valid) plog::Warn("native image unavailable; diagnostics only");
    for (size_t i=0;i<g_image.addresses.size();++i)
        plog::Info("%s: %s", armory::native::Signatures[i].name,
                   g_image.addresses[i] ? "resolved executable entry (not callable)" : "off: no unique verified entry");
}
bool InventoryReady() {
    return g_inventory && g_items && g_threads && g_inventory->getLocalPlayer &&
           g_inventory->getCursorItem && g_inventory->getEquippedItem && g_inventory->forEachInventoryItem &&
           g_items->getItemInfo && g_threads->runOnGameThread;
}
struct Observation {
    std::array<uint32_t,4> counts{};
    std::array<D2RL::ItemHandle,12> slots{};
    uint32_t invalid{};
};
D2RL::Inventory::IterationAction __cdecl Observe(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* data) noexcept {
    auto& out=*static_cast<Observation*>(data);
    if (!item || item->structSize < D2RL::Items::ItemInfoRequiredSize || item->handle == D2RL::InvalidItemHandle) {
        ++out.invalid;
        return D2RL::Inventory::IterationAction::Stop;
    }
    using D2RL::Items::ItemContainer;
    switch (item->container) {
    case ItemContainer::Equipment:
        ++out.counts[0];
        if (item->bodyLocation < 1 || item->bodyLocation > 12) { ++out.invalid; break; }
        if (out.slots[static_cast<size_t>(item->bodyLocation-1)] != D2RL::InvalidItemHandle) { ++out.invalid; break; }
        out.slots[static_cast<size_t>(item->bodyLocation-1)] = item->handle;
        break;
    case ItemContainer::Inventory: ++out.counts[1]; break;
    case ItemContainer::PersonalStash: ++out.counts[2]; break;
    case ItemContainer::SharedStash: ++out.counts[3]; break;
    default: ++out.invalid; break;
    }
    return D2RL::Inventory::IterationAction::Continue;
}
void __cdecl Probe(const D2RL::PluginContext* context, void*) noexcept {
    if (!InventoryReady()) return;
    D2RL::PlayerHandle player=D2RL::InvalidPlayerHandle;
    if (g_inventory->getLocalPlayer(context,&player)!=D2RL::Inventory::Result::Success || player==D2RL::InvalidPlayerHandle) {
        plog::Warn("probe: no authoritative local player"); return;
    }
    using D2RL::Items::ItemContainer;
    using D2RL::Items::ContainerBit;
    const D2RL::Inventory::ItemFilter filter{
        .structSize=D2RL::Inventory::ItemFilterSize,
        .containerMask=ContainerBit(ItemContainer::Equipment)|ContainerBit(ItemContainer::Inventory)|
                       ContainerBit(ItemContainer::PersonalStash)|ContainerBit(ItemContainer::SharedStash),
    };
    Observation out{};
    const auto result=g_inventory->forEachInventoryItem(context,player,&filter,Observe,&out);
    if (result!=D2RL::Inventory::Result::Success || out.invalid) {
        plog::Warn("probe: incomplete enumeration, result=%u invalid=%u", static_cast<unsigned>(result), out.invalid); return;
    }
    uint32_t present=0, empty=0, uncertain=0;
    for (int slot=1;slot<=12;++slot) {
        D2RL::ItemHandle item=D2RL::InvalidItemHandle;
        const auto status=g_inventory->getEquippedItem(context,player,slot,&item);
        const auto enumerated=out.slots[static_cast<size_t>(slot-1)];
        if ((status==D2RL::Inventory::Result::NotFound ||
             (status==D2RL::Inventory::Result::Success && item==D2RL::InvalidItemHandle)) &&
            enumerated==D2RL::InvalidItemHandle) {
            ++empty; continue;
        }
        if (status!=D2RL::Inventory::Result::Success || item==D2RL::InvalidItemHandle) { ++uncertain; continue; }
        D2RL::Items::ItemInfo info{.structSize=D2RL::Items::ItemInfoSize};
        if (g_items->getItemInfo(context,item,&info)!=D2RL::Items::Result::Success ||
            info.structSize<D2RL::Items::ItemInfoRequiredSize || info.container!=ItemContainer::Equipment ||
            info.handle!=item || item!=enumerated || info.bodyLocation!=slot) { ++uncertain; continue; }
        ++present;
    }
    D2RL::ItemHandle cursor=D2RL::InvalidItemHandle;
    const auto cursorResult=g_inventory->getCursorItem(context,player,&cursor);
    plog::Info("probe: equipment=%u inventory=%u personal-stash=%u shared-stash=%u; slots present=%u empty=%u uncertain=%u; cursor result=%u present=%u",
               out.counts[0],out.counts[1],out.counts[2],out.counts[3],present,empty,uncertain,
               static_cast<unsigned>(cursorResult),cursor!=D2RL::InvalidItemHandle ? 1u : 0u);
    // Counts do not prove all stash pages are accessible; no handles are saved.
}
auto __cdecl Status(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
    if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
    auto* context=command->plugin;
    context->WriteConsoleMessage("Armory 0.0.1 diagnostics only; equipment switching is disabled.");
    char line[160]{};
    for (size_t i=0;i<g_image.addresses.size();++i) {
        std::snprintf(line,sizeof(line),"%s: %s",armory::native::Signatures[i].name,g_image.addresses[i]?"resolved (not called)":"off");
        context->WriteConsoleMessage(line);
    }
    context->WriteConsoleMessage(InventoryReady()?"Read-only inventory probe available.":"Read-only inventory probe unavailable.");
    return D2RL::ConsoleCommandResult::Handled;
}
auto __cdecl ProbeCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
    if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
    auto* context=command->plugin;
    if (!InventoryReady() || g_threads->runOnGameThread(context,Probe,nullptr)!=D2RL::Threads::Result::Success) {
        context->WriteConsoleWarning("Armory: authoritative inventory probe unavailable.");
        return D2RL::ConsoleCommandResult::Failed;
    }
    context->WriteConsoleMessage("Armory: read-only probe queued; see plugin log.");
    return D2RL::ConsoleCommandResult::Handled;
}
} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* { return &Info; }
D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
    if (!context) return false;
    plog::Init(context,"armory");
    crash_guard::Install(ARMORY_VERSION,context->exeBase);
    char image[200]{};
    crash_guard::Describe(image,sizeof(image),D2RL::GetBuildVersion(context));
    plog::Info("%s loaded (%s), native writes disabled",ARMORY_VERSION,image);
    Scan(context);
    if (context->QueryService(&g_inventory)!=D2RL::ServiceQueryResult::Success ||
        !D2RL::HasInventoryServiceField(g_inventory,D2RL::InventoryServiceRequiredSize)) g_inventory=nullptr;
    if (context->QueryService(&g_items)!=D2RL::ServiceQueryResult::Success ||
        !D2RL::HasItemServiceField(g_items,static_cast<uint32_t>(offsetof(D2RL::ItemService,getItemInfo)+sizeof(D2RL::Items::GetItemInfoFn)))) g_items=nullptr;
    if (context->QueryService(&g_threads)!=D2RL::ServiceQueryResult::Success ||
        !D2RL::HasThreadServiceField(g_threads,D2RL::ThreadServiceRequiredSize)) g_threads=nullptr;
    auto status=D2RL::MakeConsoleCommand("armory",Status,"Armory read-only diagnostic status.");
    status.scope=D2RL::ConsoleCommandScope::LocalOnly;
    if (!context->RegisterConsoleCommand(status)) {
        crash_guard::Remove();
        return false;
    }
    if (InventoryReady()) {
        auto probe=D2RL::MakeConsoleCommand("armory-probe",ProbeCommand,"Read-only inventory enumeration on the authoritative game thread.");
        probe.scope=D2RL::ConsoleCommandScope::LocalOnly;
        if (!context->RegisterConsoleCommand(probe)) plog::Warn("armory-probe registration failed");
    } else plog::Warn("inventory probe off: required SDK fields unavailable");
    return true;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
    crash_guard::Remove();
    g_image={};
    g_inventory=nullptr; g_items=nullptr; g_threads=nullptr;
}

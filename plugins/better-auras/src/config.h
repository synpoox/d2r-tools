// Better Auras config: <game>\d2rloader\config\better-auras.toml (D2RLoader's config folder; the plugin is game-wide).
//
// Only skills listed under [skills.<id or name>] are touched; every other skill stays vanilla. Each listed skill
// starts from [defaults] and overrides what it sets:
//
//   [defaults]
//   enabled = true          # false: the skill stays vanilla
//   toggle  = true          # true: casting it toggles it on and off; false: vanilla "on while selected" (drain and
//                           #   reservation still apply while it runs)
//
//   [skills.Might]          # a skills.txt name (skill column) or id: [skills.98]
//   drain   = { resource = "mana", perSecond = 2.5 }            # or drain.resource = "mana" / [skills.Might.drain]
//   reserve = { resource = "mana", mode = "percent", value = 25 } # mode "percent" (of max) or "flat"
//
// The parser reads the TOML this needs: tables, dotted keys, inline tables, strings, numbers, booleans, comments.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <cstdint>

namespace config {

enum class Resource : uint8_t { None, Mana, Life };
enum class ReserveMode : uint8_t { Percent, Flat };

struct SkillConfig {
	int32_t skill = -1;  // skills.txt id
	char key[64] = "";   // as written in the file, for messages
	bool enabled = true;
	bool toggle = true;
	Resource drainResource = Resource::None;
	double drainPerSecond = 0;
	Resource reserveResource = Resource::None;
	ReserveMode reserveMode = ReserveMode::Percent;
	double reserveValue = 0;
};

// Reads the plugin's config through D2RLoader (<game>\d2rloader\config\better-auras.toml; written with the repo's
// better-auras.toml as defaults when missing). Names are resolved through the running mod's skills.txt when it is a
// loose file, else the vanilla aura names built in. Returns false (and says why) when the file is unreadable or
// wrong; no skill is managed then.
auto Load(const D2RL::PluginContext* context, char* report, size_t reportSize) noexcept -> bool;

// The config of a listed, enabled skill, or null (vanilla).
auto Find(int32_t skill) noexcept -> const SkillConfig*;

auto Count() noexcept -> int;
auto At(int i) noexcept -> const SkillConfig&;

auto ResourceName(Resource r) noexcept -> const char*;

}  // namespace config

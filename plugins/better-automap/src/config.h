// Settings from better-automap.toml.
//
// Read order: <plugin folder>\better-automap.toml if it exists (a copy next to the DLL), else the file D2RLoader
// creates from the embedded defaults (<game>\d2rloader\config\better-automap.toml). A missing or unreadable file
// leaves the defaults below. Unknown keys are ignored; a bad value keeps that key's default and is reported.
//
// Only a small part of TOML is understood: [tables], key = true/false, numbers and "strings", # comments.
#pragma once

#include <D2RLPlugin/api.h>

namespace config {

struct Settings {
	// [minimap]
	bool minimapEnabled = true;
	bool minimapLeft = false;  // position = "right" (default) or "left"

	// [overlay]
	bool overlayEnabled = true;

	// [zoom]
	bool zoomEnabled = true;
	float zoomMin = 0.5f;
	float zoomMax = 2.5f;
	float zoomStep = 0.25f;
};

// Reads the file once. `source` / `problems` describe what was read, for the log and the status command.
void Load(const D2RL::PluginContext* context) noexcept;

auto Get() noexcept -> const Settings&;
auto Source() noexcept -> const char*;
auto Problems() noexcept -> const char*;

}  // namespace config

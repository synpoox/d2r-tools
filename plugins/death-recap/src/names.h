#pragma once
#include <D2RLPlugin/api.h>
#include "telemetry.h"
namespace recap {
bool InstallNames(const D2RL::PluginContext*) noexcept;
void CopyNames(Report& report);
std::string SourceName(const Report& report, const Event& event);
}

#pragma once
#include "telemetry.h"
#include <filesystem>
#include <string>
namespace recap {
using History = std::vector<Report>;
std::string Encode(const History& history);
bool Decode(const std::string& bytes, History& history);
bool WriteAtomic(const std::filesystem::path& path, const std::string& bytes);
History LoadHistory(const std::filesystem::path& path, size_t retention);
std::string Json(const Report& report);
}

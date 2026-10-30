#include "spec_store.h"

#include "log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// shlobj.h needs windows.h first
#include <knownfolders.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <cwchar>

namespace ms {

namespace {

constexpr int MaxModDepth = 3;  // mods\<savepath> may be nested a few folders deep

wchar_t g_saveRoot[520] = L"";     // ...\Saved Games\Diablo II Resurrected
wchar_t g_fallbackDir[520] = L"";  // <mod support dir or plugin dir>\multispec

auto ReadSmallFile(const wchar_t* path, char* out, size_t size, size_t* length) noexcept -> bool {
	*length = 0;
	HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		return false;
	}
	DWORD got = 0;
	const BOOL ok = ReadFile(h, out, static_cast<DWORD>(size - 1), &got, nullptr);
	LARGE_INTEGER total {};
	const BOOL sized = GetFileSizeEx(h, &total);
	CloseHandle(h);
	if (!ok || !sized || static_cast<unsigned long long>(total.QuadPart) > size - 1) {
		return false;
	}
	out[got] = '\0';
	*length = got;
	return true;
}

auto Exists(const wchar_t* path) noexcept -> bool {
	return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

void Utf8(const wchar_t* w, char* out, size_t size) noexcept {
	if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, static_cast<int>(size), nullptr, nullptr) == 0 && size > 0) {
		out[0] = '\0';
	}
}

// The folders holding a character's save, and how many were found. Several slots can hold a same-name character, so
// the newest save wins and the count is reported.
struct SaveSearch {
	wchar_t dir[520] = L"";
	FILETIME written {};
	int count = 0;
};

// Checks <dir>\<file>, then the subfolders of <dir> down to `depth` levels.
void SearchSaves(const wchar_t* dir, const wchar_t* file, int depth, SaveSearch* found) noexcept {
	wchar_t save[520];
	std::swprintf(save, 520, L"%ls\\%ls", dir, file);
	WIN32_FILE_ATTRIBUTE_DATA info {};
	if (GetFileAttributesExW(save, GetFileExInfoStandard, &info) &&
	    (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		if (found->count == 0 || CompareFileTime(&info.ftLastWriteTime, &found->written) > 0) {
			std::swprintf(found->dir, 520, L"%ls", dir);
			found->written = info.ftLastWriteTime;
		}
		++found->count;
	}
	if (depth == 0) {
		return;
	}
	wchar_t pattern[520];
	std::swprintf(pattern, 520, L"%ls\\*", dir);
	WIN32_FIND_DATAW entry {};
	HANDLE h = FindFirstFileW(pattern, &entry);
	if (h == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		const bool folder = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if (folder && std::wcscmp(entry.cFileName, L".") != 0 && std::wcscmp(entry.cFileName, L"..") != 0) {
			wchar_t sub[520];
			std::swprintf(sub, 520, L"%ls\\%ls", dir, entry.cFileName);
			SearchSaves(sub, file, depth - 1, found);
		}
	} while (FindNextFileW(h, &entry));
	FindClose(h);
}

}  // namespace

void InitStorePaths(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	g_saveRoot[0] = g_fallbackDir[0] = L'\0';
	PWSTR saved = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames, 0, nullptr, &saved)) && saved != nullptr) {
		std::swprintf(g_saveRoot, 520, L"%ls\\Diablo II Resurrected", saved);
		CoTaskMemFree(saved);
	}
	const wchar_t* base = context->modSupportDirectory != nullptr ? context->modSupportDirectory : context->pluginDirectory;
	if (base != nullptr) {
		std::swprintf(g_fallbackDir, 520, L"%ls\\multispec", base);
	}
	char a[600];
	char b[600];
	Utf8(g_saveRoot, a, sizeof(a));
	Utf8(g_fallbackDir, b, sizeof(b));
	std::snprintf(report, size, "saves searched under %s (root and mods subfolders); fallback %s", a[0] ? a : "?",
	              b[0] ? b : "?");
}

auto FileStore::Open(const char* name, char* note, size_t size) noexcept -> bool {
	m_path[0] = L'\0';
	if (!ValidName(name)) {
		std::snprintf(note, size, "bad character name");
		return false;
	}
	wchar_t wname[MaxName];
	MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MaxName);
	wchar_t d2s[MaxName + 8];
	std::swprintf(d2s, MaxName + 8, L"%ls.d2s", wname);
	SaveSearch found;
	if (g_saveRoot[0] != L'\0') {
		SearchSaves(g_saveRoot, d2s, 0, &found);
		wchar_t mods[520];
		std::swprintf(mods, 520, L"%ls\\mods", g_saveRoot);
		SearchSaves(mods, d2s, MaxModDepth, &found);
	}
	if (found.count > 0) {
		std::swprintf(m_path, 520, L"%ls\\%ls.multispec", found.dir, wname);
		std::snprintf(note, size, found.count == 1 ? "next to the save" : "next to the newest of %d same-name saves",
		              found.count);
		return true;
	}
	if (g_fallbackDir[0] == L'\0') {
		std::snprintf(note, size, "no save folder and no fallback folder");
		return false;
	}
	CreateDirectoryW(g_fallbackDir, nullptr);
	std::swprintf(m_path, 520, L"%ls\\%ls.multispec", g_fallbackDir, wname);
	std::snprintf(note, size, "FALLBACK folder: %s.d2s not found in the save folder", name);
	return true;
}

auto FileStore::Load(CharSpecs* out, char* why, size_t size) noexcept -> LoadResult {
	if (m_path[0] == L'\0') {
		std::snprintf(why, size, "no file chosen");
		return LoadResult::Failed;
	}
	if (!Exists(m_path)) {
		return LoadResult::Missing;
	}
	static char text[MaxFileBytes + 1];
	size_t len = 0;
	char error[96] = "unreadable";
	if (ReadSmallFile(m_path, text, sizeof(text), &len) && Parse(text, len, out, error, sizeof(error))) {
		return LoadResult::Loaded;
	}
	// Keep the bad file for a look; never overwrite it in place.
	SYSTEMTIME t {};
	GetLocalTime(&t);
	wchar_t bad[600];
	std::swprintf(bad, 600, L"%ls.bad-%04u%02u%02u-%02u%02u%02u", m_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
	              t.wSecond);
	if (!MoveFileExW(m_path, bad, MOVEFILE_WRITE_THROUGH)) {
		std::snprintf(why, size, "%s, and it could not be moved aside", error);
		return LoadResult::Failed;
	}
	char b[600];
	Utf8(bad, b, sizeof(b));
	std::snprintf(why, size, "%s; kept as %s", error, b);
	return LoadResult::Quarantined;
}

auto FileStore::Write(const CharSpecs& specs) noexcept -> bool {
	static char text[MaxFileBytes];
	const size_t len = Serialize(specs, text, sizeof(text));
	if (len == 0 || m_path[0] == L'\0') {
		plog::Error("spec file: could not serialize");
		return false;
	}
	wchar_t tmp[600];
	wchar_t bak[600];
	std::swprintf(tmp, 600, L"%ls.tmp", m_path);
	std::swprintf(bak, 600, L"%ls.bak", m_path);
	HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		plog::Error("spec file: cannot create the temp file (error %lu)", GetLastError());
		return false;
	}
	DWORD wrote = 0;
	const BOOL ok = WriteFile(h, text, static_cast<DWORD>(len), &wrote, nullptr) && wrote == len && FlushFileBuffers(h);
	CloseHandle(h);
	if (!ok) {
		plog::Error("spec file: write failed (error %lu)", GetLastError());
		DeleteFileW(tmp);
		return false;
	}
	if (Exists(m_path)) {
		CopyFileW(m_path, bak, FALSE);  // the previous version, for a manual restore
	}
	if (!MoveFileExW(tmp, m_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		plog::Error("spec file: replace failed (error %lu)", GetLastError());
		DeleteFileW(tmp);
		return false;
	}
	return true;
}

auto FileStore::PathUtf8(char* out, size_t size) const noexcept -> void {
	Utf8(m_path, out, size);
}

}  // namespace ms

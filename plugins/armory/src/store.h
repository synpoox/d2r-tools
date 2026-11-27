#pragma once
#include "loadouts.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <cerrno>
#endif
namespace armory {
enum class StoreResult { Ok, Missing, Invalid, IoError, Busy };
inline bool IsArmoryPath(const std::filesystem::path& path) { return path.extension() == ".armory"; }
// A persistent lock file is harmless; the OS releases its lock on close or
// process termination. Never unlink a POSIX lock file while other writers may
// be waiting on its inode.
class WriteLock {
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
public:
    explicit WriteLock(const std::filesystem::path& path) {
        if (!IsArmoryPath(path)) return;
        auto lock=path; lock += ".lock";
#ifdef _WIN32
        handle_=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if (handle_ != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(handle_,&info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))) {
                CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE;
            }
        }
#else
        fd_=open(lock.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW,0600);
        if (fd_>=0) {
            struct stat info{};
            if (fstat(fd_,&info)!=0 || !S_ISREG(info.st_mode) || flock(fd_,LOCK_EX|LOCK_NB)!=0) { close(fd_); fd_=-1; }
        }
#endif
    }
    WriteLock(const WriteLock&)=delete;
    WriteLock& operator=(const WriteLock&)=delete;
    ~WriteLock() {
#ifdef _WIN32
        if (handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (fd_>=0) close(fd_);
#endif
    }
    explicit operator bool() const {
#ifdef _WIN32
        return handle_!=INVALID_HANDLE_VALUE;
#else
        return fd_>=0;
#endif
    }
};
// Caller supplies a dedicated Armory path and verified character scope. A corrupt
// existing file must never be interpreted as an empty book and overwritten.
inline StoreResult ReadBook(const std::filesystem::path& path, std::string_view scope, Book& output) {
    if (!IsArmoryPath(path)) return StoreResult::Invalid;
    std::error_code ec;
    auto status = std::filesystem::symlink_status(path, ec);
    if (status.type() == std::filesystem::file_type::not_found) return StoreResult::Missing;
    if (ec || !std::filesystem::is_regular_file(status)) return StoreResult::IoError;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) return StoreResult::IoError;
    if (size > MaxFile) return StoreResult::Invalid;
    std::ifstream file(path, std::ios::binary);
    if (!file) return StoreResult::IoError;
    std::string data(static_cast<size_t>(size), '\0');
    if (!file.read(data.data(), static_cast<std::streamsize>(data.size())) || file.peek() != std::char_traits<char>::eof() || file.bad()) return StoreResult::IoError;
    auto decoded = Decode(data, scope);
    if (!decoded) return StoreResult::Invalid;
    output = std::move(*decoded);
    return StoreResult::Ok;
}
// Same-directory exclusive temp, flush before atomic rename. A leftover temp
// from a crash blocks writes rather than overwriting another writer's file.
inline bool AtomicWrite(const std::filesystem::path& path, std::string_view bytes) {
    if (!IsArmoryPath(path) || bytes.size()>MaxFile) return false;
    auto temp = path; temp += ".tmp";
#ifdef _WIN32
    HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(h);
    if (!CloseHandle(h)) ok = false;
    if (ok) ok = MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temp.c_str());
#else
    int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return false;
    size_t pos = 0;
    while (pos < bytes.size()) {
        auto n = write(fd, bytes.data() + pos, bytes.size() - pos);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        pos += static_cast<size_t>(n);
    }
    bool ok = pos == bytes.size() && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temp.c_str(), path.c_str()) == 0;
    if (!ok) unlink(temp.c_str());
#endif
    return ok;
}
// Hold the OS lock across comparison and replacement. The caller still owns
// synchronization of its in-memory Book and game session.
inline StoreResult CommitBook(const std::filesystem::path& path, Book& current, const Book& next) {
    if (!IsArmoryPath(path) || !ValidBook(current) || current.scope != next.scope) return StoreResult::Invalid;
    auto data = Encode(next);
    if (!data) return StoreResult::Invalid;
    WriteLock lock(path);
    if (!lock) return StoreResult::Busy;
    Book disk;
    auto result = ReadBook(path, current.scope, disk);
    if (result == StoreResult::Ok && disk != current) return StoreResult::Invalid;
    if (result == StoreResult::Missing && !current.entries.empty()) return StoreResult::Invalid;
    if (result != StoreResult::Ok && result != StoreResult::Missing) return result;
    // Prepare the in-memory publication before committing the file.
    auto prepared = next;
    if (!AtomicWrite(path, *data)) return StoreResult::IoError;
    current = std::move(prepared);
    return StoreResult::Ok;
}
} // namespace armory

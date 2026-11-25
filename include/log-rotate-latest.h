#pragma once
#include <windows.h>
#include <cstdint>
#include <mutex>
#include <string>

// Bounded single-generation log rotation for latest.log and aggregate/latest.log.
// Once the active log reaches MaxBytes (default 64 KiB, matching
// latest-explorer-exits.log), it is moved to <path>.old (replacing any previous
// .old file) so disk usage stays bounded while keeping recent history intact.
namespace LeanDockLog {
inline constexpr uint64_t DefaultMaxBytes = 64 * 1024;

inline std::wstring BackupPath(std::wstring const& path) {
    return path + L".old";
}

inline bool FileSize(const wchar_t* path, uint64_t& outSize) {
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info)) return false;
    outSize = (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    return true;
}

inline bool AppendUtf8(const wchar_t* path, const void* data, DWORD bytes, uint64_t maxBytes = DefaultMaxBytes) {
    if (!path || !*path || !data || !bytes) return false;
    static std::mutex logMutex;
    std::lock_guard<std::mutex> guard(logMutex);

    bool truncate = false;
    uint64_t currentSize = 0;
    if (maxBytes > 0 && FileSize(path, currentSize) && currentSize > 0 &&
        (currentSize >= maxBytes || currentSize + bytes > maxBytes)) {
        const std::wstring backup = BackupPath(path);
        if (!MoveFileExW(path, backup.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            uint64_t afterSize = 0;
            if (FileSize(path, afterSize) && afterSize >= maxBytes * 2) {
                truncate = true;
            }
        }
    }

    HANDLE file = CreateFileW(
        path,
        truncate ? GENERIC_WRITE : FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        truncate ? CREATE_ALWAYS : OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, bytes, &written, nullptr);
    CloseHandle(file);
    return ok != FALSE && written == bytes;
}

inline bool AppendWideLine(const wchar_t* path, const wchar_t* line, uint64_t maxBytes = DefaultMaxBytes) {
    if (!path || !*path || !line || !*line) return false;
    const int needed = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return false;
    if (needed <= 8192) {
        char stackBuf[8192];
        const int written = WideCharToMultiByte(CP_UTF8, 0, line, -1, stackBuf, needed, nullptr, nullptr);
        if (written <= 1) return false;
        return AppendUtf8(path, stackBuf, static_cast<DWORD>(written - 1), maxBytes);
    }
    std::string heapBuf(static_cast<size_t>(needed), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, line, -1, heapBuf.data(), needed, nullptr, nullptr);
    if (written <= 1) return false;
    return AppendUtf8(path, heapBuf.data(), static_cast<DWORD>(written - 1), maxBytes);
}
}

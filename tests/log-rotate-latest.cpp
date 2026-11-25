#include "../include/log-rotate-latest.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static int g_checks = 0;
static int g_failures = 0;

static void Check(bool cond, const char* name) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("FAIL: %s\n", name);
    }
}

static std::string ReadAll(fs::path const& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static uint64_t SizeOf(fs::path const& p) {
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    return ec ? 0 : static_cast<uint64_t>(sz);
}

int wmain() {
    wchar_t tempDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDir);
    const fs::path root = fs::path(tempDir) / (L"leandock-log-rotate-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const fs::path logPath = root / L"latest.log";
    const fs::path oldPath = root / L"latest.log.old";

    // 1. Null / empty input guard
    Check(!LeanDockLog::AppendUtf8(nullptr, "a", 1, 64), "reject null path");
    Check(!LeanDockLog::AppendUtf8(L"", "a", 1, 64), "reject empty path");
    Check(!LeanDockLog::AppendUtf8(logPath.c_str(), nullptr, 1, 64), "reject null data");
    Check(!LeanDockLog::AppendUtf8(logPath.c_str(), "a", 0, 64), "reject zero bytes");
    Check(!LeanDockLog::AppendWideLine(logPath.c_str(), L"", 64), "reject empty wide line");
    Check(!fs::exists(logPath), "rejected calls do not create log");

    // 2. Append under cap with UTF-8 CJK preservation
    Check(LeanDockLog::AppendWideLine(logPath.c_str(), L"2026-11-21 12:00:00 测试第一行\r\n", 128), "append line 1");
    Check(LeanDockLog::AppendWideLine(logPath.c_str(), L"2026-11-21 12:00:01 second line\r\n", 128), "append line 2");
    Check(fs::exists(logPath) && !fs::exists(oldPath), "no rotation below cap");
    const std::string under = ReadAll(logPath);
    Check(under.find("second line\r\n") != std::string::npos, "line 2 present");
    Check(under.find("\xE6\xB5\x8B\xE8\xAF\x95") != std::string::npos, "UTF-8 CJK preserved");

    // 3. Cross cap -> rotate to .old, start fresh active log
    Check(LeanDockLog::AppendWideLine(logPath.c_str(), L"2026-11-21 12:00:02 third line triggers rotation because total exceeds 128 bytes\r\n", 128),
          "append line 3 triggers rotation");
    Check(fs::exists(logPath) && fs::exists(oldPath), "rotation creates .old and fresh active log");
    Check(ReadAll(oldPath) == under, ".old preserves exact previous generation");
    Check(ReadAll(logPath).find("third line triggers rotation") != std::string::npos, "fresh active log holds new line");

    // 4. Second rollover overwrites .old and keeps single-generation bound
    Check(LeanDockLog::AppendWideLine(logPath.c_str(), L"2026-11-21 12:00:03 fourth line triggers second rotation across 128 bytes limit!!\r\n", 128),
          "append line 4 triggers second rotation");
    Check(!fs::exists(root / L"latest.log.old.old"), "no chained .old.old files");
    Check(ReadAll(oldPath).find("third line triggers rotation") != std::string::npos, ".old replaced with generation 2");
    Check(ReadAll(logPath).find("fourth line triggers second rotation") != std::string::npos, "active holds generation 3");

    // 5. Fallback hard cap when .old is locked (MoveFileExW fails)
    const fs::path lockedLog = root / L"locked.log";
    const fs::path lockedOld = root / L"locked.log.old";
    HANDLE holdOld = CreateFileW(lockedOld.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(holdOld != INVALID_HANDLE_VALUE, "lock .old handle opened");
    for (int i = 0; i < 20; ++i) {
        const std::wstring row = L"row-" + std::to_wstring(i) + L"-0123456789abcdef\r\n";
        Check(LeanDockLog::AppendWideLine(lockedLog.c_str(), row.c_str(), 64), "append while .old locked succeeds");
    }
    CloseHandle(holdOld);
    const uint64_t lockedSize = SizeOf(lockedLog);
    Check(lockedSize > 0 && lockedSize <= 160, "hard cap bounds file when rotation is blocked");

    // 6. Concurrent multi-thread writes remain bounded and valid
    const fs::path concLog = root / L"concurrent.log";
    const fs::path concOld = root / L"concurrent.log.old";
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([t, &concLog]() {
            for (int i = 0; i < 60; ++i) {
                wchar_t buf[128]{};
                _snwprintf_s(buf, _TRUNCATE, L"thread=%d iter=%03d payload=0123456789abcdef\r\n", t, i);
                LeanDockLog::AppendWideLine(concLog.c_str(), buf, 512);
            }
        });
    }
    for (auto& th : threads) th.join();
    Check(fs::exists(concLog) && fs::exists(concOld), "concurrent writes rotated");
    Check(SizeOf(concLog) <= 600, "concurrent active log bounded");
    Check(SizeOf(concOld) <= 600, "concurrent backup log bounded");

    // 7. DefaultMaxBytes matches 64 KiB
    Check(LeanDockLog::DefaultMaxBytes == 65536, "default cap is 64 KiB");

    fs::remove_all(root, ec);
    std::printf("log-rotate checks=%d failures=%d\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

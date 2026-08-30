#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include "Log.h"
#include "Version.h"

namespace Log {

    static FILE* gFile = nullptr;
    static bool  gToFile = true;
    static bool  gVerbose = false;
    static CRITICAL_SECTION gLock;
    static bool  gLockInit = false;
    static char  gLogPath[MAX_PATH] = "";

    static void WriteLine(const char* prefix, const char* fmt, va_list args) {
        char buf[1024];
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        buf[sizeof(buf) - 1] = '\0';

        if (gLockInit) EnterCriticalSection(&gLock);
        OutputDebugStringA("[HelicopterOptions] ");
        if (prefix && prefix[0]) OutputDebugStringA(prefix);
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
        if (gFile) {
            std::fprintf(gFile, "[HelicopterOptions] %s%s\n", prefix ? prefix : "", buf);
            std::fflush(gFile);
        }
        if (gLockInit) LeaveCriticalSection(&gLock);
    }

    static void RotateFile() {
        char oldPath[MAX_PATH];
        std::snprintf(oldPath, MAX_PATH, "%s.old", gLogPath);
        DeleteFileA(oldPath);
        MoveFileA(gLogPath, oldPath);
    }

    void Open(void* moduleHandle) {
        if (!gLockInit) { InitializeCriticalSection(&gLock); gLockInit = true; }
        if (!gToFile || gFile) return;
        char dir[MAX_PATH]{};
        DWORD n = GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleHandle), dir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return;
        char* slash = std::strrchr(dir, '\\');
        if (slash) *(slash + 1) = '\0';

        char sub[MAX_PATH];
        std::snprintf(sub, MAX_PATH, "%sHelicopterOptions", dir);
        CreateDirectoryA(sub, nullptr);
        std::snprintf(sub, MAX_PATH, "%sHelicopterOptions\\Logs", dir);
        CreateDirectoryA(sub, nullptr);
        std::snprintf(gLogPath, MAX_PATH, "%sHelicopterOptions\\Logs\\HelicopterOptions.log", dir);

        // Hard rotation cap at open (config not parsed yet).
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExA(gLogPath, GetFileExInfoStandard, &fad)
            && fad.nFileSizeLow > 4u * 1024u * 1024u)
            RotateFile();

        gFile = std::fopen(gLogPath, "w");
        if (gFile) {
            std::time_t now = std::time(nullptr);
            std::fprintf(gFile, "HelicopterOptions " HO_VERSION_STR " log\nSession start: %s\n", std::ctime(&now));
            std::fflush(gFile);
        }
    }

    void Close() {
        if (gFile) { std::fflush(gFile); std::fclose(gFile); gFile = nullptr; }
    }

    void CheckRotate(int maxKB) {
        if (!gFile || maxKB <= 0) return;
        if (gLockInit) EnterCriticalSection(&gLock);
        const long pos = std::ftell(gFile);
        if (pos > static_cast<long>(maxKB) * 1024L) {
            std::fclose(gFile);
            RotateFile();
            gFile = std::fopen(gLogPath, "w");
            if (gFile) {
                std::fprintf(gFile, "[HelicopterOptions] log rotated (exceeded %d KB); "
                                    "previous log saved as HelicopterOptions.log.old\n", maxKB);
                std::fflush(gFile);
            }
        }
        if (gLockInit) LeaveCriticalSection(&gLock);
    }

    void Info(const char* fmt, ...)    { va_list a; va_start(a, fmt); WriteLine("",          fmt, a); va_end(a); }
    void Warn(const char* fmt, ...)    { va_list a; va_start(a, fmt); WriteLine("WARNING: ", fmt, a); va_end(a); }
    void Error(const char* fmt, ...)   { va_list a; va_start(a, fmt); WriteLine("ERROR: ",   fmt, a); va_end(a); }
    void Verbose(const char* fmt, ...) {
        if (!gVerbose) return;
        va_list a; va_start(a, fmt); WriteLine("", fmt, a); va_end(a);
    }

    void SetVerbose(bool on) { gVerbose = on; }
    void SetToFile(bool on)  { gToFile = on; if (!on) Close(); }
    bool IsVerbose()         { return gVerbose; }

    void HexString(const unsigned char* bytes, unsigned len, char* buf, unsigned bufLen) {
        static const char* d = "0123456789ABCDEF";
        unsigned i = 0;
        for (; i < len && (i * 2 + 2) < bufLen; ++i) {
            buf[i * 2]     = d[bytes[i] >> 4];
            buf[i * 2 + 1] = d[bytes[i] & 0xF];
        }
        buf[i * 2] = '\0';
    }

} // namespace Log

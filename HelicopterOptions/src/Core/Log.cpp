#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "Log.h"
#include "Version.h"

namespace Log {

    namespace {

        constexpr long kMaxBytes = 4L * 1024L * 1024L;

        FILE*            gFile = nullptr;
        CRITICAL_SECTION gLock;
        bool             gLockReady = false;
        char             gPath[MAX_PATH] = "";

        void Write(const char* prefix, const char* format, va_list args) {
            char text[1024];
            std::vsnprintf(text, sizeof(text), format, args);
            char line[1100];
            std::snprintf(line, sizeof(line), "[HelicopterOptions] %s%s\n", prefix, text);

            if (gLockReady) EnterCriticalSection(&gLock);
            OutputDebugStringA(line);
            if (gFile) {
                std::fputs(line, gFile);
                std::fflush(gFile);
            }
            if (gLockReady) LeaveCriticalSection(&gLock);
        }

    }

    void Open(void* module) {
        if (!gLockReady) {
            InitializeCriticalSection(&gLock);
            gLockReady = true;
        }
        if (gFile) return;

        char directory[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), directory, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return;
        if (char* slash = std::strrchr(directory, '\\')) slash[1] = '\0';

        char folder[MAX_PATH];
        std::snprintf(folder, sizeof(folder), "%sHelicopterOptions", directory);
        CreateDirectoryA(folder, nullptr);
        std::snprintf(folder, sizeof(folder), "%sHelicopterOptions\\Logs", directory);
        CreateDirectoryA(folder, nullptr);
        std::snprintf(gPath, sizeof(gPath), "%s\\HelicopterOptions.log", folder);

        gFile = std::fopen(gPath, "w");
        if (gFile) {
            const std::time_t now = std::time(nullptr);
            std::fprintf(gFile, "HelicopterOptions " HO_VERSION_STR " log\nSession start: %s\n", std::ctime(&now));
            std::fflush(gFile);
        }
    }

    void Close() {
        if (!gFile) return;
        std::fclose(gFile);
        gFile = nullptr;
    }

    void CheckRotate() {
        if (!gFile) return;
        if (gLockReady) EnterCriticalSection(&gLock);
        if (std::ftell(gFile) > kMaxBytes) {
            std::fclose(gFile);
            char old[MAX_PATH];
            std::snprintf(old, sizeof(old), "%s.old", gPath);
            DeleteFileA(old);
            MoveFileA(gPath, old);
            gFile = std::fopen(gPath, "w");
        }
        if (gLockReady) LeaveCriticalSection(&gLock);
    }

    void Info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("", format, args);
        va_end(args);
    }

    void Warn(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("WARNING: ", format, args);
        va_end(args);
    }

    void Error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("ERROR: ", format, args);
        va_end(args);
    }

    void Hex(const unsigned char* bytes, unsigned length, char* out, unsigned outLength) {
        static const char digits[] = "0123456789ABCDEF";
        unsigned i = 0;
        for (; i < length && i * 2 + 2 < outLength; ++i) {
            out[i * 2]     = digits[bytes[i] >> 4];
            out[i * 2 + 1] = digits[bytes[i] & 0x0F];
        }
        out[i * 2] = '\0';
    }

}

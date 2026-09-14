#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfloat>
#include <cstring>
#include "Memory.h"

namespace Memory {

    namespace {

        int SafeCompare(const void* a, const void* b, size_t length, int* result) {
            __try {
                *result = std::memcmp(a, b, length);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        int SafeCopy(void* destination, const void* source, size_t length) {
            __try {
                std::memcpy(destination, source, length);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

    }

    bool IsFinite(float value) {
        return value == value && value <= FLT_MAX && value >= -FLT_MAX;
    }

    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t length) {
        int result = 1;
        return SafeCompare(reinterpret_cast<const void*>(va), expected, length, &result) && result == 0;
    }

    bool Read(uintptr_t va, void* out, size_t length) {
        return SafeCopy(out, reinterpret_cast<const void*>(va), length) != 0;
    }

    bool WriteData(uintptr_t va, const void* bytes, size_t length) {
        return SafeCopy(reinterpret_cast<void*>(va), bytes, length) != 0;
    }

    bool WriteCode(uintptr_t va, const void* bytes, size_t length) {
        void* target = reinterpret_cast<void*>(va);
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        std::memcpy(target, bytes, length);
        FlushInstructionCache(GetCurrentProcess(), target, length);
        DWORD unused = 0;
        VirtualProtect(target, length, oldProtect, &unused);
        return true;
    }

}

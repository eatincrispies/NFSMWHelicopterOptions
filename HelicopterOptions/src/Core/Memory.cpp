#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include "Memory.h"

namespace Memory {

    static int SehMemcmp(const void* a, const void* b, size_t len, int* result) {
        __try {
            *result = memcmp(a, b, len);
            return 1;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static int SehMemcpyRead(void* dst, const void* src, size_t len) {
        __try {
            memcpy(dst, src, len);
            return 1;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t len) {
        int cmp = 1;
        if (!SehMemcmp(reinterpret_cast<const void*>(va), expected, len, &cmp)) return false;
        return cmp == 0;
    }

    bool ReadBytes(uintptr_t va, uint8_t* out, size_t len) {
        return SehMemcpyRead(out, reinterpret_cast<const void*>(va), len) != 0;
    }

    bool WriteBytes(uintptr_t va, const void* src, size_t len) {
        void* dst = reinterpret_cast<void*>(va);
        DWORD oldProtect = 0;
        if (!VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        memcpy(dst, src, len);
        FlushInstructionCache(GetCurrentProcess(), dst, len);
        DWORD ignored = 0;
        VirtualProtect(dst, len, oldProtect, &ignored);
        return true;
    }

    bool ReadPtr(uintptr_t va, void** out)   { return SehMemcpyRead(out, reinterpret_cast<const void*>(va), sizeof(void*)) != 0; }
    bool ReadFloat(uintptr_t va, float* out) { return SehMemcpyRead(out, reinterpret_cast<const void*>(va), sizeof(float)) != 0; }
    bool ReadU8(uintptr_t va, uint8_t* out)  { return SehMemcpyRead(out, reinterpret_cast<const void*>(va), sizeof(uint8_t)) != 0; }

} // namespace Memory

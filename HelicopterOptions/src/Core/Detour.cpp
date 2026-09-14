#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include "Detour.h"
#include "Log.h"
#include "Memory.h"

namespace Detour {

    namespace {

        constexpr int    kMaxDetours     = 8;
        constexpr size_t kMaxStolenBytes = 8;

        struct Installed {
            const char* name;
            uintptr_t   va;
            uint8_t     original[kMaxStolenBytes];
            size_t      length;
        };

        Installed gDetours[kMaxDetours] = {};
        int       gCount = 0;

        uint8_t* BuildTrampoline(uintptr_t va, const uint8_t* stolen, size_t length, Entry entry) {
            auto* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (!code) return nullptr;

            uint8_t* p = code;
            *p++ = 0x9C;
            *p++ = 0x60;
            *p++ = 0xFC;
            *p++ = 0x54;
            *p++ = 0xE8;
            const intptr_t call = reinterpret_cast<intptr_t>(entry) - reinterpret_cast<intptr_t>(p + 4);
            std::memcpy(p, &call, 4);
            p += 4;
            *p++ = 0x83;
            *p++ = 0xC4;
            *p++ = 0x04;
            *p++ = 0x61;
            *p++ = 0x9D;
            std::memcpy(p, stolen, length);
            p += length;
            *p++ = 0xE9;
            const intptr_t back = static_cast<intptr_t>(va + length) - reinterpret_cast<intptr_t>(p + 4);
            std::memcpy(p, &back, 4);

            FlushInstructionCache(GetCurrentProcess(), code, 64);
            return code;
        }

    }

    bool Install(const char* name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry) {
        for (int i = 0; i < gCount; ++i)
            if (gDetours[i].va == va) return true;

        if (length < 5 || length > kMaxStolenBytes || gCount == kMaxDetours) {
            Log::Error("%s was not hooked: the detour table is full or the prologue length is wrong.", name);
            return false;
        }

        if (!Memory::CheckBytes(va, prologue, length)) {
            uint8_t actual[kMaxStolenBytes] = {};
            char actualHex[kMaxStolenBytes * 2 + 1] = "unreadable";
            if (Memory::Read(va, actual, length))
                Log::Hex(actual, static_cast<unsigned>(length), actualHex, sizeof(actualHex));
            Log::Warn("%s was not hooked: its first bytes are %s, so another mod has already changed it.", name, actualHex);
            return false;
        }

        const uint8_t* trampoline = BuildTrampoline(va, prologue, length, entry);
        if (!trampoline) {
            Log::Error("%s was not hooked: no memory for the trampoline.", name);
            return false;
        }

        uint8_t jump[kMaxStolenBytes];
        jump[0] = 0xE9;
        const intptr_t relative = reinterpret_cast<intptr_t>(trampoline) - static_cast<intptr_t>(va + 5);
        std::memcpy(jump + 1, &relative, 4);
        for (size_t i = 5; i < length; ++i) jump[i] = 0x90;

        if (!Memory::WriteCode(va, jump, length)) {
            Log::Error("%s was not hooked: the jump could not be written.", name);
            return false;
        }

        Installed& slot = gDetours[gCount++];
        slot.name = name;
        slot.va = va;
        slot.length = length;
        std::memcpy(slot.original, prologue, length);
        Log::Info("%s hooked at 0x%08lX.", name, static_cast<unsigned long>(va));
        return true;
    }

    void RemoveAll() {
        while (gCount > 0) {
            const Installed& slot = gDetours[--gCount];
            if (!Memory::WriteCode(slot.va, slot.original, slot.length))
                Log::Warn("%s could not be unhooked.", slot.name);
        }
    }

}

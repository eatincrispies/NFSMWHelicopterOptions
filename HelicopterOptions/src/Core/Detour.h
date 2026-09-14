#pragma once
#include <cstddef>
#include <cstdint>

namespace Detour {

    struct Registers {
        uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
        uint32_t eflags;
    };

    using Entry = void (__cdecl*)(Registers* registers);

    bool Install(const char* name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry);
    void RemoveAll();

}

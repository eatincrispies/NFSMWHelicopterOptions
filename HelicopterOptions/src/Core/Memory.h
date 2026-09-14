#pragma once
#include <cstddef>
#include <cstdint>

namespace Memory {

    bool IsFinite(float value);
    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t length);
    bool Read(uintptr_t va, void* out, size_t length);
    bool WriteData(uintptr_t va, const void* bytes, size_t length);
    bool WriteCode(uintptr_t va, const void* bytes, size_t length);

}

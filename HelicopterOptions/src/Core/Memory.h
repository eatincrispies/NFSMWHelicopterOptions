// Memory.h - guarded low-level memory access.
#pragma once
#include <cstdint>
#include <cstddef>

namespace Memory {

    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t len);
    bool ReadBytes(uintptr_t va, uint8_t* out, size_t len);
    bool WriteBytes(uintptr_t va, const void* src, size_t len);

    bool ReadPtr(uintptr_t va, void** out);
    bool ReadFloat(uintptr_t va, float* out);
    bool ReadU8(uintptr_t va, uint8_t* out);

} // namespace Memory

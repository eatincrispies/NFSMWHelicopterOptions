// PatchManager.h - transactional, journaled patching.
//
//  * Every executable write is preceded by a byte guard.
//  * Guard failures log expected vs actual bytes.
//  * Patches are grouped; a group applies all-or-nothing.
//  * Every write (code or data) is journaled and restored on safe unload.
#pragma once
#include <cstdint>
#include <cstddef>

namespace Patch {

    void Begin(const char* groupName);

    void AddFloatOperand(const char* name, uintptr_t insnVA,
                         const uint8_t* guard6, const float* replacement);

    void AddBytes(const char* name, uintptr_t va,
                  const uint8_t* guard, size_t guardLen,
                  const uint8_t* replacement, size_t replLen);

    void AddImm8(const char* name, uintptr_t insnVA,
                 const uint8_t* guard, size_t guardLen,
                 uintptr_t immVA, uint8_t value);

    void AddImm32(const char* name, uintptr_t insnVA,
                  const uint8_t* guard, size_t guardLen,
                  uintptr_t immVA, uint32_t value);

    void AddDataFloat(const char* name, uintptr_t va, float expected, float value);
    void AddDataBool(const char* name, uintptr_t va, uint8_t value);

    bool Commit();

    int  AppliedCount();
    int  SkippedGroupCount();
    int  FailedCount();

    void RestoreAll();

} // namespace Patch

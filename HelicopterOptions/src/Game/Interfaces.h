#pragma once
#include <cstddef>
#include <cstdint>

namespace Log {

#if defined(_DEBUG)
    void Open(void* module);
    void Close();
    void CheckRotate();

    void Info(const char* format, ...);
    void Warn(const char* format, ...);
    void Error(const char* format, ...);

    void Hex(const unsigned char* bytes, unsigned length, char* out, unsigned outLength);
#else
    inline void Open(void*) {}
    inline void Close() {}
    inline void CheckRotate() {}

    template <typename... Args> void Info(const char*, const Args&...) {}
    template <typename... Args> void Warn(const char*, const Args&...) {}
    template <typename... Args> void Error(const char*, const Args&...) {}

    inline void Hex(const unsigned char*, unsigned, char* out, unsigned outLength) {
        if (outLength) out[0] = '\0';
    }
#endif

}

namespace Memory {

    bool IsFinite(float value);
    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t length);
    bool Read(uintptr_t va, void* out, size_t length);
    bool WriteData(uintptr_t va, const void* bytes, size_t length);
    bool WriteCode(uintptr_t va, const void* bytes, size_t length);

}

namespace Detour {

    struct Registers {
        uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
        uint32_t eflags;
    };

    using Entry = void (__cdecl*)(Registers* registers);

    bool Install(const char* name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry);
    void RemoveAll();

}

namespace Patch {

    struct FloatOperand {
        uintptr_t va;
        uint8_t   opcode[2];
        uintptr_t constant;
    };

    struct FloatPush {
        uintptr_t va;
        float     value;
    };

    struct CallSite {
        uintptr_t va;
        uintptr_t target;
    };

    void Begin(const char* group);
    void RedirectFloat(const char* name, const FloatOperand& site, const float* value);
    void PushFloat(const char* name, const FloatPush& site, float value);
    void RedirectCall(const char* name, const CallSite& site, const void* replacement);
    void DataFloat(const char* name, uintptr_t va, float vanilla, float value);
    bool Commit();

    bool RewritePushedFloat(const FloatPush& site, float from, float to);

    int  SkippedGroups();
    int  FailedGroups();
    void RestoreAll();

}

namespace Interfaces {

    bool ReadRigidBody(void* simable, void** rigidBody);
    bool ReadPosition(void* rigidBody, float position[3]);
    bool ReadPlayerRigidBody(void** rigidBody);
    bool ReadPlayerPosition(float position[3]);

}

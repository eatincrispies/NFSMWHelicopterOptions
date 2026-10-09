#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Log {

#if defined(_DEBUG)
    using Name = const char*;

    void Open(void* module);
    void Close();
    void CheckRotate();

    void Info(const char* format, ...);
    void Warn(const char* format, ...);
    void Error(const char* format, ...);
#else
    struct Name {
        constexpr Name(const char* = nullptr) {}
    };

    inline void Open(void*) {}
    inline void Close() {}
    inline void CheckRotate() {}

    template <typename... Args> void Info(const char*, const Args&...) {}
    template <typename... Args> void Warn(const char*, const Args&...) {}
    template <typename... Args> void Error(const char*, const Args&...) {}
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

    bool Install(Log::Name name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry);
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

    void Begin(Log::Name group);
    void RedirectFloat(Log::Name name, const FloatOperand& site, const float* value);
    void PushFloat(Log::Name name, const FloatPush& site, float value);
    void RedirectCall(Log::Name name, const CallSite& site, const void* replacement);
    void DataFloat(Log::Name name, uintptr_t va, float vanilla, float value);
    bool Commit();

    bool RewritePushedFloat(const FloatPush& site, float from, float to);

    int  SkippedGroups();
    int  FailedGroups();
    void RestoreAll();

}

namespace Game {

    template <typename Return, typename... Args>
    Return ThisCall(uintptr_t function, Args... args) {
        return reinterpret_cast<Return (__thiscall*)(Args...)>(function)(args...);
    }

    template <typename Method>
    const void* MethodAddress(Method method) {
        static_assert(sizeof(method) == sizeof(void*), "only plain member functions have a single code address");
        const void* address = nullptr;
        std::memcpy(&address, &method, sizeof(address));
        return address;
    }

    template <typename T>
    T& Global(uintptr_t va) {
        return *reinterpret_cast<T*>(va);
    }

    inline uintptr_t VirtualFunction(const void* object, unsigned slot) {
        return (*static_cast<const uintptr_t* const*>(object))[slot / sizeof(uintptr_t)];
    }

}

struct Config {
    float LeadBase                = 30.0f;
    float LeadMax                 = 45.0f;

    float CrushHover              = 0.0f;
    float CrushHeight             = 0.0f;

    float TurnClamp               = 1.3f;
    float TurnResponseScale       = -8.0f;
    float MaxChopperAccel         = 80.0f;
    float MinChopperAccel         = 30.0f;

    float SpeedCap                = 100.0f;
    float LineOfSight             = 0.0f;
    float FuelTime                = 0.0f;
    bool  HeliSheet               = true;
    float IgnoreHeliSheetDistance = 0.0f;

    float FlySpeed                = 100.0f;
    float SpawnDistance           = 250.0f;
};

extern Config gCfg;

namespace Ini {

    void Load(void* module);
    bool ApplyHeat(int level, bool racing);

}

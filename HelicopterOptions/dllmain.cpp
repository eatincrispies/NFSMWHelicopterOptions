#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>
#include <vector>
#include "dllmain.hpp"
#include "src/Helicopter/AIActionHeliExit.hpp"
#include "src/Helicopter/AIActionHeliPursuit.hpp"
#include "src/Helicopter/AIVehicleHelicopter.hpp"
#include "src/Helicopter/HeliSheet.hpp"
#include "src/Helicopter/SimpleChopper.hpp"

#if defined(_DEBUG)

namespace Log {

    namespace {

        constexpr long kMaxBytes = 4L * 1024L * 1024L;

        FILE*            gFile = nullptr;
        CRITICAL_SECTION gLock;
        bool             gLockReady = false;
        char             gPath[MAX_PATH] = "";
        DWORD            gStartMs = 0;

        void WriteHeader() {
            const std::time_t now = std::time(nullptr);
            std::fprintf(gFile, "NFSMWHelicopterOptions trace  %s", std::ctime(&now));
            std::fprintf(gFile, "%10s %5s %3s %s\n", "t", "tid", "lvl", "event");
            std::fflush(gFile);
        }

        void Write(const char* level, const char* format, va_list args) {
            char text[1024];
            std::vsnprintf(text, sizeof(text), format, args);
            const DWORD elapsed = GetTickCount() - gStartMs;
            char line[1100];
            std::snprintf(line, sizeof(line), "%6lu.%03lu %5lu %s %s\n", elapsed / 1000, elapsed % 1000, GetCurrentThreadId(), level, text);
            char debugLine[1124];
            std::snprintf(debugLine, sizeof(debugLine), "[HelicopterOptions] %s", line);

            if (gLockReady) EnterCriticalSection(&gLock);
            OutputDebugStringA(debugLine);
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
        gStartMs = GetTickCount();

        char directory[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), directory, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return;
        if (char* slash = std::strrchr(directory, '\\')) slash[1] = '\0';

        char folder[MAX_PATH];
        std::snprintf(folder, sizeof(folder), "%sHelicopterOptions", directory);
        CreateDirectoryA(folder, nullptr);
        std::snprintf(folder, sizeof(folder), "%sHelicopterOptions\\Logs", directory);
        CreateDirectoryA(folder, nullptr);
        std::snprintf(gPath, sizeof(gPath), "%s\\NFSMWHelicopterOptions.log", folder);

        gFile = std::fopen(gPath, "w");
        if (gFile) WriteHeader();
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
            if (gFile) WriteHeader();
        }
        if (gLockReady) LeaveCriticalSection(&gLock);
    }

    void Info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("INF", format, args);
        va_end(args);
    }

    void Warn(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("WRN", format, args);
        va_end(args);
    }

    void Error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("ERR", format, args);
        va_end(args);
    }

}

#endif

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

#if defined(_DEBUG)
    void DescribeBytes(uintptr_t va, size_t length, char* out, size_t outLength) {
        static const char digits[] = "0123456789ABCDEF";
        uint8_t bytes[16] = {};
        if (length > sizeof(bytes) || !Memory::Read(va, bytes, length)) {
            std::snprintf(out, outLength, "unreadable");
            return;
        }
        size_t i = 0;
        for (; i < length && i * 2 + 2 < outLength; ++i) {
            out[i * 2]     = digits[bytes[i] >> 4];
            out[i * 2 + 1] = digits[bytes[i] & 0x0F];
        }
        out[i * 2] = '\0';
    }
#else
    void DescribeBytes(uintptr_t, size_t, char* out, size_t outLength) {
        if (outLength) out[0] = '\0';
    }
#endif

}

namespace Detour {

    namespace {

        constexpr int    kMaxDetours     = 8;
        constexpr size_t kMaxStolenBytes = 8;

        constexpr uint8_t kPushfd    = 0x9C;
        constexpr uint8_t kPushad    = 0x60;
        constexpr uint8_t kCld       = 0xFC;
        constexpr uint8_t kPushEsp   = 0x54;
        constexpr uint8_t kCall      = 0xE8;
        constexpr uint8_t kAddEsp4[] = { 0x83, 0xC4, 0x04 };
        constexpr uint8_t kPopad     = 0x61;
        constexpr uint8_t kPopfd     = 0x9D;
        constexpr uint8_t kJmp       = 0xE9;
        constexpr uint8_t kNop       = 0x90;

        struct Installed {
            Log::Name name;
            uintptr_t va;
            uint8_t   original[kMaxStolenBytes];
            size_t    length;
        };

        Installed gDetours[kMaxDetours] = {};
        int       gCount = 0;

        uint8_t* WriteRelative(uint8_t* p, uint8_t opcode, intptr_t target) {
            *p++ = opcode;
            const intptr_t relative = target - reinterpret_cast<intptr_t>(p + 4);
            std::memcpy(p, &relative, 4);
            return p + 4;
        }

        uint8_t* BuildTrampoline(uintptr_t va, const uint8_t* stolen, size_t length, Entry entry) {
            auto* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (!code) return nullptr;

            uint8_t* p = code;
            *p++ = kPushfd;
            *p++ = kPushad;
            *p++ = kCld;
            *p++ = kPushEsp;
            p = WriteRelative(p, kCall, reinterpret_cast<intptr_t>(entry));
            std::memcpy(p, kAddEsp4, sizeof(kAddEsp4));
            p += sizeof(kAddEsp4);
            *p++ = kPopad;
            *p++ = kPopfd;
            std::memcpy(p, stolen, length);
            p += length;
            WriteRelative(p, kJmp, static_cast<intptr_t>(va + length));

            FlushInstructionCache(GetCurrentProcess(), code, 64);
            return code;
        }

    }

    bool Install(Log::Name name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry) {
        for (int i = 0; i < gCount; ++i)
            if (gDetours[i].va == va) return true;

        if (length < 5 || length > kMaxStolenBytes || gCount == kMaxDetours) {
            Log::Error("detour %s  0x%08X len=%u slots=%d/%d  length or table out of range", name, va, static_cast<unsigned>(length), gCount,
                       kMaxDetours);
            return false;
        }

        char expected[kMaxStolenBytes * 2 + 1];
        Memory::DescribeBytes(reinterpret_cast<uintptr_t>(prologue), length, expected, sizeof(expected));

        if (!Memory::CheckBytes(va, prologue, length)) {
            char actual[kMaxStolenBytes * 2 + 1];
            Memory::DescribeBytes(va, length, actual, sizeof(actual));
            Log::Warn("detour %s  0x%08X len=%u read %s, expected %s  SKIP", name, va, static_cast<unsigned>(length), actual, expected);
            return false;
        }

        const uint8_t* trampoline = BuildTrampoline(va, prologue, length, entry);
        if (!trampoline) {
            Log::Error("detour %s  0x%08X VirtualAlloc(64, PAGE_EXECUTE_READWRITE) GetLastError=%lu", name, va, GetLastError());
            return false;
        }

        uint8_t jump[kMaxStolenBytes];
        const intptr_t relative = reinterpret_cast<intptr_t>(trampoline) - static_cast<intptr_t>(va + 5);
        jump[0] = kJmp;
        std::memcpy(jump + 1, &relative, 4);
        for (size_t i = 5; i < length; ++i) jump[i] = kNop;

        if (!Memory::WriteCode(va, jump, length)) {
            Log::Error("detour %s  0x%08X VirtualProtect GetLastError=%lu", name, va, GetLastError());
            return false;
        }

        Installed& slot = gDetours[gCount++];
        slot.name = name;
        slot.va = va;
        slot.length = length;
        std::memcpy(slot.original, prologue, length);

        char written[kMaxStolenBytes * 2 + 1];
        Memory::DescribeBytes(va, length, written, sizeof(written));
        Log::Info("detour %s  0x%08X len=%u %s -> %s  trampoline=0x%p entry=0x%p  slot %d/%d", name, va, static_cast<unsigned>(length), expected,
                  written, trampoline, entry, gCount, kMaxDetours);
        return true;
    }

    void RemoveAll() {
        while (gCount > 0) {
            const Installed& slot = gDetours[--gCount];
            char current[kMaxStolenBytes * 2 + 1];
            char original[kMaxStolenBytes * 2 + 1];
            Memory::DescribeBytes(slot.va, slot.length, current, sizeof(current));
            Memory::DescribeBytes(reinterpret_cast<uintptr_t>(slot.original), slot.length, original, sizeof(original));
            if (Memory::WriteCode(slot.va, slot.original, slot.length))
                Log::Info("detour %s  0x%08X %s -> %s  removed", slot.name, slot.va, current, original);
            else
                Log::Warn("detour %s  0x%08X holds %s, write of %s failed", slot.name, slot.va, current, original);
        }
    }

}

namespace Patch {

    namespace {

        constexpr uint8_t kPushImm32 = 0x68;
        constexpr uint8_t kCallRel32 = 0xE8;

        enum Kind : uint8_t {
            kFloatOperand,
            kFloatPush,
            kCall,
            kDataFloat,
        };

        struct Entry {
            Log::Name name;
            Kind      kind;
            uintptr_t guardVa;
            uint8_t   guard[6];
            size_t    guardLength;
            uintptr_t writeVa;
            uint8_t   bytes[4];
            float     vanilla;
        };

        struct Original {
            Log::Name name;
            uintptr_t va;
            uint8_t   bytes[4];
        };

        std::vector<Entry>    gPending;
        std::vector<Original> gJournal;
        Log::Name             gGroup = "";
        int                   gSkipped = 0;
        int                   gFailed = 0;

        bool CheckData(const Entry& entry) {
            float current = 0.0f;
            float wanted = 0.0f;
            std::memcpy(&wanted, entry.bytes, sizeof(wanted));
            if (!Memory::Read(entry.guardVa, &current, sizeof(current))) {
                Log::Warn("patch %s | %s  [0x%08X] unreadable", gGroup, entry.name, entry.guardVa);
                return false;
            }
            if (std::fabs(current - entry.vanilla) <= 0.001f || std::fabs(current - wanted) <= 0.001f)
                return true;
            Log::Warn("patch %s | %s  [0x%08X]=%g, expected %g or %g", gGroup, entry.name, entry.guardVa, current, entry.vanilla, wanted);
            return false;
        }

        bool CheckCode(const Entry& entry) {
            if (Memory::CheckBytes(entry.guardVa, entry.guard, entry.guardLength)) return true;
            char actual[16];
            char expected[16];
            Memory::DescribeBytes(entry.guardVa, entry.guardLength, actual, sizeof(actual));
            Memory::DescribeBytes(reinterpret_cast<uintptr_t>(entry.guard), entry.guardLength, expected, sizeof(expected));
            Log::Warn("patch %s | %s  0x%08X read %s, expected %s", gGroup, entry.name, entry.guardVa, actual, expected);
            return false;
        }

        bool Check(const Entry& entry) {
            return entry.kind == kDataFloat ? CheckData(entry) : CheckCode(entry);
        }

        void PushBytes(uint8_t* out, float value) {
            out[0] = kPushImm32;
            std::memcpy(out + 1, &value, sizeof(value));
        }

        uint32_t CallOffset(uintptr_t call, uintptr_t target) {
            return static_cast<uint32_t>(target) - static_cast<uint32_t>(call + 5);
        }

#if defined(_DEBUG)
        const char* FpuMnemonic(const uint8_t* opcode) {
            static const char* const kD8[] = { "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr" };
            static const char* const kD9[] = { "fld", "?", "fst", "fstp", "fldenv", "fldcw", "fnstenv", "fnstcw" };
            const int reg = (opcode[1] >> 3) & 7;
            if (opcode[0] == 0xD8) return kD8[reg];
            if (opcode[0] == 0xD9) return kD9[reg];
            return "?";
        }

        float ReadFloat(uintptr_t va) {
            float value = 0.0f;
            Memory::Read(va, &value, sizeof(value));
            return value;
        }

        void LogWrite(const Entry& entry, const uint8_t* before) {
            char from[9];
            char to[9];
            Memory::DescribeBytes(reinterpret_cast<uintptr_t>(before), 4, from, sizeof(from));
            Memory::DescribeBytes(entry.writeVa, 4, to, sizeof(to));

            uint32_t oldWord = 0;
            uint32_t newWord = 0;
            float    oldFloat = 0.0f;
            float    newFloat = 0.0f;
            std::memcpy(&oldWord, before, sizeof(oldWord));
            std::memcpy(&newWord, entry.bytes, sizeof(newWord));
            std::memcpy(&oldFloat, before, sizeof(oldFloat));
            std::memcpy(&newFloat, entry.bytes, sizeof(newFloat));

            switch (entry.kind) {
            case kFloatOperand:
                Log::Info("patch %s | %s  0x%08X %02X %02X %s [0x%08X]=%g -> [0x%08X]=%g  @0x%08X %s -> %s", gGroup, entry.name, entry.guardVa,
                          entry.guard[0], entry.guard[1], FpuMnemonic(entry.guard), oldWord, ReadFloat(oldWord), newWord, ReadFloat(newWord),
                          entry.writeVa, from, to);
                break;
            case kFloatPush:
                Log::Info("patch %s | %s  0x%08X 68 push %g -> %g  @0x%08X %s -> %s", gGroup, entry.name, entry.guardVa, oldFloat, newFloat,
                          entry.writeVa, from, to);
                break;
            case kCall:
                Log::Info("patch %s | %s  0x%08X E8 call 0x%08X -> 0x%08X  @0x%08X %s -> %s", gGroup, entry.name, entry.guardVa,
                          entry.guardVa + 5 + oldWord, entry.guardVa + 5 + newWord, entry.writeVa, from, to);
                break;
            case kDataFloat:
                Log::Info("patch %s | %s  [0x%08X] float %g -> %g  %s -> %s", gGroup, entry.name, entry.writeVa, oldFloat, newFloat, from, to);
                break;
            }
        }
#else
        void LogWrite(const Entry&, const uint8_t*) {}
#endif

    }

    void Begin(Log::Name group) {
        gPending.clear();
        gGroup = group;
    }

    void RedirectFloat(Log::Name name, const FloatOperand& site, const float* value) {
        Entry entry = {};
        entry.name = name;
        entry.kind = kFloatOperand;
        entry.guardVa = site.va;
        entry.guard[0] = site.opcode[0];
        entry.guard[1] = site.opcode[1];
        const uint32_t constant = static_cast<uint32_t>(site.constant);
        std::memcpy(entry.guard + 2, &constant, sizeof(constant));
        entry.guardLength = 6;
        entry.writeVa = site.va + 2;
        const uint32_t target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
        std::memcpy(entry.bytes, &target, sizeof(target));
        gPending.push_back(entry);
    }

    void PushFloat(Log::Name name, const FloatPush& site, float value) {
        Entry entry = {};
        entry.name = name;
        entry.kind = kFloatPush;
        entry.guardVa = site.va;
        PushBytes(entry.guard, site.value);
        entry.guardLength = 5;
        entry.writeVa = site.va + 1;
        std::memcpy(entry.bytes, &value, sizeof(value));
        gPending.push_back(entry);
    }

    void RedirectCall(Log::Name name, const CallSite& site, const void* replacement) {
        Entry entry = {};
        entry.name = name;
        entry.kind = kCall;
        entry.guardVa = site.va;
        entry.guard[0] = kCallRel32;
        const uint32_t original = CallOffset(site.va, site.target);
        std::memcpy(entry.guard + 1, &original, sizeof(original));
        entry.guardLength = 5;
        entry.writeVa = site.va + 1;
        const uint32_t offset = CallOffset(site.va, reinterpret_cast<uintptr_t>(replacement));
        std::memcpy(entry.bytes, &offset, sizeof(offset));
        gPending.push_back(entry);
    }

    void DataFloat(Log::Name name, uintptr_t va, float vanilla, float value) {
        Entry entry = {};
        entry.name = name;
        entry.kind = kDataFloat;
        entry.guardVa = va;
        entry.writeVa = va;
        entry.vanilla = vanilla;
        std::memcpy(entry.bytes, &value, sizeof(value));
        gPending.push_back(entry);
    }

    bool Commit() {
        for (const Entry& entry : gPending) {
            if (!Check(entry)) {
                Log::Warn("patch %s  SKIP  %u write(s) discarded", gGroup, static_cast<unsigned>(gPending.size()));
                ++gSkipped;
                gPending.clear();
                return false;
            }
        }

        const size_t start = gJournal.size();
        for (const Entry& entry : gPending) {
            Original original = {};
            original.name = entry.name;
            original.va = entry.writeVa;
            if (!Memory::Read(entry.writeVa, original.bytes, sizeof(original.bytes))
                || !Memory::WriteCode(entry.writeVa, entry.bytes, sizeof(entry.bytes))) {
                Log::Error("patch %s | %s  @0x%08X write failed GetLastError=%lu, %u write(s) rolled back", gGroup, entry.name, entry.writeVa,
                           GetLastError(), static_cast<unsigned>(gJournal.size() - start));
                while (gJournal.size() > start) {
                    const Original& undo = gJournal.back();
                    Memory::WriteCode(undo.va, undo.bytes, sizeof(undo.bytes));
                    gJournal.pop_back();
                }
                ++gFailed;
                gPending.clear();
                return false;
            }
            LogWrite(entry, original.bytes);
            gJournal.push_back(original);
        }

        Log::Info("patch %s  OK  %u write(s), journal %u", gGroup, static_cast<unsigned>(gPending.size()), static_cast<unsigned>(gJournal.size()));
        gPending.clear();
        return true;
    }

    bool RewritePushedFloat(Log::Name name, const FloatPush& site, float from, float to) {
        uint8_t expected[5];
        PushBytes(expected, from);
        if (!Memory::CheckBytes(site.va, expected, sizeof(expected))) {
            char actual[16];
            char wanted[16];
            Memory::DescribeBytes(site.va, sizeof(expected), actual, sizeof(actual));
            Memory::DescribeBytes(reinterpret_cast<uintptr_t>(expected), sizeof(expected), wanted, sizeof(wanted));
            Log::Warn("patch %s  0x%08X read %s, expected %s  live rewrite off", name, site.va, actual, wanted);
            return false;
        }
        if (!Memory::WriteCode(site.va + 1, &to, sizeof(to))) {
            Log::Error("patch %s  @0x%08X write failed GetLastError=%lu", name, site.va + 1, GetLastError());
            return false;
        }
        Log::Info("patch %s  0x%08X 68 push %g -> %g", name, site.va, from, to);
        return true;
    }

    int SkippedGroups() {
        return gSkipped;
    }

    int FailedGroups() {
        return gFailed;
    }

    void RestoreAll() {
        while (!gJournal.empty()) {
            const Original& original = gJournal.back();
            char current[9];
            char restored[9];
            Memory::DescribeBytes(original.va, sizeof(original.bytes), current, sizeof(current));
            Memory::DescribeBytes(reinterpret_cast<uintptr_t>(original.bytes), sizeof(original.bytes), restored, sizeof(restored));
            if (Memory::WriteCode(original.va, original.bytes, sizeof(original.bytes)))
                Log::Info("restore %s  @0x%08X %s -> %s", original.name, original.va, current, restored);
            else
                Log::Warn("restore %s  @0x%08X holds %s, write of %s failed", original.name, original.va, current, restored);
            gJournal.pop_back();
        }
    }

}

namespace {

    constexpr uintptr_t kIPlayerLists = 0x0092D848u;

    struct IPlayerList {
        void*     mVTable;
        IPlayer** mBegin;
        size_t    mCapacity;
        size_t    mSize;
        IPlayer*  mStorage[8];
    };

    static_assert(sizeof(IPlayerList) == 0x30, "ListableSet<IPlayer, 8, ePlayerList, 3>::List is 0x30 bytes");

}

IPlayer* IPlayer::First(ePlayerList idx) {
    const IPlayerList& list = Game::Global<IPlayerList[PLAYER_MAX]>(kIPlayerLists)[idx];
    return list.mSize != 0 ? list.mBegin[0] : nullptr;
}

IRigidBody* GetLocalPlayerRigidBody() {
    IPlayer* player = IPlayer::First(PLAYER_LOCAL);
    ISimable* simable = player != nullptr ? player->GetSimable() : nullptr;
    return simable != nullptr ? simable->GetRigidBody() : nullptr;
}

float UMath::Distancexz(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

namespace Ini {

    namespace {

        constexpr int kLevels    = 10;
        constexpr int kMostParts = 4;

        struct SettingsTable {
            const Setting* settings;
            int            count;
        };

        template <size_t Count>
        constexpr SettingsTable Table(const Setting (&settings)[Count]) {
            return { settings, static_cast<int>(Count) };
        }

        const SettingsTable kTables[] = {
            Table(AIActionHeliPursuit::kIniSettings),
            Table(SimpleChopper::kIniSettings),
            Table(AIVehicleHelicopter::kIniSettings),
            Table(HeliSheet::kIniSettings),
            Table(AIActionHeliExit::kIniSettings),
        };

        constexpr int kCount = static_cast<int>(std::size(AIActionHeliPursuit::kIniSettings) + std::size(SimpleChopper::kIniSettings)
                                                + std::size(AIVehicleHelicopter::kIniSettings) + std::size(HeliSheet::kIniSettings)
                                                + std::size(AIActionHeliExit::kIniSettings));

        const Setting* gSettings[kCount] = {};

        void GatherSettings() {
            int index = 0;
            for (const SettingsTable& table : kTables)
                for (int i = 0; i < table.count; ++i)
                    gSettings[index++] = &table.settings[i];
        }

        struct Values {
            float    vanilla;
            float    base;
            float    heat[kLevels];
            float    race[kLevels];
            bool     hasBase;
            uint16_t hasHeat;
            uint16_t hasRace;
        };

        Values gValues[kCount];
        int    gLevel = -1;
        bool   gRacing = false;

        float Get(int index) {
            const Setting& setting = *gSettings[index];
            if (setting.toggle) return *setting.toggle ? 1.0f : 0.0f;
            return *setting.number;
        }

        void Set(int index, float value) {
            const Setting& setting = *gSettings[index];
            if (setting.toggle)
                *setting.toggle = value != 0.0f;
            else
                *setting.number = value;
        }

        void Trim(char* text) {
            char* start = text;
            while (*start == ' ' || *start == '\t') ++start;
            if (start != text) std::memmove(text, start, std::strlen(start) + 1);
            size_t length = std::strlen(text);
            while (length && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n'))
                text[--length] = '\0';
        }

        int FindSection(const char* section) {
            for (int i = 0; i < kCount; ++i)
                if (_stricmp(gSettings[i]->section, section) == 0) return i;
            return -1;
        }

        int PartCount(int first) {
            int count = 1;
            while (first + count < kCount && std::strcmp(gSettings[first + count]->section, gSettings[first]->section) == 0) ++count;
            return count;
        }

        int ParseLevel(const char* key, bool* race) {
            if (_stricmp(key, "default") == 0) return 0;
            if (_strnicmp(key, "heat", 4) == 0)
                *race = false;
            else if (_strnicmp(key, "race", 4) == 0)
                *race = true;
            else
                return -1;

            const char* digits = key + 4;
            if (digits[0] < '0' || digits[0] > '9' || digits[1] < '0' || digits[1] > '9' || digits[2]) return -1;
            const int level = (digits[0] - '0') * 10 + (digits[1] - '0');
            return level >= 1 && level <= kLevels ? level : -1;
        }

        bool ParseFloat(const char* text, float* out) {
            char* end = nullptr;
            const double value = std::strtod(text, &end);
            if (end == text) return false;
            while (*end == ' ' || *end == '\t') ++end;
            if (*end || !(value == value) || value > FLT_MAX || value < -FLT_MAX) return false;
            *out = static_cast<float>(value);
            return true;
        }

        int SplitParts(char* text, char* parts[kMostParts]) {
            int count = 0;
            char* next = text;
            while (next && count < kMostParts) {
                char* comma = std::strchr(next, ',');
                if (comma) *comma = '\0';
                Trim(next);
                parts[count++] = next;
                next = comma ? comma + 1 : nullptr;
            }
            return next ? kMostParts + 1 : count;
        }

        bool ParseValue(const Setting& setting, const char* section, const char* key, const char* text, int lineNumber, float* value,
                        int* problems) {
            if (setting.toggle) {
                if (_stricmp(text, "true") != 0 && _stricmp(text, "false") != 0) {
                    Log::Warn("ini L%d [%s] %s %s=%s  not bool", lineNumber, section, key, setting.name, text);
                    ++*problems;
                    return false;
                }
                *value = _stricmp(text, "true") == 0 ? 1.0f : 0.0f;
                return true;
            }
            if (!ParseFloat(text, value)) {
                Log::Warn("ini L%d [%s] %s %s=%s  not float", lineNumber, section, key, setting.name, text);
                ++*problems;
                return false;
            }
            if (*value < setting.min || *value > setting.max) {
                const float clamped = *value < setting.min ? setting.min : setting.max;
                Log::Warn("ini L%d [%s] %s %s=%g  clamp [%g, %g] -> %g", lineNumber, section, key, setting.name, *value, setting.min, setting.max,
                          clamped);
                *value = clamped;
                ++*problems;
            }
            return true;
        }

        bool Store(int index, int level, bool race, float value) {
            Values& values = gValues[index];
            if (level == 0) {
                if (values.hasBase) return false;
                values.base = value;
                values.hasBase = true;
                return true;
            }
            const uint16_t bit = static_cast<uint16_t>(1u << (level - 1));
            uint16_t& mask = race ? values.hasRace : values.hasHeat;
            if (mask & bit) return false;
            (race ? values.race : values.heat)[level - 1] = value;
            mask = static_cast<uint16_t>(mask | bit);
            return true;
        }

        float Resolve(const Values& values, int level, bool racing) {
            if (level >= 1 && level <= kLevels) {
                const uint16_t bit = static_cast<uint16_t>(1u << (level - 1));
                if (racing && (values.hasRace & bit)) return values.race[level - 1];
                if (values.hasHeat & bit) return values.heat[level - 1];
            }
            return values.hasBase ? values.base : values.vanilla;
        }

        void ReadLine(char* text, int lineNumber, char* section, size_t sectionSize, int* first, int* read, int* problems, int* unknownSections) {
            Trim(text);
            if (!*text || *text == ';' || *text == '#') return;

            if (*text == '[') {
                char* close = std::strchr(text, ']');
                if (!close) {
                    Log::Warn("ini L%d \"%s\"  no ']'", lineNumber, text);
                    ++*problems;
                    *first = -1;
                    return;
                }
                *close = '\0';
                std::snprintf(section, sectionSize, "%s", text + 1);
                Trim(section);
                *first = FindSection(section);
                if (*first < 0) {
                    Log::Warn("ini L%d [%s]  unknown section", lineNumber, section);
                    ++*unknownSections;
                }
                return;
            }
            if (*first < 0) return;

            char* equals = std::strchr(text, '=');
            if (!equals) {
                Log::Warn("ini L%d \"%s\"  no '='", lineNumber, text);
                ++*problems;
                return;
            }
            *equals = '\0';
            char* key = text;
            char* valueText = equals + 1;
            Trim(key);
            Trim(valueText);

            bool race = false;
            const int level = ParseLevel(key, &race);
            if (level < 0) {
                Log::Warn("ini L%d [%s] %s  key not default/heat01-10/race01-10", lineNumber, section, key);
                ++*problems;
                return;
            }
            if (level == 0 && !gSettings[*first]->hasDefault) {
                Log::Warn("ini L%d [%s] default  no default slot, heat01-10/race01-10 only", lineNumber, section);
                ++*problems;
                return;
            }

            const int parts = PartCount(*first);
            char* partText[kMostParts] = {};
            if (SplitParts(valueText, partText) != parts) {
                Log::Warn("ini L%d [%s] %s  %d comma value(s) expected", lineNumber, section, key, parts);
                ++*problems;
                return;
            }

            float values[kMostParts] = {};
            for (int part = 0; part < parts; ++part)
                if (!ParseValue(*gSettings[*first + part], section, key, partText[part], lineNumber, &values[part], problems)) return;

            bool stored = true;
            for (int part = 0; part < parts; ++part)
                stored = Store(*first + part, level, race, values[part]) && stored;
            if (!stored) {
                Log::Warn("ini L%d [%s] %s  duplicate, first kept", lineNumber, section, key);
                ++*problems;
                return;
            }
            ++*read;
        }

        bool ReadFile(const char* path) {
            FILE* file = std::fopen(path, "rb");
            if (!file) return false;

            char line[1024];
            char section[128] = "";
            int  first = -1;
            int  lineNumber = 0;
            int  read = 0;
            int  problems = 0;
            int  unknownSections = 0;

            while (std::fgets(line, sizeof(line), file)) {
                ++lineNumber;
                if (!std::strchr(line, '\n') && !std::feof(file)) {
                    int c;
                    while ((c = std::fgetc(file)) != EOF && c != '\n') {}
                }

                char* text = line;
                if (lineNumber == 1 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB
                    && static_cast<unsigned char>(text[2]) == 0xBF)
                    text += 3;
                ReadLine(text, lineNumber, section, sizeof(section), &first, &read, &problems, &unknownSections);
            }
            std::fclose(file);

            if (problems + unknownSections == 0)
                Log::Info("ini %s  lines=%d read=%d", path, lineNumber, read);
            else
                Log::Warn("ini %s  lines=%d read=%d errors=%d unknownSections=%d", path, lineNumber, read, problems, unknownSections);
            return true;
        }

#if defined(_DEBUG)
        const void* Address(const Setting& setting) {
            return setting.toggle ? static_cast<const void*>(setting.toggle) : static_cast<const void*>(setting.number);
        }

        void LogTable() {
            for (int i = 0; i < kCount; ++i) {
                const Setting& setting = *gSettings[i];
                const Values&  values = gValues[i];
                char base[16] = "-";
                if (values.hasBase) std::snprintf(base, sizeof(base), "%g", values.base);
                Log::Info("ini %2d [%s] %s  @0x%p %s=%g vanilla=%g default=%s heat=%03X race=%03X range=[%g, %g]", i, setting.section, setting.name,
                          Address(setting), setting.toggle ? "bool" : "float", Get(i), values.vanilla, base, values.hasHeat, values.hasRace, setting.min,
                          setting.max);
            }
        }

        void LogChange(int index, float previous) {
            const Setting& setting = *gSettings[index];
            Log::Info("  [%s] %s  @0x%p %g -> %g", setting.section, setting.name, Address(setting), previous, Get(index));
        }
#else
        void LogTable() {}
        void LogChange(int, float) {}
#endif

    }

    void Load(void* module) {
        GatherSettings();
        for (int i = 0; i < kCount; ++i)
            gValues[i].vanilla = Get(i);

        char directory[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), directory, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            Log::Error("ini  GetModuleFileNameA length=%lu GetLastError=%lu, vanilla values", length, GetLastError());
            return;
        }
        if (char* slash = std::strrchr(directory, '\\')) slash[1] = '\0';

        char path[MAX_PATH];
        std::snprintf(path, sizeof(path), "%sHelicopterOptions\\Configuration\\General.ini", directory);
        if (!ReadFile(path))
            Log::Warn("ini %s  fopen errno=%d, vanilla values", path, errno);

        for (int i = 0; i < kCount; ++i)
            Set(i, Resolve(gValues[i], 0, false));
        LogTable();
    }

    bool ApplyHeat(int level, bool racing) {
        if (level < 1) level = 1;
        if (level > kLevels) level = kLevels;
        if (level == gLevel && racing == gRacing) return false;
        gLevel = level;
        gRacing = racing;

        int   changed[kCount];
        float previous[kCount];
        int   count = 0;
        for (int i = 0; i < kCount; ++i) {
            const float current = Get(i);
            const float value = Resolve(gValues[i], level, racing);
            if (current == value) continue;
            previous[count] = current;
            changed[count++] = i;
            Set(i, value);
        }

        Log::Info("heat level=%d racing=%d  %d change(s)", level, racing ? 1 : 0, count);
        for (int n = 0; n < count; ++n)
            LogChange(changed[n], previous[n]);

        return count > 0;
    }

}

namespace {

    constexpr uint32_t kSetHeatArgument = 0x08u;

    static_assert(offsetof(AIPerpVehicle, mIOwner) == 0x34, "Behavior::mIOwner");
    static_assert(offsetof(AIPerpVehicle, mIPerpetrator) == 0x758, "AIPerpVehicle's IPerpetrator");
    static_assert(offsetof(AIPerpVehicle, mHeat) == 0x774, "AIPerpVehicle::mHeat");
    static_assert(offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate) == 0x786, "AIPerpVehicle::mWasInRaceEventLastHeatUpdate");

    AIPerpVehicle* gPlayerPerp = nullptr;
    uintptr_t      gPlayerPerpVTable = 0;

    bool IsHeatLevel(float heat) {
        return heat >= 1.0f && heat < 11.0f;
    }

    AIPerpVehicle* FromIPerpetrator(uintptr_t iperp) {
        return iperp ? reinterpret_cast<AIPerpVehicle*>(iperp - offsetof(AIPerpVehicle, mIPerpetrator)) : nullptr;
    }

    uintptr_t IPerpetratorVTable(const AIPerpVehicle* perp) {
        uintptr_t vtable = 0;
        Memory::Read(reinterpret_cast<uintptr_t>(perp) + offsetof(AIPerpVehicle, mIPerpetrator), &vtable, sizeof(vtable));
        return vtable;
    }

    void ApplyHeat(int level, bool racing) {
        if (!Ini::ApplyHeat(level, racing)) return;
        AIActionHeliPursuit::Refresh();
        AIVehicleHelicopter::Refresh();
        SimpleChopper::Refresh();
        AIActionHeliExit::Refresh();
    }

}

void __cdecl AIPerpVehicle::SetHeatEntry(Detour::Registers* registers) {
    AIPerpVehicle* perp = FromIPerpetrator(registers->ecx);
    if (!perp || !perp->IsLocalPlayer()) return;

    float heat = 0.0f;
    const bool read = Memory::Read(registers->esp + kSetHeatArgument, &heat, sizeof(heat));
    gPlayerPerpVTable = IPerpetratorVTable(perp);
    if (perp != gPlayerPerp) {
        gPlayerPerp = perp;
        Log::Info("AIPerpVehicle::SetHeat  ecx=0x%08X this=0x%p IPerpetrator[+0x758] vtbl=0x%08X mIOwner[+0x34]=0x%p mHeat[+0x774]=%g "
                  "[esp+0x08]=%g mWasInRaceEventLastHeatUpdate[+0x786]=%d",
                  registers->ecx, perp, gPlayerPerpVTable, perp->mIOwner, perp->mHeat, heat, perp->mWasInRaceEventLastHeatUpdate);
    }
    if (!read || !IsHeatLevel(heat)) return;
    ApplyHeat(static_cast<int>(heat), perp->mWasInRaceEventLastHeatUpdate);
}

void AIPerpVehicle::UpdateHeat() {
    if (!gPlayerPerp || IPerpetratorVTable(gPlayerPerp) != gPlayerPerpVTable) return;

    float   heat = 0.0f;
    uint8_t racing = 0;
    const uintptr_t va = reinterpret_cast<uintptr_t>(gPlayerPerp);
    if (!Memory::Read(va + offsetof(AIPerpVehicle, mHeat), &heat, sizeof(heat)) || !IsHeatLevel(heat)
        || !Memory::Read(va + offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate), &racing, sizeof(racing)))
        return;
    ApplyHeat(static_cast<int>(heat), racing != 0);
}

bool AIPerpVehicle::IsLocalPlayer() const {
    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    return mIOwner != nullptr && playerRigidBody != nullptr && mIOwner->GetRigidBody() == playerRigidBody;
}

namespace {

    constexpr const char* kVersion = "V3.3.0";

    HMODULE gModule = nullptr;

#if defined(_DEBUG)
    void LogImage(Log::Name label, HMODULE module) {
        const uintptr_t    base = reinterpret_cast<uintptr_t>(module);
        IMAGE_DOS_HEADER   dos = {};
        IMAGE_NT_HEADERS32 nt = {};
        char               path[MAX_PATH] = "";
        GetModuleFileNameA(module, path, MAX_PATH);
        if (!Memory::Read(base, &dos, sizeof(dos)) || !Memory::Read(base + dos.e_lfanew, &nt, sizeof(nt))) {
            Log::Warn("image %s  base=0x%08X PE header unreadable  %s", label, base, path);
            return;
        }
        Log::Info("image %s  base=0x%08X SizeOfImage=0x%08lX EntryPoint=0x%08lX TimeDateStamp=0x%08lX CheckSum=0x%08lX  %s", label, base,
                  nt.OptionalHeader.SizeOfImage, base + nt.OptionalHeader.AddressOfEntryPoint, nt.FileHeader.TimeDateStamp,
                  nt.OptionalHeader.CheckSum, path);
    }
#else
    void LogImage(Log::Name, HMODULE) {}
#endif

    DWORD WINAPI Initialize(void*) {
        Sleep(1000);

        Log::Open(gModule);
        Log::Info("init  NFSMWHelicopterOptions %s  pid=%lu", kVersion, GetCurrentProcessId());
        LogImage("speed.exe", GetModuleHandleA(nullptr));
        LogImage("NFSMWHelicopterOptions.asi", gModule);

        Ini::Load(gModule);

        AIActionHeliPursuit::InstallPatches();
        AIVehicleHelicopter::InstallPatches();
        SimpleChopper::InstallPatches();
        AIActionHeliExit::InstallPatches();

        AIPerpVehicle::HookSetHeat();
        AIActionHeliPursuit::HookConstructor();
        AIVehicleHelicopter::HookOnDriving();
        AIVehicleHelicopter::HookCanSeeTarget();

        if (Patch::SkippedGroups() + Patch::FailedGroups() == 0)
            Log::Info("init  ready  skipped=0 failed=0");
        else
            Log::Warn("init  ready  skipped=%d failed=%d", Patch::SkippedGroups(), Patch::FailedGroups());
        return 0;
    }

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        gModule = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
        break;

    case DLL_PROCESS_DETACH:
        Log::Info("detach  lpReserved=0x%p", reserved);
        if (reserved == nullptr) {
            Detour::RemoveAll();
            SimpleChopper::RestoreSpeedCap();
            Patch::RestoreAll();
        }
        Log::Close();
        break;
    }
    return TRUE;
}

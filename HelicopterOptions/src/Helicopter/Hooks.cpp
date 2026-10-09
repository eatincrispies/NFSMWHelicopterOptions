#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "Hooks.hpp"

Config gCfg;

#if defined(_DEBUG)

namespace Log {

    namespace {

        constexpr long kMaxBytes = 4L * 1024L * 1024L;

        FILE*            gFile = nullptr;
        CRITICAL_SECTION gLock;
        bool             gLockReady = false;
        char             gPath[MAX_PATH] = "";

        void Write(const char* prefix, const char* format, va_list args) {
            char text[1024];
            std::vsnprintf(text, sizeof(text), format, args);
            char line[1100];
            std::snprintf(line, sizeof(line), "[HelicopterOptions] %s%s\n", prefix, text);

            if (gLockReady) EnterCriticalSection(&gLock);
            OutputDebugStringA(line);
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
        if (gFile) {
            const std::time_t now = std::time(nullptr);
            std::fprintf(gFile, "NFSMWHelicopterOptions log\nSession start: %s\n", std::ctime(&now));
            std::fflush(gFile);
        }
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
        }
        if (gLockReady) LeaveCriticalSection(&gLock);
    }

    void Info(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("", format, args);
        va_end(args);
    }

    void Warn(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("WARNING: ", format, args);
        va_end(args);
    }

    void Error(const char* format, ...) {
        va_list args;
        va_start(args, format);
        Write("ERROR: ", format, args);
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

}

namespace {

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
            Log::Error("%s was not hooked: the detour table is full or the prologue length is wrong.", name);
            return false;
        }

        if (!Memory::CheckBytes(va, prologue, length)) {
            char actual[kMaxStolenBytes * 2 + 1];
            DescribeBytes(va, length, actual, sizeof(actual));
            Log::Warn("%s was not hooked: its first bytes are %s, so another mod has already changed it.", name, actual);
            return false;
        }

        const uint8_t* trampoline = BuildTrampoline(va, prologue, length, entry);
        if (!trampoline) {
            Log::Error("%s was not hooked: no memory for the trampoline.", name);
            return false;
        }

        uint8_t jump[kMaxStolenBytes];
        const intptr_t relative = reinterpret_cast<intptr_t>(trampoline) - static_cast<intptr_t>(va + 5);
        jump[0] = kJmp;
        std::memcpy(jump + 1, &relative, 4);
        for (size_t i = 5; i < length; ++i) jump[i] = kNop;

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

namespace Patch {

    namespace {

        constexpr uint8_t kPushImm32 = 0x68;
        constexpr uint8_t kCallRel32 = 0xE8;

        struct Entry {
            Log::Name name;
            uintptr_t guardVa;
            uint8_t   guard[6];
            size_t    guardLength;
            uintptr_t writeVa;
            uint8_t   bytes[4];
            bool      data;
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
                Log::Warn("%s: %s at 0x%08lX could not be read.", gGroup, entry.name, static_cast<unsigned long>(entry.guardVa));
                return false;
            }
            if (std::fabs(current - entry.vanilla) <= 0.001f || std::fabs(current - wanted) <= 0.001f)
                return true;
            Log::Warn("%s: %s at 0x%08lX holds %g instead of the game's %g.", gGroup, entry.name,
                      static_cast<unsigned long>(entry.guardVa), current, entry.vanilla);
            return false;
        }

        bool CheckCode(const Entry& entry) {
            if (Memory::CheckBytes(entry.guardVa, entry.guard, entry.guardLength)) return true;
            char actual[16];
            DescribeBytes(entry.guardVa, entry.guardLength, actual, sizeof(actual));
            Log::Warn("%s: %s at 0x%08lX holds %s, not the game's code.", gGroup, entry.name,
                      static_cast<unsigned long>(entry.guardVa), actual);
            return false;
        }

        bool Check(const Entry& entry) {
            return entry.data ? CheckData(entry) : CheckCode(entry);
        }

        void PushBytes(uint8_t* out, float value) {
            out[0] = kPushImm32;
            std::memcpy(out + 1, &value, sizeof(value));
        }

        uint32_t CallOffset(uintptr_t call, uintptr_t target) {
            return static_cast<uint32_t>(target) - static_cast<uint32_t>(call + 5);
        }

    }

    void Begin(Log::Name group) {
        gPending.clear();
        gGroup = group;
    }

    void RedirectFloat(Log::Name name, const FloatOperand& site, const float* value) {
        Entry entry = {};
        entry.name = name;
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
        entry.guardVa = va;
        entry.writeVa = va;
        entry.data = true;
        entry.vanilla = vanilla;
        std::memcpy(entry.bytes, &value, sizeof(value));
        gPending.push_back(entry);
    }

    bool Commit() {
        for (const Entry& entry : gPending) {
            if (!Check(entry)) {
                Log::Warn("%s: skipped, because another mod has already changed this code.", gGroup);
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
                Log::Error("%s: %s at 0x%08lX could not be written; the group was rolled back.", gGroup, entry.name,
                           static_cast<unsigned long>(entry.writeVa));
                while (gJournal.size() > start) {
                    const Original& undo = gJournal.back();
                    Memory::WriteCode(undo.va, undo.bytes, sizeof(undo.bytes));
                    gJournal.pop_back();
                }
                ++gFailed;
                gPending.clear();
                return false;
            }
            gJournal.push_back(original);
        }

        Log::Info("%s: %u patch(es) applied.", gGroup, static_cast<unsigned>(gPending.size()));
        gPending.clear();
        return true;
    }

    bool RewritePushedFloat(const FloatPush& site, float from, float to) {
        uint8_t expected[5];
        PushBytes(expected, from);
        return Memory::CheckBytes(site.va, expected, sizeof(expected)) && Memory::WriteCode(site.va + 1, &to, sizeof(to));
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
            if (!Memory::WriteCode(original.va, original.bytes, sizeof(original.bytes)))
                Log::Warn("%s at 0x%08lX could not be restored.", original.name, static_cast<unsigned long>(original.va));
            gJournal.pop_back();
        }
    }

}

namespace Ini {

    namespace {

        constexpr int kLevels    = 10;
        constexpr int kMostParts = 4;

        const Config kGameValues{};

        struct Setting {
            const char*     section;
            Log::Name       name;
            float Config::* number;
            bool  Config::* toggle;
            float           min;
            float           max;
            bool            hasDefault;
        };

        const Setting kSettings[] = {
            { "Helicopter:Leading",                 "LeadBase",                &Config::LeadBase,                nullptr,            0.0f,   100.0f,  true },
            { "Helicopter:Leading",                 "LeadMax",                 &Config::LeadMax,                 nullptr,            5.0f,   150.0f,  true },
            { "Helicopter:CrushAttack",             "HoverHeight",             &Config::CrushHover,              nullptr,            0.0f,   10.0f,   false },
            { "Helicopter:CrushAttack",             "CrushHeight",             &Config::CrushHeight,             nullptr,            0.0f,   6.0f,    false },
            { "Helicopter:Turning",                 "TurnClamp",               &Config::TurnClamp,               nullptr,            0.4f,   5.0f,    true },
            { "Helicopter:Turning",                 "TurnResponseScale",       &Config::TurnResponseScale,       nullptr,            -30.0f, -0.5f,   true },
            { "Helicopter:Acceleration",            "MaxChopperAccel",         &Config::MaxChopperAccel,         nullptr,            40.0f,  250.0f,  true },
            { "Helicopter:Acceleration",            "MinChopperAccel",         &Config::MinChopperAccel,         nullptr,            0.0f,   160.0f,  true },
            { "Helicopter:SpeedCap",                "SpeedCap",                &Config::SpeedCap,                nullptr,            30.0f,  500.0f,  true },
            { "Helicopter:LineOfSight",             "LineOfSight",             &Config::LineOfSight,             nullptr,            0.0f,   5000.0f, false },
            { "Helicopter:FuelTime",                "FuelTime",                &Config::FuelTime,                nullptr,            0.0f,   3600.0f, false },
            { "Helicopter:HeliSheet",               "HeliSheet",               nullptr,                          &Config::HeliSheet, 0.0f,   1.0f,    false },
            { "Helicopter:IgnoreHeliSheetDistance", "IgnoreHeliSheetDistance", &Config::IgnoreHeliSheetDistance, nullptr,            0.0f,   2000.0f, false },
            { "Helicopter:FlySpeed",                "FlySpeed",                &Config::FlySpeed,                nullptr,            40.0f,  220.0f,  true },
            { "Helicopter:SpawnDistance",           "SpawnDistance",           &Config::SpawnDistance,           nullptr,            30.0f,  600.0f,  true },
        };

        constexpr int kCount = static_cast<int>(sizeof(kSettings) / sizeof(kSettings[0]));

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
            const Setting& setting = kSettings[index];
            if (setting.toggle) return gCfg.*setting.toggle ? 1.0f : 0.0f;
            return gCfg.*setting.number;
        }

        void Set(int index, float value) {
            const Setting& setting = kSettings[index];
            if (setting.toggle)
                gCfg.*setting.toggle = value != 0.0f;
            else
                gCfg.*setting.number = value;
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
                if (_stricmp(kSettings[i].section, section) == 0) return i;
            return -1;
        }

        int PartCount(int first) {
            int count = 1;
            while (first + count < kCount && std::strcmp(kSettings[first + count].section, kSettings[first].section) == 0) ++count;
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
                    Log::Warn("General.ini line %d: [%s] %s = %s must be true or false.", lineNumber, section, key, text);
                    ++*problems;
                    return false;
                }
                *value = _stricmp(text, "true") == 0 ? 1.0f : 0.0f;
                return true;
            }
            if (!ParseFloat(text, value)) {
                Log::Warn("General.ini line %d: [%s] %s = %s is not a number.", lineNumber, section, key, text);
                ++*problems;
                return false;
            }
            if (*value < setting.min || *value > setting.max) {
                const float clamped = *value < setting.min ? setting.min : setting.max;
                Log::Warn("General.ini line %d: [%s] %s %s = %g is outside %g to %g; using %g.", lineNumber, section, key, setting.name,
                          *value, setting.min, setting.max, clamped);
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
                    Log::Warn("General.ini line %d: \"%s\" is not a section header.", lineNumber, text);
                    ++*problems;
                    *first = -1;
                    return;
                }
                *close = '\0';
                std::snprintf(section, sectionSize, "%s", text + 1);
                Trim(section);
                *first = FindSection(section);
                if (*first < 0) {
                    Log::Warn("General.ini line %d: unknown section [%s] ignored.", lineNumber, section);
                    ++*unknownSections;
                }
                return;
            }
            if (*first < 0) return;

            char* equals = std::strchr(text, '=');
            if (!equals) {
                Log::Warn("General.ini line %d: \"%s\" has no '='.", lineNumber, text);
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
                Log::Warn("General.ini line %d: [%s] \"%s\" is not default, heat01-heat10 or race01-race10.", lineNumber, section, key);
                ++*problems;
                return;
            }
            if (level == 0 && !kSettings[*first].hasDefault) {
                Log::Warn("General.ini line %d: [%s] can't have a default value; set heat01-heat10 instead.", lineNumber, section);
                ++*problems;
                return;
            }

            const int parts = PartCount(*first);
            char* partText[kMostParts] = {};
            if (SplitParts(valueText, partText) != parts) {
                Log::Warn("General.ini line %d: [%s] %s needs %d value(s) separated by commas.", lineNumber, section, key, parts);
                ++*problems;
                return;
            }

            float values[kMostParts] = {};
            for (int part = 0; part < parts; ++part)
                if (!ParseValue(kSettings[*first + part], section, key, partText[part], lineNumber, &values[part], problems)) return;

            bool stored = true;
            for (int part = 0; part < parts; ++part)
                stored = Store(*first + part, level, race, values[part]) && stored;
            if (!stored) {
                Log::Warn("General.ini line %d: [%s] %s is set twice; the first value is kept.", lineNumber, section, key);
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

            Log::Info("General.ini: %d value(s) read%s.", read, problems ? ", with the problems listed above" : "");
            if (unknownSections)
                Log::Warn("General.ini: %d section(s) were not recognised. Sections are named [Helicopter:...], such as "
                          "[Helicopter:Turning]; a General.ini from an older version has to be replaced.",
                          unknownSections);
            return true;
        }

        void Sanitize() {
            if (gCfg.MinChopperAccel > gCfg.MaxChopperAccel) {
                Log::Warn("[Helicopter:Acceleration] minChopperAccel %g is above maxChopperAccel %g; lowered to match.", gCfg.MinChopperAccel,
                          gCfg.MaxChopperAccel);
                gCfg.MinChopperAccel = gCfg.MaxChopperAccel;
            }

            if (gCfg.LeadBase > gCfg.LeadMax)
                Log::Warn("[Helicopter:Leading] leadBase %g is above leadMax %g, so the helicopter always aims %g m ahead.", gCfg.LeadBase,
                          gCfg.LeadMax, gCfg.LeadMax);

            if (gCfg.CrushHover > 0.0f && gCfg.CrushHeight > gCfg.CrushHover) {
                Log::Warn("[Helicopter:CrushAttack] crushHeight %g is above hoverHeight %g; lowered to match.", gCfg.CrushHeight, gCfg.CrushHover);
                gCfg.CrushHeight = gCfg.CrushHover;
            }

            const float authority = std::fabs(gCfg.TurnResponseScale / kGameValues.TurnResponseScale) * (gCfg.TurnClamp / kGameValues.TurnClamp);
            if (authority >= 1.75f)
                Log::Warn("Turning authority is %.0f%% of the game's ([Helicopter:Turning] TurnClamp %g x TurnResponseScale %g). "
                          "Past about 175%% the helicopter rolls far over and can flip.",
                          authority * 100.0f, gCfg.TurnClamp, gCfg.TurnResponseScale);
        }

    }

    void Load(void* module) {
        for (int i = 0; i < kCount; ++i)
            gValues[i].vanilla = Get(i);

        char directory[MAX_PATH] = "";
        const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), directory, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            Log::Error("The scripts folder could not be located; every setting uses the game's own value.");
            return;
        }
        if (char* slash = std::strrchr(directory, '\\')) slash[1] = '\0';

        char path[MAX_PATH];
        std::snprintf(path, sizeof(path), "%sHelicopterOptions\\Configuration\\General.ini", directory);
        if (!ReadFile(path))
            Log::Warn("%s was not found; every setting uses the game's own value.", path);

        for (int i = 0; i < kCount; ++i)
            Set(i, Resolve(gValues[i], 0, false));
        Sanitize();
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

        Log::Info("Heat %d (%s): %d setting(s) changed.", level, racing ? "race" : "free roam", count);
        for (int n = 0; n < count; ++n)
            Log::Info("  [%s] %s %g -> %g", kSettings[changed[n]].section, kSettings[changed[n]].name, previous[n], Get(changed[n]));

        if (count) Sanitize();
        return count > 0;
    }

}

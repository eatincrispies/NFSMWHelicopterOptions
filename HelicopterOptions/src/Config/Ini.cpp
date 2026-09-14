#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Ini.h"
#include "Config.h"
#include "../Core/Log.h"

Config gCfg;

namespace Ini {

    namespace {

        constexpr int kLevels = 10;

        struct Setting {
            const char*     section;
            float Config::* number;
            bool  Config::* toggle;
            float           min;
            float           max;
        };

        const Setting kSettings[] = {
            { "AIActionHeliPursuit:LeadSpeedScale",       &Config::LeadSpeedScale,          nullptr,            0.0f,    2.0f },
            { "AIActionHeliPursuit:LeadBase",             &Config::LeadBase,                nullptr,            0.0f,  100.0f },
            { "AIActionHeliPursuit:LeadMax",              &Config::LeadMax,                 nullptr,            5.0f,  150.0f },
            { "AIActionHeliPursuit:ChaseHeightSkid",      &Config::ChaseHeightSkid,         nullptr,           -5.0f,   30.0f },
            { "AIActionHeliPursuit:ChaseHeightClose",     &Config::ChaseHeightClose,        nullptr,           -5.0f,   30.0f },
            { "AIActionHeliPursuit:ChaseHeightHigh",      &Config::ChaseHeightHigh,         nullptr,           -5.0f,   50.0f },
            { "AIActionHeliPursuit:SkidCooldown",         &Config::SkidCooldown,            nullptr,          -10.0f,    1.0f },
            { "AIActionHeliPursuit:SkidEntryMinDistance", &Config::SkidEntryMinDistance,    nullptr,            0.0f,   25.0f },
            { "AIActionHeliPursuit:SkidEntryMaxDistance", &Config::SkidEntryMaxDistance,    nullptr,            5.0f,  150.0f },
            { "AIActionHeliPursuit:SkidEntryAlignment",   &Config::SkidEntryAlignment,      nullptr,           -1.0f,    0.99f },
            { "AIActionHeliPursuit:SkidEntryMaxHeight",   &Config::SkidEntryMaxHeight,      nullptr,            2.0f,   60.0f },
            { "AIActionHeliPursuit:ReattackDelay",        &Config::ReattackDelay,           nullptr,            0.0f,  120.0f },
            { "SimpleChopper:TurnResponseScale",          &Config::TurnResponseScale,       nullptr,          -30.0f,   -0.5f },
            { "SimpleChopper:TurnClamp",                  &Config::TurnClamp,               nullptr,            0.4f,    5.0f },
            { "SimpleChopper:MaxChopperAccel",            &Config::MaxChopperAccel,         nullptr,           40.0f,  250.0f },
            { "SimpleChopper:MinChopperAccel",            &Config::MinChopperAccel,         nullptr,            0.0f,  160.0f },
            { "AIVehicleHelicopter:SpeedCap",             &Config::SpeedCap,                nullptr,           30.0f,  500.0f },
            { "AIVehicleHelicopter:MaxVerticalSpeed",     &Config::MaxVerticalSpeed,        nullptr,            0.0f,  200.0f },
            { "AIVehicleHelicopter:FuelTime",             &Config::FuelTime,                nullptr,            0.0f, 3600.0f },
            { "AIVehicleHelicopter:HeliSheet",            nullptr,                          &Config::HeliSheet, 0.0f,    1.0f },
            { "AIVehicleHelicopter:IgnoreHeliSheetDistance", &Config::IgnoreHeliSheetDistance, nullptr,         0.0f, 2000.0f },
            { "AIActionHeliExit:FlySpeed",                &Config::FlySpeed,                nullptr,           40.0f,  220.0f },
            { "AICopManager:SpawnDistance",               &Config::SpawnDistance,           nullptr,           30.0f,  600.0f },
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
            while (length && (text[length - 1] == ' ' || text[length - 1] == '\t'
                              || text[length - 1] == '\r' || text[length - 1] == '\n'))
                text[--length] = '\0';
        }

        int FindSetting(const char* section) {
            for (int i = 0; i < kCount; ++i)
                if (_stricmp(kSettings[i].section, section) == 0) return i;
            return -1;
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

        float Resolve(const Values& values, int level, bool racing) {
            if (level >= 1 && level <= kLevels) {
                const uint16_t bit = static_cast<uint16_t>(1u << (level - 1));
                if (racing && (values.hasRace & bit)) return values.race[level - 1];
                if (values.hasHeat & bit) return values.heat[level - 1];
            }
            return values.hasBase ? values.base : values.vanilla;
        }

        bool ReadFile(const char* path) {
            FILE* file = std::fopen(path, "rb");
            if (!file) return false;

            char line[1024];
            char section[128] = "";
            int  index = -1;
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
                if (lineNumber == 1 && static_cast<unsigned char>(text[0]) == 0xEF
                    && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
                    text += 3;
                Trim(text);
                if (!*text || *text == ';' || *text == '#') continue;

                if (*text == '[') {
                    char* close = std::strchr(text, ']');
                    if (!close) {
                        Log::Warn("General.ini line %d: \"%s\" is not a section header.", lineNumber, text);
                        ++problems;
                        index = -1;
                        continue;
                    }
                    *close = '\0';
                    std::snprintf(section, sizeof(section), "%s", text + 1);
                    Trim(section);
                    index = FindSetting(section);
                    if (index < 0) {
                        Log::Warn("General.ini line %d: unknown section [%s] ignored.", lineNumber, section);
                        ++unknownSections;
                    }
                    continue;
                }
                if (index < 0) continue;

                char* equals = std::strchr(text, '=');
                if (!equals) {
                    Log::Warn("General.ini line %d: \"%s\" has no '='.", lineNumber, text);
                    ++problems;
                    continue;
                }
                *equals = '\0';
                char* key = text;
                char* valueText = equals + 1;
                Trim(key);
                Trim(valueText);

                bool race = false;
                const int level = ParseLevel(key, &race);
                if (level < 0) {
                    Log::Warn("General.ini line %d: [%s] \"%s\" is not default, heat01-heat10 or race01-race10.",
                              lineNumber, section, key);
                    ++problems;
                    continue;
                }

                const Setting& setting = kSettings[index];
                float value = 0.0f;
                if (setting.toggle) {
                    if (std::strcmp(valueText, "0") != 0 && std::strcmp(valueText, "1") != 0) {
                        Log::Warn("General.ini line %d: [%s] %s = %s must be 0 or 1.", lineNumber, section, key, valueText);
                        ++problems;
                        continue;
                    }
                    value = valueText[0] == '1' ? 1.0f : 0.0f;
                } else {
                    if (!ParseFloat(valueText, &value)) {
                        Log::Warn("General.ini line %d: [%s] %s = %s is not a number.", lineNumber, section, key, valueText);
                        ++problems;
                        continue;
                    }
                    if (value < setting.min || value > setting.max) {
                        const float clamped = value < setting.min ? setting.min : setting.max;
                        Log::Warn("General.ini line %d: [%s] %s = %g is outside %g to %g; using %g.",
                                  lineNumber, section, key, value, setting.min, setting.max, clamped);
                        value = clamped;
                        ++problems;
                    }
                }

                Values& values = gValues[index];
                bool duplicate = false;
                if (level == 0) {
                    duplicate = values.hasBase;
                    if (!duplicate) {
                        values.base = value;
                        values.hasBase = true;
                    }
                } else {
                    const uint16_t bit = static_cast<uint16_t>(1u << (level - 1));
                    uint16_t& mask = race ? values.hasRace : values.hasHeat;
                    duplicate = (mask & bit) != 0;
                    if (!duplicate) {
                        (race ? values.race : values.heat)[level - 1] = value;
                        mask = static_cast<uint16_t>(mask | bit);
                    }
                }
                if (duplicate) {
                    Log::Warn("General.ini line %d: [%s] %s is set twice; the first value is kept.", lineNumber, section, key);
                    ++problems;
                    continue;
                }
                ++read;
            }
            std::fclose(file);

            Log::Info("General.ini: %d value(s) read%s.", read, problems ? ", with the problems listed above" : "");
            if (unknownSections)
                Log::Warn("General.ini: %d section(s) were not recognised. Sections are named after the game's classes, "
                          "such as [SimpleChopper:TurnClamp]; a General.ini from an older version has to be replaced.",
                          unknownSections);
            return true;
        }

        void Sanitize() {
            if (gCfg.MinChopperAccel > gCfg.MaxChopperAccel) {
                Log::Warn("[SimpleChopper:MinChopperAccel] %g is above [SimpleChopper:MaxChopperAccel] %g; lowered to match.",
                          gCfg.MinChopperAccel, gCfg.MaxChopperAccel);
                gCfg.MinChopperAccel = gCfg.MaxChopperAccel;
            }
            if (gCfg.SkidEntryMinDistance >= gCfg.SkidEntryMaxDistance)
                Log::Info("[AIActionHeliPursuit:SkidEntryMinDistance] is not below SkidEntryMaxDistance, "
                          "so the helicopter cannot start an attack with these values.");

            const float authority = std::fabs(gCfg.TurnResponseScale / Addr::SimpleChopper::Vanilla::TurnResponseScale)
                                  * (gCfg.TurnClamp / Addr::SimpleChopper::Vanilla::TurnClamp);
            if (authority >= 1.75f)
                Log::Warn("Turning authority is %.0f%% of the game's (TurnResponseScale %g x TurnClamp %g). "
                          "Past about 175%% the helicopter rolls far over and can flip.",
                          authority * 100.0f, gCfg.TurnResponseScale, gCfg.TurnClamp);
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
        std::snprintf(path, sizeof(path), "%sHelicopterOptions\\Configuration\\Debug.ini", directory);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            Log::Info("Debug.ini is no longer used and can be deleted.");

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
            Log::Info("  [%s] %g -> %g", kSettings[changed[n]].section, previous[n], Get(changed[n]));

        if (count) Sanitize();
        return count > 0;
    }

}

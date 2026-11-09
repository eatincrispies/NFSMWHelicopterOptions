#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "dllmain.hpp"
#include "src/Helicopter/AIActionHeliExit.hpp"
#include "src/Helicopter/AIActionHeliPursuit.hpp"
#include "src/Helicopter/AIVehicleHelicopter.hpp"
#include "src/Helicopter/HeliSheet.hpp"
#include "src/Helicopter/SimpleChopper.hpp"

namespace {

    #define IPLAYER_LISTS                    0x0092D848
    #define SET_HEAT                         0x00409060
    #define SET_HEAT_ARGUMENT                0x08
    #define PERP_IPERPETRATOR                0x758

    #define INIT_DELAY_MS                    1000

    #define MOST_DETOURS                     16
    #define MOST_STOLEN_BYTES                8
    #define TRAMPOLINE_SIZE                  64
    #define MOST_PENDING                     16
    #define MOST_JOURNAL                     96
    #define MOST_PATCH_BYTES                 8
    #define WORD_SIZE                        4
    #define BRANCH_SIZE                      5
    #define FLOAT_OPERAND_SIZE               6
    #define DATA_TOLERANCE_PER_THOUSAND      1

    #define OPCODE_PUSHFD                    0x9C
    #define OPCODE_PUSHAD                    0x60
    #define OPCODE_CLD                       0xFC
    #define OPCODE_PUSH_ESP                  0x54
    #define OPCODE_POPAD                     0x61
    #define OPCODE_POPFD                     0x9D
    #define OPCODE_PUSH                      0x68
    #define OPCODE_CALL                      0xE8
    #define OPCODE_JUMP                      0xE9
    #define OPCODE_NOP                       0x90

    #define MOST_SETTING_PARTS               4
    #define MOST_VALUE_TEXT                  128

    const BYTE kSetHeatCode[] = { 0x6A, 0xFF, 0x68, 0xBC, 0x79, 0x86, 0x00 };
    const BYTE kAddEsp4Code[] = { 0x83, 0xC4, 0x04 };

    const char* const kIniFiles[] = { "General", "Advanced" };

    struct IPlayerList {
        void*     mVTable;
        IPlayer** mBegin;
        size_t    mCapacity;
        size_t    mSize;
        IPlayer*  mStorage[8];
    };

    struct Setting {
        const char*  section;
        float*       number;
        bool*        toggle;
        float        min;
        float        max;
        bool         hasDefault;
        const float* heatDefaults;
        const bool*  heatToggles;
    };

    struct TextSetting {
        const char* section;
        char*       text;
        bool        hasDefault;
    };

    struct TextValues {
        char vanilla[MOST_NAME_TEXT];
        char base[MOST_NAME_TEXT];
        char heat[HEAT_LEVELS][MOST_NAME_TEXT];
        char race[HEAT_LEVELS][MOST_NAME_TEXT];
        bool hasBase;
        WORD hasHeat;
        WORD hasRace;
    };

    struct Values {
        float vanilla;
        float base;
        float heat[HEAT_LEVELS];
        float race[HEAT_LEVELS];
        bool  hasBase;
        WORD  hasHeat;
        WORD  hasRace;
    };

    struct InstalledDetour {
        DWORD  address;
        BYTE   original[MOST_STOLEN_BYTES];
        size_t length;
    };

    struct PendingPatch {
        DWORD  guardAddress;
        BYTE   guard[MOST_PATCH_BYTES];
        size_t guardLength;
        DWORD  writeAddress;
        BYTE   bytes[MOST_PATCH_BYTES];
        size_t length;
        bool   isData;
        float  vanilla;
    };

    struct OriginalBytes {
        DWORD  address;
        BYTE   bytes[MOST_PATCH_BYTES];
        size_t length;
    };

    C_ASSERT(sizeof(IPlayerList) == 0x30);
    C_ASSERT(offsetof(AIPerpVehicle, mIOwner) == 0x34);
    C_ASSERT(offsetof(AIPerpVehicle, mIPerpetrator) == PERP_IPERPETRATOR);
    C_ASSERT(offsetof(AIPerpVehicle, mHeat) == 0x774);
    C_ASSERT(offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate) == 0x786);

    const Setting kSettings[] = {
        { "Helicopter:Leading",                 &AIActionHeliPursuit::sSettings.LeadBase,                NULL,                                          0.0f,   100.0f,  true },
        { "Helicopter:Leading",                 &AIActionHeliPursuit::sSettings.LeadMax,                 NULL,                                          5.0f,   150.0f,  true },
        { "Helicopter:HeliStrategy2",           &AIActionHeliPursuit::sSettings.AttackDuration,          NULL,                                          2.0f,   30.0f,   true },
        { "Helicopter:ClosingSpeed",            &AIActionHeliPursuit::sSettings.ClosingSpeedNear,        NULL,                                          0.0f,   200.0f,  true },
        { "Helicopter:ClosingSpeed",            &AIActionHeliPursuit::sSettings.ClosingSpeedMid,         NULL,                                          0.0f,   200.0f,  true },
        { "Helicopter:ClosingSpeed",            &AIActionHeliPursuit::sSettings.ClosingSpeedFar,         NULL,                                          0.0f,   200.0f,  true },
        { "Helicopter:SearchSpeed",             &AIActionHeliPursuit::sSettings.SearchSpeedMin,          NULL,                                          5.0f,   120.0f,  true },
        { "Helicopter:SearchSpeed",             &AIActionHeliPursuit::sSettings.SearchSpeedMax,          NULL,                                          5.0f,   120.0f,  true },
        { "Helicopter:SearchRadius",            &AIActionHeliPursuit::sSettings.SearchRadiusFirst,       NULL,                                          20.0f,  1000.0f, true },
        { "Helicopter:SearchRadius",            &AIActionHeliPursuit::sSettings.SearchRadiusLargest,     NULL,                                          20.0f,  2000.0f, true },
        { "Helicopter:Turning",                 &SimpleChopper::sSettings.TurnClamp,                     NULL,                                          0.4f,   5.0f,    true },
        { "Helicopter:Turning",                 &SimpleChopper::sSettings.TurnResponseScale,             NULL,                                          -30.0f, -0.5f,   true },
        { "Helicopter:Acceleration",            &SimpleChopper::sSettings.MaxChopperAccel,               NULL,                                          40.0f,  250.0f,  true },
        { "Helicopter:Acceleration",            &SimpleChopper::sSettings.MinChopperAccel,               NULL,                                          0.0f,   160.0f,  true },
        { "Helicopter:SpeedCap",                &SimpleChopper::sSettings.SpeedCap,                      NULL,                                          30.0f,  500.0f,  true },
        { "Helicopter:Brake",                   &AIVehicleHelicopter::sSettings.StoppingRatio,           NULL,                                          0.3f,   10.0f,   true },
        { "Helicopter:Brake",                   &AIVehicleHelicopter::sSettings.BrakeSpeedScale,         NULL,                                          0.0f,   1.0f,    true },
        { "Helicopter:LineOfSight",             &AIVehicleHelicopter::sSettings.LineOfSight,             NULL,                                          0.0f,   5000.0f, false },
        { "Helicopter:FuelTime",                &AIVehicleHelicopter::sSettings.FuelTime,                NULL,                                          0.0f,   3600.0f, false, AIVehicleHelicopter::sSettings.HeatFuelTime },
        { "Helicopter:SpawnDistance",           &AIVehicleHelicopter::sSettings.SpawnDistance,           NULL,                                          30.0f,  600.0f,  true },
        { "Helicopter:AllAroundSight",          &AIVehicleHelicopter::sSettings.AllAroundSight,          NULL,                                          0.0f,   1000.0f, true },
        { "Helicopter:FrontSightOnly",          NULL,                                                    &AIVehicleHelicopter::sSettings.FrontSightOnly, 0.0f,   1.0f,    true },
        { "Helicopter:Respawning",              &AIVehicleHelicopter::sSettings.RespawnDelay,            NULL,                                          0.0f,   600.0f,  true },
        { "Helicopter:HidingSpots",             NULL,                                                    &AIVehicleHelicopter::sSettings.HidingSpots,    0.0f,   1.0f,    true },
        { "Helicopter:DespawnDistance",         &AIVehicleHelicopter::sSettings.DespawnDistance,         NULL,                                          100.0f, 5000.0f, true },
        { "Helicopter:Helicopters",             &AIVehicleHelicopter::sSettings.Helicopters,             NULL,                                          1.0f,   4.0f,    true,  AIVehicleHelicopter::sSettings.HeatHelicopters },
        { "Helicopter:Falcon2",                 NULL,                                                    &AIVehicleHelicopter::sSettings.Falcon2,       0.0f,   1.0f,    true,  NULL, AIVehicleHelicopter::sSettings.HeatFalcon2 },
        { "Helicopter:Recycle",                 &AIVehicleHelicopter::sSettings.RecycleTime,             NULL,                                          0.0f,   300.0f,  true },
        { "Helicopter:Recycle",                 &AIVehicleHelicopter::sSettings.RecycleDistance,         NULL,                                          0.0f,   2000.0f, true },
        { "Helicopter:Disp911Report",           NULL,                                                    &AIVehicleHelicopter::sSettings.Disp911Report, 0.0f,   1.0f,    true,  NULL, AIVehicleHelicopter::sSettings.HeatDisp911Report },
        { "Helicopter:Bounty",                  &AIVehicleHelicopter::sSettings.Bounty,                  NULL,                                          0.0f,   100000000.0f, true },
        { "Helicopter:HeliSheet",               NULL,                                                    &HeliSheet::sSettings.HeliSheet,               0.0f,   1.0f,    false },
        { "Helicopter:IgnoreHeliSheetDistance", &HeliSheet::sSettings.IgnoreHeliSheetDistance,           NULL,                                          0.0f,   2000.0f, true },
        { "Helicopter:FlySpeed",                &AIActionHeliExit::sSettings.FlySpeed,                   NULL,                                          40.0f,  220.0f,  true },
    };

    #define SETTING_COUNT                    ((int)(sizeof(kSettings) / sizeof(kSettings[0])))

    const TextSetting kTextSettings[] = {
        { "Helicopter:Pvehicle", AIVehicleHelicopter::sSettings.Pvehicle, true },
    };

    #define TEXT_SETTING_COUNT               ((int)(sizeof(kTextSettings) / sizeof(kTextSettings[0])))

    HMODULE         gModule           = NULL;
    AIPerpVehicle*  gPlayerPerp       = NULL;
    DWORD           gPlayerPerpVTable = 0;

    InstalledDetour gDetours[MOST_DETOURS];
    int             gDetourCount      = 0;
    PendingPatch    gPending[MOST_PENDING];
    int             gPendingCount     = 0;
    bool            gPendingOverflow  = false;
    OriginalBytes   gJournal[MOST_JOURNAL];
    int             gJournalCount     = 0;
    Values          gValues[SETTING_COUNT];
    TextValues      gTextValues[TEXT_SETTING_COUNT];
    int             gLevel            = -1;
    bool            gRacing           = false;

    bool           IsHeatLevel(float heat);
    AIPerpVehicle* FromIPerpetrator(DWORD iperp);
    DWORD          IPerpetratorVTable(const AIPerpVehicle* perp);
    void           ApplyHeat(int level, bool racing);
    BYTE*          WriteBranch(BYTE* code, BYTE opcode, DWORD target);
    BYTE*          BuildTrampoline(DWORD address, const BYTE* stolen, size_t length, Detour::Entry entry);
    PendingPatch*  NewPatch();
    bool           CheckPatch(const PendingPatch& patch);
    DWORD          BranchOffset(DWORD site, DWORD target);
    void           PushBytes(BYTE* code, float value);
    bool           Rewrite(DWORD address, const BYTE* expected, size_t length, size_t at, const void* bytes, size_t count);
    float          GetSetting(int index);
    void           SetSetting(int index, float value);
    int            PartCount(int first);
    void           Trim(char* text);
    int            SplitParts(char* text, char* parts[MOST_SETTING_PARTS]);
    bool           ParseFloat(const char* text, float* out);
    bool           ParseValue(const Setting& setting, const char* text, float* value);
    void           Store(int index, int level, bool race, float value);
    float          Resolve(int index, int level, bool racing);
    void           ReadKey(const char* path, int first, int parts, const char* key, int level, bool race);
    void           ReadFile(const char* path);
    bool           IsName(const char* text);
    void           StoreText(int index, int level, bool race, const char* text);
    const char*    ResolveText(int index, int level, bool racing);
    bool           ApplyText(int index, int level, bool racing);
    void           ReadTextKey(const char* path, int index, const char* key, int level, bool race);
    void           ReadTextFile(const char* path);
    bool           FileExists(const char* path);

}

IPlayer* IPlayer::First(ePlayerList idx) {
    const IPlayerList& list = ((const IPlayerList*)IPLAYER_LISTS)[idx];
    return list.mSize != 0 ? list.mBegin[0] : NULL;
}

IRigidBody* GetLocalPlayerRigidBody() {
    IPlayer* player = IPlayer::First(PLAYER_LOCAL);
    ISimable* simable = player != NULL ? player->GetSimable() : NULL;
    return simable != NULL ? simable->GetRigidBody() : NULL;
}

float UMath::Distancexz(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

bool AIPerpVehicle::HookSetHeat() {
    return Detour::Install(SET_HEAT, kSetHeatCode, sizeof(kSetHeatCode), &SetHeatEntry);
}

void __cdecl AIPerpVehicle::SetHeatEntry(Detour::Registers* registers) {
    AIPerpVehicle* perp = FromIPerpetrator(registers->ecx);
    if (perp == NULL || !perp->IsLocalPlayer()) return;

    float heat = 0;
    bool read = Memory::Read(registers->esp + SET_HEAT_ARGUMENT, &heat, sizeof(heat));
    gPlayerPerpVTable = IPerpetratorVTable(perp);
    gPlayerPerp = perp;
    if (!read || !IsHeatLevel(heat)) return;
    ApplyHeat((int)heat, perp->mWasInRaceEventLastHeatUpdate);
}

void AIPerpVehicle::UpdateHeat() {
    if (gPlayerPerp == NULL || IPerpetratorVTable(gPlayerPerp) != gPlayerPerpVTable) return;

    float heat = 0;
    BYTE racing = 0;
    DWORD perp = (DWORD)gPlayerPerp;
    if (!Memory::Read(perp + offsetof(AIPerpVehicle, mHeat), &heat, sizeof(heat)) || !IsHeatLevel(heat)
        || !Memory::Read(perp + offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate), &racing, sizeof(racing)))
        return;
    ApplyHeat((int)heat, racing != 0);
}

bool AIPerpVehicle::IsLocalPlayer() const {
    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    return mIOwner != NULL && playerRigidBody != NULL && mIOwner->GetRigidBody() == playerRigidBody;
}

namespace {

    DWORD WINAPI Initialize(void*) {
        Sleep(INIT_DELAY_MS);
        Ini::Load(gModule);

        AIActionHeliPursuit::InstallPatches();
        AIVehicleHelicopter::InstallPatches();
        HeliSheetCoordinate::InstallPatches();
        CollisionGeometry::Collections::InstallPatches();
        Minimap::InstallPatches();
        CameraMover::InstallPatches();
        SoundHeli::InstallPatches();
        DrawHeli::InstallPatches();
        SoundAI::InstallPatches();
        EAXAirSupport::InstallPatches();
        EAXCop::InstallPatches();
        CSTATEMGR_Helicopter::InstallPatches();
        SimpleChopper::InstallPatches();
        AIActionHeliExit::InstallPatches();

        AIPerpVehicle::HookSetHeat();
        AIActionHeliPursuit::HookConstructor();
        AIVehicleHelicopter::HookOnDriving();
        AIVehicleHelicopter::HookCanSeeTarget();
        AIVehicleHelicopter::HookRespawnTimer();
        AICopManager::HookApplyBreakerZones();
        AICopManager::HookRecycle();
        AICopManager::HookGetAvailableCopVehicleByName();
        AIPursuit::HookIncNumCopsDestroyed();
        EAXAirSupport::HookSetHandle();
        EAXAirSupport::HookAddNewHeli();
        EAXAirSupport::HookScheduleSpeech();
        SoundAI::HookSyncCarsToActors();
        SoundAI::HookChopperLOS();
        return 0;
    }

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            gModule = module;
            DisableThreadLibraryCalls(module);
            HANDLE thread = CreateThread(NULL, 0, Initialize, NULL, 0, NULL);
            if (thread != NULL) CloseHandle(thread);
            break;
        }
        case DLL_PROCESS_DETACH:
            if (reserved == NULL) {
                Detour::RemoveAll();
                SimpleChopper::RestoreWrecks();
                SimpleChopper::RestoreSpeedCap();
                Patch::RestoreAll();
            }
            break;
    }
    return TRUE;
}

float Seconds(int milliseconds) {
    return (float)milliseconds / 1000;
}

float Metres(int millimetres) {
    return (float)millimetres / 1000;
}

float Fraction(int percent) {
    return (float)percent / 100;
}

float Smaller(float a, float b) {
    return b < a ? b : a;
}

float Larger(float a, float b) {
    return a < b ? b : a;
}

float Clamp(float value, float low, float high) {
    return value < low ? low : (high < value ? high : value);
}

int ClampInt(int value, int low, int high) {
    return value < low ? low : (high < value ? high : value);
}

namespace Memory {

    bool IsFinite(float value) {
        return value == value && value <= FLT_MAX && value >= -FLT_MAX;
    }

    bool Matches(DWORD address, const BYTE* expected, size_t length) {
        __try {
            return memcmp((const void*)address, expected, length) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool Read(DWORD address, void* out, size_t length) {
        __try {
            memcpy(out, (const void*)address, length);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool WriteData(DWORD address, const void* bytes, size_t length) {
        __try {
            memcpy((void*)address, bytes, length);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool WriteCode(DWORD address, const void* bytes, size_t length) {
        DWORD oldProtect = 0;
        if (!VirtualProtect((void*)address, length, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        memcpy((void*)address, bytes, length);
        FlushInstructionCache(GetCurrentProcess(), (void*)address, length);
        VirtualProtect((void*)address, length, oldProtect, &oldProtect);
        return true;
    }

}

namespace Detour {

    bool Install(DWORD address, const BYTE* prologue, size_t length, Entry entry) {
        for (int i = 0; i < gDetourCount; ++i)
            if (gDetours[i].address == address) return true;
        if (length < BRANCH_SIZE || length > MOST_STOLEN_BYTES || gDetourCount == MOST_DETOURS || !Memory::Matches(address, prologue, length))
            return false;

        BYTE* trampoline = BuildTrampoline(address, prologue, length, entry);
        if (trampoline == NULL) return false;

        BYTE jump[MOST_STOLEN_BYTES];
        memset(jump, OPCODE_NOP, length);
        jump[0] = OPCODE_JUMP;
        DWORD relative = BranchOffset(address, (DWORD)trampoline);
        memcpy(jump + 1, &relative, sizeof(relative));
        if (!Memory::WriteCode(address, jump, length)) return false;

        InstalledDetour& installed = gDetours[gDetourCount++];
        installed.address = address;
        installed.length = length;
        memcpy(installed.original, prologue, length);
        return true;
    }

    void RemoveAll() {
        while (gDetourCount > 0) {
            const InstalledDetour& installed = gDetours[--gDetourCount];
            Memory::WriteCode(installed.address, installed.original, installed.length);
        }
    }

}

namespace Patch {

    void Begin() {
        gPendingCount = 0;
        gPendingOverflow = false;
    }

    void RedirectFloat(const FloatOperand& site, const float* value) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        patch->guard[0] = site.opcode[0];
        patch->guard[1] = site.opcode[1];
        memcpy(patch->guard + 2, &site.constant, sizeof(site.constant));
        patch->guardLength = FLOAT_OPERAND_SIZE;
        patch->writeAddress = site.address + 2;
        DWORD target = (DWORD)value;
        memcpy(patch->bytes, &target, sizeof(target));
    }

    void PushFloat(const FloatPush& site, float value) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        PushBytes(patch->guard, site.value);
        patch->guardLength = BRANCH_SIZE;
        patch->writeAddress = site.address + 1;
        memcpy(patch->bytes, &value, sizeof(value));
    }

    void RedirectCall(const CallSite& site, const void* replacement) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        patch->guard[0] = OPCODE_CALL;
        DWORD original = BranchOffset(site.address, site.target);
        memcpy(patch->guard + 1, &original, sizeof(original));
        patch->guardLength = BRANCH_SIZE;
        patch->writeAddress = site.address + 1;
        DWORD offset = BranchOffset(site.address, (DWORD)replacement);
        memcpy(patch->bytes, &offset, sizeof(offset));
    }

    void RedirectIndirectCall(const IndirectCall& site, const void* replacement) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        memcpy(patch->guard, site.code, sizeof(site.code));
        patch->guardLength = sizeof(site.code);
        patch->writeAddress = site.address;
        patch->length = sizeof(site.code);
        memset(patch->bytes, OPCODE_NOP, patch->length);
        patch->bytes[0] = OPCODE_CALL;
        DWORD offset = BranchOffset(site.address, (DWORD)replacement);
        memcpy(patch->bytes + 1, &offset, sizeof(offset));
    }

    void RedirectVirtual(DWORD slot, DWORD function, const void* replacement) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = slot;
        memcpy(patch->guard, &function, sizeof(function));
        patch->guardLength = sizeof(function);
        patch->writeAddress = slot;
        DWORD hook = (DWORD)replacement;
        memcpy(patch->bytes, &hook, sizeof(hook));
    }

    void DataFloat(DWORD address, float vanilla, float value) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->isData = true;
        patch->guardAddress = address;
        patch->writeAddress = address;
        patch->vanilla = vanilla;
        memcpy(patch->bytes, &value, sizeof(value));
    }

    void StoreFloat(const FloatStore& site, float value) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        memcpy(patch->guard, site.opcode, sizeof(site.opcode));
        memcpy(patch->guard + sizeof(site.opcode), &site.value, sizeof(site.value));
        patch->guardLength = sizeof(site.opcode) + sizeof(site.value);
        patch->writeAddress = site.address + sizeof(site.opcode);
        memcpy(patch->bytes, &value, sizeof(value));
    }

    void StoreByte(const ByteStore& site, BYTE value) {
        PendingPatch* patch = NewPatch();
        if (patch == NULL) return;
        patch->guardAddress = site.address;
        memcpy(patch->guard, site.code, sizeof(site.code));
        patch->guard[sizeof(site.code)] = site.value;
        patch->guardLength = sizeof(site.code) + 1;
        patch->writeAddress = site.address + patch->guardLength - patch->length;
        memcpy(patch->bytes, patch->guard + patch->guardLength - patch->length, patch->length);
        patch->bytes[patch->length - 1] = value;
    }

    bool Commit() {
        bool ready = !gPendingOverflow;
        for (int i = 0; ready && i < gPendingCount; ++i) ready = CheckPatch(gPending[i]);
        if (!ready || gJournalCount + gPendingCount > MOST_JOURNAL) {
            gPendingCount = 0;
            return false;
        }

        int start = gJournalCount;
        for (int i = 0; i < gPendingCount; ++i) {
            const PendingPatch& patch = gPending[i];
            OriginalBytes& original = gJournal[gJournalCount];
            original.address = patch.writeAddress;
            original.length = patch.length;
            if (!Memory::Read(patch.writeAddress, original.bytes, patch.length) || !Memory::WriteCode(patch.writeAddress, patch.bytes, patch.length)) {
                while (gJournalCount > start) {
                    const OriginalBytes& undo = gJournal[--gJournalCount];
                    Memory::WriteCode(undo.address, undo.bytes, undo.length);
                }
                gPendingCount = 0;
                return false;
            }
            ++gJournalCount;
        }
        gPendingCount = 0;
        return true;
    }

    bool RewritePushedFloat(const FloatPush& site, float from, float to) {
        BYTE expected[BRANCH_SIZE];
        PushBytes(expected, from);
        return Rewrite(site.address, expected, sizeof(expected), 1, &to, sizeof(to));
    }

    bool RewriteStoredFloat(const FloatStore& site, float from, float to) {
        BYTE expected[sizeof(site.opcode) + sizeof(from)];
        memcpy(expected, site.opcode, sizeof(site.opcode));
        memcpy(expected + sizeof(site.opcode), &from, sizeof(from));
        return Rewrite(site.address, expected, sizeof(expected), sizeof(site.opcode), &to, sizeof(to));
    }

    bool RewriteStoredByte(const ByteStore& site, BYTE from, BYTE to) {
        BYTE expected[sizeof(site.code) + 1];
        memcpy(expected, site.code, sizeof(site.code));
        expected[sizeof(site.code)] = from;
        return Rewrite(site.address, expected, sizeof(expected), sizeof(site.code), &to, sizeof(to));
    }

    void RestoreAll() {
        while (gJournalCount > 0) {
            const OriginalBytes& original = gJournal[--gJournalCount];
            Memory::WriteCode(original.address, original.bytes, original.length);
        }
    }

}

namespace Ini {

    void Load(HMODULE module) {
        for (int i = 0; i < SETTING_COUNT; ++i) {
            memset(&gValues[i], 0, sizeof(gValues[i]));
            gValues[i].vanilla = GetSetting(i);
        }
        for (int i = 0; i < TEXT_SETTING_COUNT; ++i) {
            memset(&gTextValues[i], 0, sizeof(gTextValues[i]));
            strcpy_s(gTextValues[i].vanilla, sizeof(gTextValues[i].vanilla), kTextSettings[i].text);
        }

        char directory[MAX_PATH] = "";
        DWORD length = GetModuleFileNameA(module, directory, MAX_PATH);
        char* slash = (length != 0 && length < MAX_PATH) ? strrchr(directory, '\\') : NULL;
        if (slash == NULL) return;
        slash[1] = '\0';

        for (int file = 0; file < (int)(sizeof(kIniFiles) / sizeof(kIniFiles[0])); ++file) {
            char inFolder[MAX_PATH];
            char beside[MAX_PATH];
            sprintf_s(inFolder, sizeof(inFolder), "%sHelicopterOptions\\Configuration\\%s\\%s.ini", directory, kIniFiles[file], kIniFiles[file]);
            sprintf_s(beside, sizeof(beside), "%sHelicopterOptions\\Configuration\\%s.ini", directory, kIniFiles[file]);
            const char* path = FileExists(inFolder) ? inFolder : (FileExists(beside) ? beside : NULL);
            if (path == NULL) continue;
            ReadFile(path);
            ReadTextFile(path);
        }

        for (int i = 0; i < SETTING_COUNT; ++i) SetSetting(i, Resolve(i, 0, false));
        for (int i = 0; i < TEXT_SETTING_COUNT; ++i) ApplyText(i, 0, false);
    }

    bool ApplyHeat(int level, bool racing) {
        level = ClampInt(level, LOWEST_HEAT, HEAT_LEVELS);
        if (level == gLevel && racing == gRacing) return false;
        gLevel = level;
        gRacing = racing;

        int changed = 0;
        for (int i = 0; i < SETTING_COUNT; ++i) {
            float value = Resolve(i, level, racing);
            if (GetSetting(i) == value) continue;
            SetSetting(i, value);
            ++changed;
        }
        for (int i = 0; i < TEXT_SETTING_COUNT; ++i)
            if (ApplyText(i, level, racing)) ++changed;
        return changed > 0;
    }

}

namespace {

    bool IsHeatLevel(float heat) {
        return heat >= LOWEST_HEAT && heat < HEAT_LEVELS + 1;
    }

    AIPerpVehicle* FromIPerpetrator(DWORD iperp) {
        return iperp != 0 ? (AIPerpVehicle*)(iperp - PERP_IPERPETRATOR) : NULL;
    }

    DWORD IPerpetratorVTable(const AIPerpVehicle* perp) {
        DWORD vtable = 0;
        Memory::Read((DWORD)perp + PERP_IPERPETRATOR, &vtable, sizeof(vtable));
        return vtable;
    }

    void ApplyHeat(int level, bool racing) {
        if (!Ini::ApplyHeat(level, racing)) return;
        AIActionHeliPursuit::Refresh();
        AIVehicleHelicopter::Refresh();
        SimpleChopper::Refresh();
        AIActionHeliExit::Refresh();
    }

    BYTE* WriteBranch(BYTE* code, BYTE opcode, DWORD target) {
        *code++ = opcode;
        DWORD relative = target - ((DWORD)code + WORD_SIZE);
        memcpy(code, &relative, sizeof(relative));
        return code + WORD_SIZE;
    }

    BYTE* BuildTrampoline(DWORD address, const BYTE* stolen, size_t length, Detour::Entry entry) {
        BYTE* trampoline = (BYTE*)VirtualAlloc(NULL, TRAMPOLINE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (trampoline == NULL) return NULL;

        BYTE* code = trampoline;
        *code++ = OPCODE_PUSHFD;
        *code++ = OPCODE_PUSHAD;
        *code++ = OPCODE_CLD;
        *code++ = OPCODE_PUSH_ESP;
        code = WriteBranch(code, OPCODE_CALL, (DWORD)entry);
        memcpy(code, kAddEsp4Code, sizeof(kAddEsp4Code));
        code += sizeof(kAddEsp4Code);
        *code++ = OPCODE_POPAD;
        *code++ = OPCODE_POPFD;
        memcpy(code, stolen, length);
        code += length;
        WriteBranch(code, OPCODE_JUMP, address + length);

        FlushInstructionCache(GetCurrentProcess(), trampoline, TRAMPOLINE_SIZE);
        return trampoline;
    }

    PendingPatch* NewPatch() {
        if (gPendingCount == MOST_PENDING) {
            gPendingOverflow = true;
            return NULL;
        }
        PendingPatch* patch = &gPending[gPendingCount++];
        memset(patch, 0, sizeof(*patch));
        patch->length = WORD_SIZE;
        return patch;
    }

    bool CheckPatch(const PendingPatch& patch) {
        if (!patch.isData) return Memory::Matches(patch.guardAddress, patch.guard, patch.guardLength);
        float current = 0;
        float wanted = 0;
        memcpy(&wanted, patch.bytes, sizeof(wanted));
        if (!Memory::Read(patch.guardAddress, &current, sizeof(current))) return false;
        float tolerance = (float)DATA_TOLERANCE_PER_THOUSAND / 1000;
        return fabsf(current - patch.vanilla) <= tolerance || fabsf(current - wanted) <= tolerance;
    }

    DWORD BranchOffset(DWORD site, DWORD target) {
        return target - (site + BRANCH_SIZE);
    }

    void PushBytes(BYTE* code, float value) {
        code[0] = OPCODE_PUSH;
        memcpy(code + 1, &value, sizeof(value));
    }

    bool Rewrite(DWORD address, const BYTE* expected, size_t length, size_t at, const void* bytes, size_t count) {
        return Memory::Matches(address, expected, length) && Memory::WriteCode(address + at, bytes, count);
    }

    float GetSetting(int index) {
        const Setting& setting = kSettings[index];
        if (setting.toggle != NULL) return (float)(*setting.toggle ? 1 : 0);
        return *setting.number;
    }

    void SetSetting(int index, float value) {
        const Setting& setting = kSettings[index];
        if (setting.toggle != NULL)
            *setting.toggle = value != 0;
        else
            *setting.number = value;
    }

    int PartCount(int first) {
        int count = 1;
        while (first + count < SETTING_COUNT && strcmp(kSettings[first + count].section, kSettings[first].section) == 0) ++count;
        return count;
    }

    void Trim(char* text) {
        char* start = text;
        while (*start == ' ' || *start == '\t') ++start;
        if (start != text) memmove(text, start, strlen(start) + 1);
        size_t length = strlen(text);
        while (length != 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n'))
            text[--length] = '\0';
    }

    int SplitParts(char* text, char* parts[MOST_SETTING_PARTS]) {
        int count = 0;
        char* next = text;
        while (next != NULL && count < MOST_SETTING_PARTS) {
            char* comma = strchr(next, ',');
            if (comma != NULL) *comma = '\0';
            Trim(next);
            parts[count++] = next;
            next = comma != NULL ? comma + 1 : NULL;
        }
        return next != NULL ? MOST_SETTING_PARTS + 1 : count;
    }

    bool ParseFloat(const char* text, float* out) {
        char* end = NULL;
        double value = strtod(text, &end);
        if (end == text) return false;
        while (*end == ' ' || *end == '\t') ++end;
        if (*end != '\0' || !(value == value) || value > FLT_MAX || value < -FLT_MAX) return false;
        *out = (float)value;
        return true;
    }

    bool ParseValue(const Setting& setting, const char* text, float* value) {
        if (setting.toggle != NULL) {
            if (_stricmp(text, "true") != 0 && _stricmp(text, "false") != 0) return false;
            *value = (float)(_stricmp(text, "true") == 0 ? 1 : 0);
            return true;
        }
        if (!ParseFloat(text, value)) return false;
        if (*value < setting.min || *value > setting.max) *value = *value < setting.min ? setting.min : setting.max;
        return true;
    }

    void Store(int index, int level, bool race, float value) {
        Values& values = gValues[index];
        if (level == 0) {
            if (values.hasBase) return;
            values.base = value;
            values.hasBase = true;
            return;
        }
        WORD bit = (WORD)(1 << (level - 1));
        WORD& mask = race ? values.hasRace : values.hasHeat;
        if (mask & bit) return;
        (race ? values.race : values.heat)[level - 1] = value;
        mask = (WORD)(mask | bit);
    }

    float Resolve(int index, int level, bool racing) {
        const Values& values = gValues[index];
        if (level >= LOWEST_HEAT && level <= HEAT_LEVELS) {
            WORD bit = (WORD)(1 << (level - 1));
            if (racing && (values.hasRace & bit)) return values.race[level - 1];
            if (values.hasHeat & bit) return values.heat[level - 1];
            if (!values.hasBase && kSettings[index].heatDefaults != NULL) return kSettings[index].heatDefaults[level - 1];
            if (!values.hasBase && kSettings[index].heatToggles != NULL) return (float)(kSettings[index].heatToggles[level - 1] ? 1 : 0);
        }
        return values.hasBase ? values.base : values.vanilla;
    }

    void ReadKey(const char* path, int first, int parts, const char* key, int level, bool race) {
        char text[MOST_VALUE_TEXT] = "";
        if (GetPrivateProfileStringA(kSettings[first].section, key, "", text, sizeof(text), path) == 0) return;

        char* partText[MOST_SETTING_PARTS] = { NULL };
        if (SplitParts(text, partText) != parts) return;

        float values[MOST_SETTING_PARTS];
        for (int part = 0; part < parts; ++part)
            if (!ParseValue(kSettings[first + part], partText[part], &values[part])) return;
        for (int part = 0; part < parts; ++part) Store(first + part, level, race, values[part]);
    }

    void ReadFile(const char* path) {
        for (int first = 0; first < SETTING_COUNT; first += PartCount(first)) {
            int parts = PartCount(first);
            if (kSettings[first].hasDefault) ReadKey(path, first, parts, "default", 0, false);
            for (int level = LOWEST_HEAT; level <= HEAT_LEVELS; ++level) {
                char key[8];
                sprintf_s(key, sizeof(key), "heat%02d", level);
                ReadKey(path, first, parts, key, level, false);
                sprintf_s(key, sizeof(key), "race%02d", level);
                ReadKey(path, first, parts, key, level, true);
            }
        }
    }

    bool IsName(const char* text) {
        size_t length = strlen(text);
        if (length == 0 || length >= MOST_NAME_TEXT) return false;
        for (size_t i = 0; i < length; ++i) {
            char c = text[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
        }
        return true;
    }

    void StoreText(int index, int level, bool race, const char* text) {
        TextValues& values = gTextValues[index];
        if (level == 0) {
            if (values.hasBase) return;
            strcpy_s(values.base, sizeof(values.base), text);
            values.hasBase = true;
            return;
        }
        WORD bit = (WORD)(1 << (level - 1));
        WORD& mask = race ? values.hasRace : values.hasHeat;
        if (mask & bit) return;
        strcpy_s(race ? values.race[level - 1] : values.heat[level - 1], MOST_NAME_TEXT, text);
        mask = (WORD)(mask | bit);
    }

    const char* ResolveText(int index, int level, bool racing) {
        const TextValues& values = gTextValues[index];
        if (level >= LOWEST_HEAT && level <= HEAT_LEVELS) {
            WORD bit = (WORD)(1 << (level - 1));
            if (racing && (values.hasRace & bit)) return values.race[level - 1];
            if (values.hasHeat & bit) return values.heat[level - 1];
        }
        return values.hasBase ? values.base : values.vanilla;
    }

    bool ApplyText(int index, int level, bool racing) {
        const char* value = ResolveText(index, level, racing);
        char* text = kTextSettings[index].text;
        if (strcmp(text, value) == 0) return false;
        strcpy_s(text, MOST_NAME_TEXT, value);
        return true;
    }

    void ReadTextKey(const char* path, int index, const char* key, int level, bool race) {
        char text[MOST_VALUE_TEXT] = "";
        if (GetPrivateProfileStringA(kTextSettings[index].section, key, "", text, sizeof(text), path) == 0) return;
        Trim(text);
        if (!IsName(text)) return;
        StoreText(index, level, race, text);
    }

    void ReadTextFile(const char* path) {
        for (int index = 0; index < TEXT_SETTING_COUNT; ++index) {
            if (kTextSettings[index].hasDefault) ReadTextKey(path, index, "default", 0, false);
            for (int level = LOWEST_HEAT; level <= HEAT_LEVELS; ++level) {
                char key[8];
                sprintf_s(key, sizeof(key), "heat%02d", level);
                ReadTextKey(path, index, key, level, false);
                sprintf_s(key, sizeof(key), "race%02d", level);
                ReadTextKey(path, index, key, level, true);
            }
        }
    }

    bool FileExists(const char* path) {
        return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    }

}

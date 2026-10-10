#pragma once
#include "Interfaces.hpp"

namespace Ini {

    struct Setting {
        const char* section;
        Log::Name   name;
        float*      number;
        bool*       toggle;
        float       min;
        float       max;
        bool        hasDefault;
    };

    void Load(void* module);
    bool ApplyHeat(int level, bool racing);

}

class AIPerpVehicle {
  public:
    static constexpr uintptr_t kSetHeat            = 0x00409060u;
    static constexpr uint8_t   kSetHeatPrologue[7] = { 0x6A, 0xFF, 0x68, 0xBC, 0x79, 0x86, 0x00 };

    static bool HookSetHeat() {
        return Detour::Install("AIPerpVehicle::SetHeat", kSetHeat, kSetHeatPrologue, sizeof(kSetHeatPrologue), &SetHeatEntry);
    }

    static void __cdecl SetHeatEntry(Detour::Registers* registers);
    static void UpdateHeat();

    bool IsLocalPlayer() const;

    unsigned char mBehavior[0x34];
    ISimable*     mIOwner;
    unsigned char mAIVehiclePid[0x720];
    unsigned char mIPerpetrator[0x1C];
    float         mHeat;
    int           mCostToState;
    int           mPendingRepPointsNormal;
    int           mPendingRepPointsFromCopDestruction;
    bool          mHiddenFromCars;
    bool          mHiddenFromHelicopters;
    bool          mWasInRaceEventLastHeatUpdate;
};

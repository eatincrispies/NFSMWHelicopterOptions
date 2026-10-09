#pragma once
#include "Interfaces.hpp"

class AIPerpVehicle {
  public:
    static bool HookSetHeat();
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

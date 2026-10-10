#pragma once
#include "Interfaces.hpp"

struct Config {
    float LeadBase                = 30.0f;
    float LeadMax                 = 45.0f;

    float CrushHover              = 0.0f;
    float CrushHeight             = 0.0f;

    float TurnClamp               = 1.3f;
    float TurnResponseScale       = -8.0f;
    float MaxChopperAccel         = 80.0f;
    float MinChopperAccel         = 30.0f;
    float StoppingRatio           = 1.55f;
    float BrakeSpeedScale         = 0.4f;

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

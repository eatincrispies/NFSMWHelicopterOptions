#pragma once
#include "../../dllmain.hpp"

class IAIHelicopter;
class IPursuitAI;
class IVehicle;
class IVehicleAI;

class AIActionHeliExit {
  public:
    enum kExitMode {
        kSeekUp  = 0,
        kSeekCar = 1,
        kFlyOut  = 2
    };

    struct Settings {
        Settings();

        float FlySpeed;
    };

    static Settings sSettings;

    static void InstallPatches();
    static void Refresh();

    void FlyAway(float dT);

    unsigned char  mAIAction[0x48];
    IVehicleAI*    mIVehicleAI;
    IVehicle*      mIVehicle;
    IRigidBody*    mIRigidBody;
    IAIHelicopter* mIAIHelicopter;
    IPursuitAI*    mIPursuitAI;
    float          mExitTime;
    bool           mBuildingPath;
    UMath::Vector3 mSeekPosition;
    kExitMode      mExitMode;
};

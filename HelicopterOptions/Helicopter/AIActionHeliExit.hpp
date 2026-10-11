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
        kFlyOut  = 2,
    };

    struct Settings {
        float FlySpeed = 100.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:FlySpeed", "FlySpeed", &sSettings.FlySpeed, nullptr, 40.0f, 220.0f, true },
    };

    static constexpr uintptr_t        kUpdate         = 0x004279F0u;
    static constexpr uintptr_t        kVTableUpdate   = 0x008913A4u;
    static constexpr Patch::FloatPush kUpdateFlySpeed = { 0x00427BFEu, 100.0f };

    static inline bool  sPatched  = false;
    static inline float sFlySpeed = 0.0f;

    static void InstallPatches() {
        Patch::Begin("AIActionHeliExit::Update");
        Patch::PushFloat("flySpeed", kUpdateFlySpeed, sSettings.FlySpeed);
        sPatched = Patch::Commit();
        sFlySpeed = sSettings.FlySpeed;

        Patch::Begin("AIActionHeliExit vtable");
        Patch::RedirectVirtual("Update", kVTableUpdate, kUpdate, Game::MethodAddress(&AIActionHeliExit::FlyAway));
        Patch::Commit();
    }

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

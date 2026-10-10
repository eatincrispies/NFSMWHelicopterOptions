#pragma once
#include "../../dllmain.hpp"
#include "HeliSheet.hpp"

class AIActionHeliPursuit;
class ISimpleChopper;
class WRoadNav;

class IAIHelicopter : public UTL::COM::IUnknown {
  public:
    virtual float GetDesiredHeightOverDest() const = 0;
    virtual void SetDesiredHeightOverDest(const float height) = 0;
    virtual void SetLookAtPosition(UMath::Vector3 la) = 0;
    virtual UMath::Vector3 GetLookAtPosition() const = 0;
    virtual void SetDestinationVelocity(const UMath::Vector3& v) = 0;
    virtual void SteerToNav(WRoadNav* road_nav, float height, float speed, bool bStopAtDest) = 0;
    virtual bool StartPathToPoint(UMath::Vector3& point) = 0;
    virtual bool StrafeToDestIsSet() const = 0;
    virtual void SetStrafeToDest(bool strafe) = 0;
    virtual bool FilterHeliAltitude(UMath::Vector3& point) = 0;
    virtual void RestrictPointToRoadNet(UMath::Vector3& seekPosition) = 0;
};

class AIVehicleHelicopter {
  public:
    struct Settings {
        float StoppingRatio   = 1.55f;
        float BrakeSpeedScale = 0.4f;
        float LineOfSight     = 0.0f;
        float FuelTime        = 0.0f;
        float SpawnDistance   = 250.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:Brake",         "StoppingRatio",   &sSettings.StoppingRatio,   nullptr, 0.3f,  10.0f,   true },
        { "Helicopter:Brake",         "BrakeSpeedScale", &sSettings.BrakeSpeedScale, nullptr, 0.0f,  1.0f,    true },
        { "Helicopter:LineOfSight",   "LineOfSight",     &sSettings.LineOfSight,     nullptr, 0.0f,  5000.0f, false },
        { "Helicopter:FuelTime",      "FuelTime",        &sSettings.FuelTime,        nullptr, 0.0f,  3600.0f, false },
        { "Helicopter:SpawnDistance", "SpawnDistance",   &sSettings.SpawnDistance,   nullptr, 30.0f, 600.0f,  true },
    };

    static constexpr uintptr_t kOnDriving                              = 0x00417A20u;
    static constexpr uint8_t   kOnDrivingPrologue[5]                   = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
    static constexpr uintptr_t kCanSeeTargetHeliLOSDistance            = 0x00417146u;
    static constexpr uint8_t   kCanSeeTargetHeliLOSDistancePrologue[8] = { 0x89, 0x54, 0x24, 0x24, 0xD9, 0x44, 0x24, 0x24 };
    static constexpr uintptr_t kAvoidCamera                            = 0x00417790u;

    static constexpr Patch::CallSite     kOnDrivingAvoidCamera     = { 0x00417B02u, kAvoidCamera };
    static constexpr Patch::FloatOperand kOnDrivingStoppingRatio   = { 0x00417C88u, { 0xD8, 0x0D }, 0x008910FCu };
    static constexpr Patch::FloatOperand kOnDrivingBrakeSpeedScale = { 0x00417CA7u, { 0xD8, 0x0D }, 0x00891054u };
    static constexpr Patch::FloatPush    kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABFu, 250.0f };

    struct Brake {
        float stoppingRatio;
        float speedScale;
    };

    static inline Brake sBrake         = {};
    static inline bool  sSpawnPatched  = false;
    static inline float sSpawnDistance = 0.0f;

    static void InstallPatches() {
        Refresh();

        Patch::Begin("AIVehicleHelicopter::OnDriving");
        Patch::RedirectCall("AvoidCamera", kOnDrivingAvoidCamera, Game::MethodAddress(&AIVehicleHelicopter::AvoidCameraHook));
        Patch::RedirectFloat("stopping distance ratio", kOnDrivingStoppingRatio, &sBrake.stoppingRatio);
        Patch::RedirectFloat("braking mDriveSpeed scale", kOnDrivingBrakeSpeedScale, &sBrake.speedScale);
        Patch::Commit();

        Patch::Begin("AICopManager::SpawnPursuitHelicopter");
        Patch::PushFloat("testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition, sSettings.SpawnDistance);
        sSpawnPatched = Patch::Commit();
        sSpawnDistance = sSettings.SpawnDistance;
    }

    static bool HookOnDriving() {
        return Detour::Install("AIVehicleHelicopter::OnDriving", kOnDriving, kOnDrivingPrologue, sizeof(kOnDrivingPrologue), &OnDrivingEntry);
    }

    static bool HookCanSeeTarget() {
        return Detour::Install("AIVehicleHelicopter::CanSeeTarget heliLOSdistance", kCanSeeTargetHeliLOSDistance,
                               kCanSeeTargetHeliLOSDistancePrologue, sizeof(kCanSeeTargetHeliLOSDistancePrologue), &CanSeeTargetEntry);
    }

    static void Refresh();
    static void __cdecl OnDrivingEntry(Detour::Registers* registers);
    static void __cdecl CanSeeTargetEntry(Detour::Registers* registers);

    IRigidBody* GetRigidBody() const {
        return mIOwner != nullptr ? mIOwner->GetRigidBody() : nullptr;
    }

    void UpdateHelicopterOptions();
    void BeginHelicopter(const UMath::Vector3& position);
    void AvoidCamera(UMath::Vector3& dest);
    void AvoidCameraHook(UMath::Vector3& dest);

#if defined(_DEBUG)
    void LogLive(const IRigidBody* rigidBody, const AIActionHeliPursuit* pursuit) const;
#else
    void LogLive(const IRigidBody*, const AIActionHeliPursuit*) const {}
#endif

    unsigned char       mBehavior[0x34];
    ISimable*           mIOwner;
    unsigned char       mAIVehicle[0x4C];
    float               mDriveSpeed;
    UMath::Vector3      mDest;
    unsigned char       mAIVehiclePursuit[0x710];
    unsigned char       mIAIHelicopter[0x8];
    UMath::Vector3      mDestinationVelocity;
    UMath::Vector3      mLookAtPosition;
    UMath::Vector3      mLastPlaceHeliSawPerp;
    float               mHeight;
    bool                mStrafeToDest;
    bool                mPerpHiddenFromMe;
    float               mHeliFuelTimeRemaining;
    float               mShadowScale;
    float               mDustStormIntensity;
    unsigned char       mHeliSheetCoordAlignment[0xC];
    HeliSheetCoordinate mHeliSheetCoord;
    HeliSheetCoordinate mSecondaryHeliSheetCoord;
    HeliSheetCoordinate mThirdHeliSheetCoord;
    ISimpleChopper*     mISimpleChopper;
};

bool HeliVehicleActive();

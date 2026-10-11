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
        float AllAroundSight  = 75.0f;
        bool  FrontSightOnly  = true;
        float RespawnDelay    = 0.0f;
        bool  HidingSpots     = true;
        float DespawnDistance = 600.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:Brake",         "StoppingRatio",   &sSettings.StoppingRatio,   nullptr, 0.3f,  10.0f,   true },
        { "Helicopter:Brake",         "BrakeSpeedScale", &sSettings.BrakeSpeedScale, nullptr, 0.0f,  1.0f,    true },
        { "Helicopter:LineOfSight",   "LineOfSight",     &sSettings.LineOfSight,     nullptr, 0.0f,  5000.0f, false },
        { "Helicopter:FuelTime",      "FuelTime",        &sSettings.FuelTime,        nullptr, 0.0f,  3600.0f, false },
        { "Helicopter:SpawnDistance", "SpawnDistance",   &sSettings.SpawnDistance,   nullptr, 30.0f, 600.0f,  true },
        { "Helicopter:AllAroundSight", "AllAroundSight", &sSettings.AllAroundSight,  nullptr, 0.0f,  1000.0f, true },
        { "Helicopter:FrontSightOnly", "FrontSightOnly", nullptr, &sSettings.FrontSightOnly,          0.0f,  1.0f,    true },
        { "Helicopter:Respawning",     "RespawnDelay",   &sSettings.RespawnDelay,    nullptr, 0.0f,  600.0f,  false },
        { "Helicopter:HidingSpots",    "HidingSpots",    nullptr, &sSettings.HidingSpots,             0.0f,  1.0f,    true },
        { "Helicopter:DespawnDistance", "DespawnDistance", &sSettings.DespawnDistance, nullptr, 100.0f, 5000.0f, true },
    };

    static constexpr uintptr_t kOnDriving                              = 0x00417A20u;
    static constexpr uint8_t   kOnDrivingPrologue[5]                   = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
    static constexpr uintptr_t kCanSeeTargetHeliLOSDistance            = 0x00417146u;
    static constexpr uint8_t   kCanSeeTargetHeliLOSDistancePrologue[8] = { 0x89, 0x54, 0x24, 0x24, 0xD9, 0x44, 0x24, 0x24 };

    static constexpr Patch::FloatOperand kOnDrivingStoppingRatio   = { 0x00417C88u, { 0xD8, 0x0D }, 0x008910FCu };
    static constexpr Patch::FloatOperand kOnDrivingBrakeSpeedScale = { 0x00417CA7u, { 0xD8, 0x0D }, 0x00891054u };
    static constexpr Patch::FloatPush    kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABFu, 250.0f };
    static constexpr Patch::FloatOperand kCanSeeTargetFacing       = { 0x004171FAu, { 0xD8, 0x1D }, 0x00890968u };
    static constexpr uintptr_t           kHeliVisualSphere         = 0x008EB30Cu;
    static constexpr float               kGameHeliVisualSphere     = 75.0f;
    static constexpr float               kFrontFacing              = 0.0f;
    static constexpr float               kAnyFacing                = -2.0f;
    static constexpr Patch::ByteStore    kHidingSpotHeliStore      = { 0x0042995Cu, { 0xC6, 0x86, 0x85, 0x07, 0x00, 0x00 }, 1 };
    static constexpr Patch::FloatOperand kCopManagerChopperDespawn = { 0x0043180Eu, { 0xD9, 0x05 }, 0x00891988u };
    static constexpr uintptr_t           kRemoveVehicleSpawnHeliTimer           = 0x0042B6D9u;
    static constexpr uint8_t             kRemoveVehicleSpawnHeliTimerPrologue[6] = { 0x89, 0x8F, 0xE0, 0x00, 0x00, 0x00 };

    struct Brake {
        float stoppingRatio;
        float speedScale;
    };

    static inline Brake sBrake         = {};
    static inline bool  sSpawnPatched  = false;
    static inline float sSpawnDistance = 0.0f;
    static inline float sSightFacing   = kFrontFacing;
    static inline bool  sSightPatched  = false;
    static inline float sVisualSphere  = 0.0f;
    static inline bool  sHidingPatched = false;
    static inline bool  sHidingSpots   = true;
    static inline float sDespawnDistance = 0.0f;

    static void InstallPatches() {
        Refresh();

        Patch::Begin("AIVehicleHelicopter::OnDriving");
        Patch::RedirectFloat("stopping distance ratio", kOnDrivingStoppingRatio, &sBrake.stoppingRatio);
        Patch::RedirectFloat("braking mDriveSpeed scale", kOnDrivingBrakeSpeedScale, &sBrake.speedScale);
        Patch::Commit();

        Patch::Begin("AICopManager::SpawnPursuitHelicopter");
        Patch::PushFloat("testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition, sSettings.SpawnDistance);
        sSpawnPatched = Patch::Commit();
        sSpawnDistance = sSettings.SpawnDistance;

        Patch::Begin("AIVehicleHelicopter::CanSeeTarget");
        Patch::DataFloat("kHeliVisualSphere", kHeliVisualSphere, kGameHeliVisualSphere, sSettings.AllAroundSight);
        Patch::RedirectFloat("Dot(forwardVec, heli2Perp) > 0.0", kCanSeeTargetFacing, &sSightFacing);
        sSightPatched = Patch::Commit();
        sVisualSphere = sSettings.AllAroundSight;

        Patch::Begin("AICopManager despawn check");
        Patch::RedirectFloat("compDist = bIsChopper ? 600.0 : 375.0", kCopManagerChopperDespawn, &sDespawnDistance);
        Patch::Commit();

        Patch::Begin("AIVehicle hiding zone");
        Patch::StoreByte("mHiddenFromHelicopters = true", kHidingSpotHeliStore, sSettings.HidingSpots ? 1 : 0);
        sHidingPatched = Patch::Commit();
        sHidingSpots = sSettings.HidingSpots;
    }

    static bool HookRespawnTimer() {
        return Detour::Install("AIPursuit mSpawnHeliTimer = TimeBetweenHeliActive", kRemoveVehicleSpawnHeliTimer, kRemoveVehicleSpawnHeliTimerPrologue,
                               sizeof(kRemoveVehicleSpawnHeliTimerPrologue), &RespawnTimerEntry);
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
    static void __cdecl RespawnTimerEntry(Detour::Registers* registers);

    IRigidBody* GetRigidBody() const {
        return mIOwner != nullptr ? mIOwner->GetRigidBody() : nullptr;
    }

    void UpdateHelicopterOptions();
    void BeginHelicopter(const UMath::Vector3& position);

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

class EAXAirSupport {
  public:
    static constexpr uintptr_t kUpdateSoundAI            = 0x00709F58u;
    static constexpr uint8_t   kUpdateSoundAIPrologue[5] = { 0xA1, 0xC8, 0x3C, 0x99, 0x00 };

    static bool HookUpdate() {
        return Detour::Install("EAXAirSupport::Update", kUpdateSoundAI, kUpdateSoundAIPrologue, sizeof(kUpdateSoundAIPrologue), &UpdateEntry);
    }

    static void __cdecl UpdateEntry(Detour::Registers* registers);

    bool IsDead();
    void Bailout();
    void HeliDestroyed();
};

class AICopManager {
  public:
    static constexpr uintptr_t kApplyBreakerZones            = 0x0042E930u;
    static constexpr uint8_t   kApplyBreakerZonesPrologue[6] = { 0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9 };

    static bool HookApplyBreakerZones() {
        return Detour::Install("AICopManager::ApplyBreakerZones", kApplyBreakerZones, kApplyBreakerZonesPrologue, sizeof(kApplyBreakerZonesPrologue),
                               &ApplyBreakerZonesEntry);
    }

    static void __cdecl ApplyBreakerZonesEntry(Detour::Registers* registers);

    bool HasVehicle(uintptr_t vehicle) const;
    uintptr_t FindVehicle(const ISimable* simable) const;
    void UpdateBreakerFire();
};

#pragma once
#include "../../dllmain.hpp"
#include "HeliSheet.hpp"

class AIActionHeliPursuit;
class ISimpleChopper;
class IPursuit;
class IVehicleAI;
class WRoadNav;
struct AITarget;

class IPursuitAI : public UTL::COM::IUnknown {
  public:
    static constexpr unsigned kGetPursuitTarget          = 0x24u;
    static constexpr unsigned kGetTimeSinceTargetSeen    = 0x74u;
    static constexpr unsigned kWasWithinEngagementRadius = 0x8Cu;

    AITarget* GetPursuitTarget() const {
        return Game::ThisCall<AITarget*>(Game::VirtualFunction(this, kGetPursuitTarget), this);
    }

    float GetTimeSinceTargetSeen() const {
        return Game::ThisCall<float>(Game::VirtualFunction(this, kGetTimeSinceTargetSeen), this);
    }

    bool WasWithinEngagementRadius() const {
        return Game::ThisCall<bool>(Game::VirtualFunction(this, kWasWithinEngagementRadius), this);
    }
};

class IVehicle : public UTL::COM::IUnknown {
  public:
    struct List {
        void*      mVTable;
        IVehicle** mBegin;
        size_t     mCapacity;
        size_t     mSize;
    };

    static constexpr uintptr_t kAICops            = 0x0092CE98u;
    static constexpr uintptr_t kCHOPPER           = 0x0092C4F4u;
    static constexpr uintptr_t kFindInterface     = 0x005D59F0u;
    static constexpr uintptr_t kIPursuitAIHandle  = 0x004038E0u;
    static constexpr unsigned  kGetSimable        = 0x04u;
    static constexpr unsigned  kGetPosition       = 0x0Cu;
    static constexpr unsigned  kGetVehicleClass   = 0x44u;
    static constexpr unsigned  kIsDestroyed       = 0x7Cu;
    static constexpr unsigned  kIsActive          = 0x88u;
    static constexpr unsigned  kGetAIVehiclePtr   = 0xACu;

    static const List& AICops() {
        return Game::Global<List>(kAICops);
    }

    ISimable* GetSimable() const {
        return Game::ThisCall<ISimable*>(Game::VirtualFunction(this, kGetSimable), this);
    }

    bool IsChopper() const {
        const uint32_t* vehicleClass = Game::ThisCall<const uint32_t*>(Game::VirtualFunction(this, kGetVehicleClass), this);
        return vehicleClass != nullptr && *vehicleClass == Game::Global<uint32_t>(kCHOPPER);
    }

    bool IsDestroyed() const {
        return Game::ThisCall<bool>(Game::VirtualFunction(this, kIsDestroyed), this);
    }

    bool IsActive() const {
        return Game::ThisCall<bool>(Game::VirtualFunction(this, kIsActive), this);
    }

    IVehicleAI* GetAIVehiclePtr() const {
        return Game::ThisCall<IVehicleAI*>(Game::VirtualFunction(this, kGetAIVehiclePtr), this);
    }

    IPursuitAI* GetPursuitAI() const {
        const void* object = *reinterpret_cast<const void* const*>(reinterpret_cast<uintptr_t>(this) + sizeof(void*));
        return object != nullptr ? Game::ThisCall<IPursuitAI*>(kFindInterface, object, kIPursuitAIHandle) : nullptr;
    }

    bool IsActiveOnMinimap() const;
};

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
        float Helicopters     = 1.0f;
    };

    static constexpr int kMostHelicopters = 4;

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
        { "Helicopter:Helicopters",    "Helicopters",     &sSettings.Helicopters,     nullptr, 1.0f,   4.0f,    true },
    };

    static constexpr uintptr_t kOnDriving                              = 0x00417A20u;
    static constexpr uintptr_t kAvoidCamera                            = 0x00417790u;
    static constexpr Patch::CallSite kOnDrivingAvoidCamera             = { 0x00417B02u, kAvoidCamera };
    static constexpr float     kNoBrakeRatio                           = 10000.0f;
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

        Patch::Begin("AIVehicleHelicopter::OnDriving AvoidCamera");
        Patch::RedirectCall("AvoidCamera", kOnDrivingAvoidCamera, Game::MethodAddress(&AIVehicleHelicopter::AvoidCameraHook));
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
    static int  Helicopters();
    static void __cdecl OnDrivingEntry(Detour::Registers* registers);
    static void __cdecl CanSeeTargetEntry(Detour::Registers* registers);
    static void __cdecl RespawnTimerEntry(Detour::Registers* registers);

    IRigidBody* GetRigidBody() const {
        return mIOwner != nullptr ? mIOwner->GetRigidBody() : nullptr;
    }

    void UpdateHelicopterOptions();
    void AvoidCameraHook(UMath::Vector3& dest);
    void BeginHelicopter(const UMath::Vector3& position);
    void KeepApart(IRigidBody* rigidBody) const;

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

namespace CollisionGeometry {

    struct Bounds {
        int16_t     fOrientation[4];
        int16_t     fPosition[3];
        uint16_t    fFlags;
        int16_t     fHalfDimensions[3];
        uint8_t     fNumChildren;
        int8_t      fPCloudIndex;
        int16_t     fPivot[3];
        int16_t     fChildIndex;
        float       fRadius;
        uint32_t    fSurface;
        uint32_t    fNameHash;
        const void* fCollection;
    };

    struct Collection {
        uint32_t fNameHash;
        int      fNumBounds;
        int      fIsResolved;
        int      fPad;
    };

    class Collections {
      public:
        static constexpr uintptr_t       kFind                  = 0x00681340u;
        static constexpr Patch::CallSite kPVehicleConstructFind = { 0x00689A09u, kFind };
        static constexpr UMath::Vector3  kHeliCabinOffset       = { 0.0f, 0.212f, 1.175f };
        static constexpr UMath::Vector3  kHeliCabinHalf         = { 1.0f, 1.1f, 2.25f };
        static constexpr UMath::Vector3  kHeliTailOffset        = { 0.0f, 0.707f, -3.775f };
        static constexpr UMath::Vector3  kHeliTailHalf          = { 0.5f, 0.95f, 2.5f };

        static void InstallPatches() {
            Patch::Begin("PVehicle::Construct CollisionGeometry::Collections::Find");
            Patch::RedirectCall("Collections::Find", kPVehicleConstructFind, Game::MethodAddress(&Collections::VehicleFind));
            Patch::Commit();
        }

        const Collection* Find(uint32_t name);
        const Collection* VehicleFind(uint32_t name);
    };

}

class AICopManager {
  public:
    static constexpr uintptr_t kApplyBreakerZones            = 0x0042E930u;
    static constexpr uint8_t   kApplyBreakerZonesPrologue[6] = { 0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9 };
    static constexpr uintptr_t kSpawnPursuitHelicopter       = 0x004269A0u;

    static bool HookApplyBreakerZones() {
        return Detour::Install("AICopManager::ApplyBreakerZones", kApplyBreakerZones, kApplyBreakerZonesPrologue, sizeof(kApplyBreakerZonesPrologue),
                               &ApplyBreakerZonesEntry);
    }

    static void __cdecl ApplyBreakerZonesEntry(Detour::Registers* registers);

    bool HasVehicle(uintptr_t vehicle) const;
    void UpdateBreakerFire();
    IPursuit* GetPlayerPursuit() const;
    int GetNumActiveCopHelicopters() const;
    bool SpawnPursuitHelicopter(IPursuit* ipursuit);
    void UpdateHelicopterTopUp();
};

class FEObject;

struct FEMinList {
    static constexpr uintptr_t kAddNode = 0x005AEE90u;

    void AddNode(FEObject* after, FEObject* node) {
        Game::ThisCall<void>(kAddNode, this, after, node);
    }

    void*     mVTable;
    unsigned  NumElements;
    FEObject* Head;
    FEObject* Tail;
};

class FEObject {
  public:
    static constexpr int      FE_Group     = 5;
    static constexpr uint32_t FF_Invisible = 1u;
    static constexpr unsigned kClone       = 0x04u;

    FEObject* Clone(bool bReference) {
        return Game::ThisCall<FEObject*>(Game::VirtualFunction(this, kClone), this, bReference);
    }

    void*          mVTable;
    FEObject*      Next;
    FEObject*      Prev;
    uint32_t       GUID;
    uint32_t       NameHash;
    char*          pName;
    int            Type;
    uint32_t       Flags;
    uint16_t       RenderContext;
    uint16_t       ResourceIndex;
    uint32_t       Handle;
    uint32_t       UserParam;
    unsigned char* pData;
    uint32_t       DataSize;
    FEMinList      Responses;
    FEMinList      Scripts;
    void*          pCurrentScript;
    void*          Cached;
};

class FEGroup : public FEObject {
  public:
    FEMinList Children;
};

class FEMultiImage : public FEObject {
  public:
    static constexpr int FE_MultiImage = 12;

    uint32_t ImageFlags;
    uint32_t hTexture[3];
    uint32_t TextureFlags[3];
};

class FEPackage {
  public:
    unsigned char mPackage[0x44];
    FEMinList     Objects;
};

struct bPList {
    static constexpr uintptr_t kAddTail = 0x0058AF10u;

    void AddTail(FEObject* object) {
        Game::ThisCall<void>(kAddTail, this, object);
    }

    void* HeadNode[2];
};

class cFEng {
  public:
    static constexpr uintptr_t kmInstance   = 0x0091CADCu;
    static constexpr uintptr_t kFindPackage = 0x00516AF0u;

    static cFEng* Get() {
        return Game::Global<cFEng*>(kmInstance);
    }

    FEPackage* FindPackage(const char* name) {
        return Game::ThisCall<FEPackage*>(kFindPackage, this, name);
    }
};

class Minimap {
  public:
    static constexpr uintptr_t           kUpdateCopElements         = 0x00579E00u;
    static constexpr uintptr_t           kUpdateElementArt          = 0x005675D0u;
    static constexpr Patch::CallSite     kUpdateUpdateCopElements   = { 0x0059DCFDu, kUpdateCopElements };
    static constexpr Patch::IndirectCall kUpdateCopElementsIsActive = { 0x00579E94u, { 0xFF, 0x90, 0x88, 0x00, 0x00, 0x00 } };

    static void InstallPatches() {
        Patch::Begin("Minimap::UpdateCopElements");
        Patch::RedirectCall("UpdateCopElements", kUpdateUpdateCopElements, Game::MethodAddress(&Minimap::UpdateHelicopterElements));
        Patch::RedirectIndirectCall("(*iter)->IsActive()", kUpdateCopElementsIsActive, Game::MethodAddress(&IVehicle::IsActiveOnMinimap));
        Patch::Commit();
    }

    void UpdateCopElements(IVehicle* ivehicle);
    void UpdateHelicopterElements(IVehicle* ivehicle);
    void UpdateElementArt(bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse);
    void UpdateHelicopterElement(IVehicle* helicopter, FEObject* heliArt, FEObject* lineOfSiteArt);
    bool ShowsPursuitCops(IVehicle* ivehicle) const;

    unsigned char mHudElement[0x8];
    bPList        Objects;
    const char*   pPackageName;
    unsigned char mMinimap[0xB4];
    FEObject*     mHeliElementArt;
    FEObject*     mHeliLineOfSiteArt;
};

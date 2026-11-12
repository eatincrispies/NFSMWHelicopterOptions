#pragma once
#include "../../dllmain.hpp"
#include "HeliSheet.hpp"

#define MOST_HELICOPTERS                        4

#define AI_COPS                                 0x0092CE98
#define VEHICLE_CLASS_CHOPPER                   0x0092C4F4
#define FIND_INTERFACE                          0x005D59F0
#define IPURSUITAI_HANDLE                       0x004038E0
#define IAIHELICOPTER_HANDLE                    0x00404060
#define SOUND_AI                                0x00993CC8
#define SOUND_AI_FLAGS                          0x054
#define SOUND_AI_HELI                           0x0E0
#define SOUND_AI_PURSUIT                        0x130
#define SOUND_AI_FOCUS                          0x138
#define SOUND_AI_PLAYER_ROAD                    0x1A8
#define SOUND_AI_REPORT_ROAD                    0x1C8
#define SOUND_AI_PURSUIT_STATE                  0x1D4
#define CFENG_INSTANCE                          0x0091CADC
#define CFENG_FIND_PACKAGE                      0x00516AF0
#define FEMINLIST_ADD_NODE                      0x005AEE90
#define BPLIST_ADD_TAIL                         0x0058AF10
#define CSTATEMGR_HELICOPTER_CREATE_STATE       0x004E5970

#define IPURSUITAI_START_FLEE                   0x00C
#define IPURSUITAI_GET_PURSUIT_TARGET           0x024
#define IPURSUITAI_GET_TIME_SINCE_TARGET_SEEN   0x074
#define IPURSUITAI_CAN_SEE_TARGET               0x07C
#define IPURSUITAI_WAS_WITHIN_ENGAGEMENT_RADIUS 0x08C

#define IVEHICLE_GET_SIMABLE                    0x004
#define IVEHICLE_GET_POSITION                   0x00C
#define IVEHICLE_GET_VEHICLE_CLASS              0x044
#define IVEHICLE_IS_DESTROYED                   0x07C
#define IVEHICLE_ACTIVATE                       0x080
#define IVEHICLE_IS_ACTIVE                      0x088
#define IVEHICLE_GET_AI_VEHICLE_PTR             0x0AC

#define EAX_CHARACTER_GET_HANDLE                0x028
#define EAX_CHARACTER_SET_UNIT_NUMBER           0x040
#define EAX_CHARACTER_IS_ACTIVE                 0x060
#define EAX_CHARACTER_HAS_LOS                   0x070
#define EAX_AIR_SUPPORT_HAZARD_ALERT            0x214
#define EAX_AIR_SUPPORT_BAILOUT                 0x104
#define EAX_AIR_SUPPORT_INTENT_TO_BAIL          0x20C
#define EAX_COP_ATTEMPT_VEHICLE_STOP            0x080
#define EAX_COP_SPOTTER_REPLY                   0x11C
#define EAX_COP_SPOTTED                         0x140
#define EAX_COP_BAILOUT                         0x104
#define EAX_COP_OFFROAD                         0x150
#define EAX_AIR_SUPPORT_SELF_STRATEGY           0x090
#define EAX_AIR_SUPPORT_INTENT_TO_RAM           0x0F8

#define FEOBJECT_CLONE                          0x004
#define CSTATE_BASE_SETUP                       0x004

class AIActionHeliPursuit;
class IAIHelicopter;
class ISimpleChopper;
class IPursuit;
class IVehicleAI;
class WRoadNav;
struct AITarget;

typedef void* (__thiscall* FindInterfaceMethod)(const void* object, DWORD handle);
typedef bool  (__thiscall* TargetQuery)(void* object, AITarget* target);
typedef void* (__thiscall* CloneMethod)(void* object, bool reference);
typedef void* (__thiscall* FindPackageMethod)(void* feng, const char* name);
typedef void  (__thiscall* AddNodeMethod)(void* list, void* after, void* node);
typedef void* (__thiscall* CreateStateMethod)(void* manager, int stateInstType, int sfxFlags);
typedef void  (__thiscall* OffroadMethod)(void* cop, unsigned int id, bool subsequent);

class IPursuitAI : public UTL::COM::IUnknown {
  public:
    void StartFlee() {
        ((VoidMethod)VirtualFunction(this, IPURSUITAI_START_FLEE))(this);
    }

    AITarget* GetPursuitTarget() const {
        return (AITarget*)((PointerQuery)VirtualFunction(this, IPURSUITAI_GET_PURSUIT_TARGET))(this);
    }

    float GetTimeSinceTargetSeen() const {
        return ((FloatQuery)VirtualFunction(this, IPURSUITAI_GET_TIME_SINCE_TARGET_SEEN))(this);
    }

    bool CanSeeTarget(AITarget* target) {
        return ((TargetQuery)VirtualFunction(this, IPURSUITAI_CAN_SEE_TARGET))(this, target);
    }

    bool WasWithinEngagementRadius() const {
        return ((BoolQuery)VirtualFunction(this, IPURSUITAI_WAS_WITHIN_ENGAGEMENT_RADIUS))(this);
    }

    AITarget* PursuitRequest();
};

class IVehicle : public UTL::COM::IUnknown {
  public:
    struct List {
        void*      mVTable;
        IVehicle** mBegin;
        size_t     mCapacity;
        size_t     mSize;
    };

    static const List& AICops() {
        return *(const List*)AI_COPS;
    }

    static int ReadList(DWORD address, IVehicle** out, int most);

    ISimable* GetSimable() const {
        return (ISimable*)((PointerQuery)VirtualFunction(this, IVEHICLE_GET_SIMABLE))(this);
    }

    const UMath::Vector3* GetPosition() const {
        return (const UMath::Vector3*)((PointerQuery)VirtualFunction(this, IVEHICLE_GET_POSITION))(this);
    }

    bool IsChopper() const {
        const DWORD* vehicleClass = (const DWORD*)((PointerQuery)VirtualFunction(this, IVEHICLE_GET_VEHICLE_CLASS))(this);
        return vehicleClass != NULL && *vehicleClass == *(const DWORD*)VEHICLE_CLASS_CHOPPER;
    }

    bool IsDestroyed() const {
        return ((BoolQuery)VirtualFunction(this, IVEHICLE_IS_DESTROYED))(this);
    }

    void Activate() {
        ((VoidMethod)VirtualFunction(this, IVEHICLE_ACTIVATE))(this);
    }

    bool IsActive() const {
        return ((BoolQuery)VirtualFunction(this, IVEHICLE_IS_ACTIVE))(this);
    }

    IVehicleAI* GetAIVehiclePtr() const {
        return (IVehicleAI*)((PointerQuery)VirtualFunction(this, IVEHICLE_GET_AI_VEHICLE_PTR))(this);
    }

    IPursuitAI* GetPursuitAI() const {
        return (IPursuitAI*)FindInterface(IPURSUITAI_HANDLE);
    }

    IAIHelicopter* GetAIHelicopter() const {
        return (IAIHelicopter*)FindInterface(IAIHELICOPTER_HANDLE);
    }

    void* FindInterface(DWORD handle) const {
        const void* object = *(const void* const*)((DWORD)this + sizeof(void*));
        return object != NULL ? ((FindInterfaceMethod)FIND_INTERFACE)(object, handle) : NULL;
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
    virtual void SetFuelFull() = 0;
    virtual float GetFuelTimeRemaining() = 0;
};

class AIVehicleHelicopter {
  public:
    struct Settings {
        Settings();

        float StoppingRatio;
        float BrakeSpeedScale;
        float LineOfSight;
        float FuelTime;
        float HeatFuelTime[HEAT_LEVELS];
        float SpawnDistance;
        float AllAroundSight;
        bool  FrontSightOnly;
        float RespawnDelay;
        bool  HidingSpots;
        float DespawnDistance;
        float Helicopters;
        float HeatHelicopters[HEAT_LEVELS];
        bool  Falcon2;
        bool  HeatFalcon2[HEAT_LEVELS];
        float RecycleTime;
        float RecycleDistance;
        bool  Disp911Report;
        bool  HeatDisp911Report[HEAT_LEVELS];
        float Bounty;
        char  Pvehicle[MOST_NAME_TEXT];
    };

    static Settings sSettings;

    static void InstallPatches();
    static bool HookOnDriving();
    static bool HookCanSeeTarget();
    static bool HookRespawnTimer();
    static void Refresh();
    static int  Helicopters();
    static void __cdecl OnDrivingEntry(Detour::Registers* registers);
    static void __cdecl CanSeeTargetEntry(Detour::Registers* registers);
    static void __cdecl RespawnTimerEntry(Detour::Registers* registers);

    IRigidBody* GetRigidBody() const {
        return mIOwner != NULL ? mIOwner->GetRigidBody() : NULL;
    }

    bool OwnsHeliSheetCoord(const HeliSheetCoordinate* coord) const {
        return coord == &mHeliSheetCoord || coord == &mSecondaryHeliSheetCoord || coord == &mThirdHeliSheetCoord;
    }

    void UpdateFuel(float dT);
    void UpdateHelicopterOptions();
    void BeginHelicopter();
    void KeepApart(IRigidBody* rigidBody) const;
    void KeepAboveGround(IRigidBody* rigidBody) const;
    void AvoidCameraHook(UMath::Vector3& dest);

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
    enum Type_self_strategy_type {
        Type_self_strategy_type_Rolling_Roadblock = 2,
        Type_self_strategy_type_Herding           = 4
    };

    enum Type_speaker_call_sign_id {
        Type_speaker_call_sign_id_CallSign01 = 1,
        Type_speaker_call_sign_id_CallSign02 = 2
    };

    enum Type_pursuit_type {
        Type_pursuit_type_Possible_Wanted = 2
    };

    enum Type_heli_bailout_type {
        Type_heli_bailout_type_damage_sustained = 8
    };

    enum Type_heli_hazard_alert_type {
        Type_heli_hazard_alert_type_approaching_highway = 2,
        Type_heli_hazard_alert_type_approaching_city    = 8,
        Type_heli_hazard_alert_type_approaching_blimp   = 32
    };

    enum HazardSlot {
        kHazardHighway,
        kHazardCity,
        kHazardBlimp,
        kHazardSlots
    };

    static void InstallPatches();
    static bool HookSetHandle();
    static bool HookAddNewHeli();
    static bool HookScheduleSpeech();
    static void __cdecl SetHandleEntry(Detour::Registers* registers);
    static void __cdecl AddNewHeliEntry(Detour::Registers* registers);
    static void __cdecl ScheduleSpeechEntry(Detour::Registers* registers);
    static void UpdateCallSigns();
    static void UpdateHazardAlerts();

    static EAXAirSupport* SoundAIHeli() {
        const void* soundAI = *(const void* const*)SOUND_AI;
        return soundAI != NULL ? *(EAXAirSupport* const*)((DWORD)soundAI + SOUND_AI_HELI) : NULL;
    }

    HSIMABLE__* GetHandle() {
        return (HSIMABLE__*)((PointerQuery)VirtualFunction(this, EAX_CHARACTER_GET_HANDLE))(this);
    }

    void SetUnitNumber(int unitnum) {
        ((IntMethod)VirtualFunction(this, EAX_CHARACTER_SET_UNIT_NUMBER))(this, unitnum);
    }

    bool IsActive() {
        return ((BoolQuery)VirtualFunction(this, EAX_CHARACTER_IS_ACTIVE))(this);
    }

    bool HasLOS() {
        return ((BoolQuery)VirtualFunction(this, EAX_CHARACTER_HAS_LOS))(this);
    }

    void HazardAlert(int type) {
        ((IntMethod)VirtualFunction(this, EAX_AIR_SUPPORT_HAZARD_ALERT))(this, type);
    }

    void Say(DWORD slot) {
        ((VoidMethod)VirtualFunction(this, slot))(this);
    }

    void SelfStrategy(int type) {
        ((IntMethod)VirtualFunction(this, EAX_AIR_SUPPORT_SELF_STRATEGY))(this, type);
    }

    void IntentToRam() {
        ((VoidMethod)VirtualFunction(this, EAX_AIR_SUPPORT_INTENT_TO_RAM))(this);
    }

    void BackupArrives();
    void FixLineTags(Detour::Registers* registers);
    void LostVisual();
    void WarnLowFuel();
    void DeactivateBailout();
    void AnnounceWreck();
    bool FollowsWreck();
    bool FollowsChasingHelicopter();
    void DelayArrival();
    void UseCallSign(HSIMABLE__* handle);
    void AnnounceFirstArrival();
    void AnnounceArrival();
    void Arrive(int unit);
    void WarnOf(HazardSlot slot, int type);
};

class EAXCop {
  public:
    enum Type_offroad_moment_id {
        Type_offroad_moment_id_hospital    = 8,
        Type_offroad_moment_id_junkyard    = 256,
        Type_offroad_moment_id_subway      = 16384,
        Type_offroad_moment_id_hotel       = 32768,
        Type_offroad_moment_id_museum      = 65536,
        Type_offroad_moment_id_hydro_plant = 262144
    };

    static void InstallPatches();

    void Say(DWORD slot) {
        ((VoidMethod)VirtualFunction(this, slot))(this);
    }

    void Offroad(unsigned int id, bool subsequent) {
        ((OffroadMethod)VirtualFunction(this, EAX_COP_OFFROAD))(this, id, subsequent);
    }

    void SpotterReply();
    void Spotted();
    void CollisionBailout();
    bool OpensFor911();
    void ReportOffroad(unsigned int id, bool subsequent);
};

class AIPursuit {
  public:
    static bool HookIncNumCopsDestroyed();
    static void __cdecl IncNumCopsDestroyedEntry(Detour::Registers* registers);
};

class AICopManager {
  public:
    static bool HookApplyBreakerZones();
    static bool HookRecycle();
    static bool HookGetAvailableCopVehicleByName();
    static void __cdecl ApplyBreakerZonesEntry(Detour::Registers* registers);
    static void __cdecl RecycleEntry(Detour::Registers* registers);
    static void __cdecl GetAvailableCopVehicleByNameEntry(Detour::Registers* registers);
    static bool IsHelicopterPvehicle(const char* name);

    void UpdateBreakerFire();
    bool HasVehicle(const IVehicle* vehicle) const;
    void UpdateHelicopterTopUp();
    void CountPursuitHelicopters(const IPursuit* ipursuit, int* inPursuit, int* chasing) const;
    int GetNumActiveCopHelicopters() const;
    IPursuit* GetPlayerPursuit() const;
    bool SpawnPursuitHelicopter(IPursuit* ipursuit);
    bool CanSpawn911Helicopter();
    IVehicle* Spawn911Helicopter(const UMath::Vector3& reported);
    IVehicle* GetAvailableCopVehicleByClass(DWORD vehicleClass, bool bValidOnesOnly);
    IVehicle* GetAvailableCopVehicleByName(const char* name);
    bool VehicleSpawningEnabled(bool isdespawn);
};

class SoundAI {
  public:
    enum MachineState {
        kNoMachine   = 0,
        kPursuitFlow = 1,
        kTerminal    = 999
    };

    static void InstallPatches();
    static bool HookSyncCarsToActors();
    static bool HookChopperLOS();
    static void __cdecl ChopperCheckEntry(Detour::Registers* registers);
    static void __cdecl ChopperLOSEntry(Detour::Registers* registers);

    static SoundAI* Get() {
        return *(SoundAI* const*)SOUND_AI;
    }

    int GetFocus() const {
        return *(const int*)((DWORD)this + SOUND_AI_FOCUS);
    }

    DWORD GetFlags() const {
        return *(const DWORD*)((DWORD)this + SOUND_AI_FLAGS);
    }

    int GetPlayerRoad() const {
        return *(const int*)((DWORD)this + SOUND_AI_PLAYER_ROAD);
    }

    bool PursuitActive() const {
        return *(const void* const*)((DWORD)this + SOUND_AI_PURSUIT) != NULL && *(const int*)((DWORD)this + SOUND_AI_PURSUIT_STATE) == 0;
    }

    bool InPursuit() const;
    bool CanAnnounceBackup() const;
    void Force911State();
    void UpdateReportRoad();
    EAXCop* FindClosestCop(bool enforceLOS, bool includeHeli);
    EAXCop* FindOffroadSpotter(bool enforceLOS, bool includeHeli);
};

class EAXDispatch {
  public:
    void Report911(int pursuit_type);
};

class FEObject;

struct FEColor {
    int b, g, r, a;
};

struct FEMinList {
    void AddNode(FEObject* after, FEObject* node) {
        ((AddNodeMethod)FEMINLIST_ADD_NODE)(this, after, node);
    }

    void*     mVTable;
    unsigned  NumElements;
    FEObject* Head;
    FEObject* Tail;
};

class FEObject {
  public:
    enum FEObjType {
        FE_Group      = 5,
        FE_MultiImage = 12
    };

    FEObject* Clone(bool bReference) {
        return (FEObject*)((CloneMethod)VirtualFunction(this, FEOBJECT_CLONE))(this, bReference);
    }

    void*          mVTable;
    FEObject*      Next;
    FEObject*      Prev;
    DWORD          GUID;
    DWORD          NameHash;
    char*          pName;
    int            Type;
    DWORD          Flags;
    WORD           RenderContext;
    WORD           ResourceIndex;
    DWORD          Handle;
    DWORD          UserParam;
    unsigned char* pData;
    DWORD          DataSize;
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
    DWORD ImageFlags;
    DWORD hTexture[3];
    DWORD TextureFlags[3];
};

class FEPackage {
  public:
    unsigned char mPackage[0x44];
    FEMinList     Objects;
};

struct bPList {
    void AddTail(FEObject* object) {
        ((PointerMethod)BPLIST_ADD_TAIL)(this, object);
    }

    void* HeadNode[2];
};

class cFEng {
  public:
    static cFEng* Get() {
        return *(cFEng* const*)CFENG_INSTANCE;
    }

    FEPackage* FindPackage(const char* name) {
        return (FEPackage*)((FindPackageMethod)CFENG_FIND_PACKAGE)(this, name);
    }
};

class CSTATE_Base {
  public:
    void Setup(int sfxFlags) {
        ((IntMethod)VirtualFunction(this, CSTATE_BASE_SETUP))(this, sfxFlags);
    }
};

class CSTATEMGR_Helicopter {
  public:
    static void InstallPatches();

    void EnterWorld(int esgm);

    CSTATE_Base* CreateState(int stateInstType, int sfxFlags) {
        return (CSTATE_Base*)((CreateStateMethod)CSTATEMGR_HELICOPTER_CREATE_STATE)(this, stateInstType, sfxFlags);
    }

    unsigned char mStateMgrBase[0x14];
    int           m_CurNumStates;
    bool          bIsInitialized;
};

struct HSIMSERVICE__;

class SoundHeli {
  public:
    static void InstallPatches();

    bool OnService(HSIMSERVICE__* hCon, void* pkt);

    unsigned char mBehavior[0x5C];
    IVehicle*     mVehicle;
};

struct HSIMTASK__;

class DrawHeli {
  public:
    static void InstallPatches();

    bool OnTask(HSIMTASK__* hTask, float dT);

    unsigned char mBehavior[0x40];
    IVehicle*     mVehicle;
    unsigned char mDrawVehicle[0x3C];
    unsigned char mEffect[0x4C];
    bool          mEffectPaused;
    unsigned char mWash[0x1F];
    HSIMTASK__*   mWashTask;
};

class CameraMover {
  public:
    static void InstallPatches();

    void ChopperNoise(bMatrix4* world_to_camera, float f_scale, bool useWorldTimer);
};

class Camera {
  public:
    struct Settings {
        Settings();

        bool DebugWatchHeli;
    };

    static Settings sSettings;

    void SetCameraMatrix(const bMatrix4* world_to_camera, float dT);
    void WatchHeli(const bMatrix4* world_to_camera, float dT);
};

class Minimap {
  public:
    static void InstallPatches();

    void UpdateCopElements(IVehicle* ivehicle);
    void UpdateHelicopterElements(IVehicle* ivehicle);
    void UpdateHelicopterElement(IVehicle* helicopter, FEObject* heliArt, FEObject* lineOfSiteArt, bool searching);
    void FadeLineOfSiteArt(const IVehicle* helicopter, FEObject* lineOfSiteArt);
    void UpdateElementArt(bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse);
    bool ShowsPursuitCops(IVehicle* ivehicle) const;

    unsigned char mHudElement[0x8];
    bPList        Objects;
    const char*   pPackageName;
    unsigned char mMinimap[0xB4];
    FEObject*     mHeliElementArt;
    FEObject*     mHeliLineOfSiteArt;
};

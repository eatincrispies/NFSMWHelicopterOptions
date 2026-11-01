#include <math.h>
#include "AIVehicleHelicopter.hpp"
#include "AIActionHeliPursuit.hpp"
#include "HeliSheet.hpp"
#include "SimpleChopper.hpp"

namespace {

    #define HELI_VEHICLE                           0x0090D61C
    #define ON_DRIVING                             0x00417A20
    #define AVOID_CAMERA                           0x00417790
    #define CAN_SEE_TARGET_LOS_DISTANCE            0x00417146
    #define REMOVE_VEHICLE_SPAWN_HELI_TIMER        0x0042B6D9
    #define HELI_VISUAL_SPHERE                     0x008EB30C
    #define GAME_HELI_VISUAL_SPHERE                75
    #define GAME_SPAWN_DISTANCE                    250
    #define FRONT_FACING                           0
    #define ANY_FACING                             (-2)
    #define NO_BRAKE_RATIO                         10000

    #define EAX_AIR_SUPPORT_VTABLE                 0x008B2278
    #define EAX_CHARACTER_SET_HANDLE               0x007001D0
    #define SET_HANDLE_ARGUMENT                    0x08
    #define ADD_NEW_HELI_VTABLE_STORE              0x0070DAD6

    #define COLLECTIONS_FIND                       0x00681340
    #define COP_HELI_COLLECTION                    0x47C914A3
    #define BOUNDS_PRIM_VS_WORLD                   (1 << 1)
    #define BOUNDS_PRIM_VS_OBJECTS                 (1 << 2)
    #define BOUNDS_PRIM_VS_GROUND                  (1 << 3)
    #define BOUNDS_BOX                             (1 << 6)
    #define QUATERNION_ONE                         32767
    #define HELI_COLLISION_BOUNDS                  3

    #define APPLY_BREAKER_ZONES                    0x0042E930
    #define UPDATE_PATROLS_RECYCLE                 0x0043182E
    #define UPDATE_PATROLS_SHOULD_RESPAWN          (0x04 + 0x12)
    #define SPAWN_PURSUIT_HELICOPTER               0x004269A0
    #define COP_MANAGER_ACTIVE_HELICOPTERS         0x09C
    #define COP_MANAGER_IVEHICLE_LIST              0x0C4
    #define COP_MANAGER_IPURSUIT_LIST              0x128
    #define PURSUIT_LIST_NEXT                      0x0
    #define PURSUIT_LIST_VALUE                     0x8
    #define MOST_PURSUITS                          32
    #define TOP_UP_SECONDS                         5

    #define ATTRIB_STRING_TO_KEY                   0x00454640
    #define EFFECT_UPDATE                          0x00744A50
    #define EFFECT_LOCAL_MATRIX                    0x10
    #define EFFECT_EMITTER_GROUP                   0x50
    #define EFFECT_SIZE                            0x60
    #define BREAKER_BURST_SECONDS                  6
    #define LONGEST_EFFECT_STEP_MS                 100
    #define FULL_INTENSITY                         1
    #define NOT_BURSTING                           (-1)

    #define CSTATEMGR_HELICOPTER_ENTER_WORLD       0x004E60F0
    #define CSTATEMGR_HELICOPTER_VTABLE_ENTER      0x00898708
    #define HELICOPTER_STATE_INST_TYPE             0
    #define HELICOPTER_SFX_FLAGS                   1

    #define SOUND_HELI_ON_SERVICE                  0x00694980
    #define SOUND_HELI_VTABLE_ON_SERVICE           0x008ABA68

    #define AI_PURSUIT_INC_NUM_COPS_DESTROYED      0x00418F30
    #define AI_PURSUIT_VTABLE_INC_NUM_COPS         0x0089280C
    #define AI_PURSUIT_RECENT_REP_POINTS           0x0F4
    #define AI_PURSUIT_RECENT_COP_TYPE             0x0F8
    #define AI_PURSUIT_COPS_DESTROYED              0x13C
    #define PURSUIT_BOARD_NO_COP_MESSAGE           0x00595C2C
    #define PURSUIT_BOARD_SHOW_COP_MESSAGE         0x00595C43
    #define PURSUIT_BOARD_SKIP_COP_MESSAGE         0x00595CB3
    #define MOST_MESSAGE_TEXT                      128

    #define CAMERA_MOVER_CHOPPER_NOISE             0x00470A60
    #define CUBIC_CAMERA_CHOPPER_NOISE_CALL        0x0047DD7D

    #define MINIMAP_UPDATE_COP_ELEMENTS            0x00579E00
    #define MINIMAP_UPDATE_ELEMENT_ART             0x005675D0
    #define GET_VEHICLE_VECTORS                    0x00579840
    #define FENG_SET_INVISIBLE                     0x00514C70
    #define FENG_IS_SCRIPT_SET                     0x00514DA0
    #define FENG_SET_SCRIPT                        0x00514D10
    #define MINIMAP_SHOW_NON_PURSUIT_COPS          0x0091CF00
    #define FEHASH_INIT                            0x001744B3
    #define FEHASH_TRACKING                        0x55CAFA92
    #define TRACKING_MS                            250
    #define EXTRA_HELICOPTERS                      (MOST_HELICOPTERS - 1)
    #define DEEPEST_ART                            16
    #define MOST_COPIES_MADE                       (EXTRA_HELICOPTERS * 2)

    #define NEW_FUEL_MARGIN                        1
    #define APART_DISTANCE                         14
    #define APART_HEIGHT                           6
    #define APART_STIFFNESS                        1
    #define CLOSEST_APART_MM                       10
    #define MOST_COP_VEHICLES                      64
    #define MOST_CALL_SIGNS                        (MOST_HELICOPTERS * 2)

    typedef DWORD       (__cdecl* StringToKeyFunction)(const char* name);
    typedef void        (__thiscall* EffectUpdateMethod)(void* effect, const void* world, DWORD key, float dT, float intensity, const void* extra);
    typedef const void* (__thiscall* CollectionFindMethod)(void* collections, DWORD name);
    typedef bool        (__thiscall* SpawnPursuitHelicopterMethod)(void* manager, IPursuit* ipursuit);
    typedef void        (__thiscall* UpdateElementArtMethod)(void* minimap, bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse);
    typedef void        (__cdecl* GetVehicleVectorsFunction)(bVector2* pos, bVector2* dir, ISimable* isimable);
    typedef void        (__cdecl* SetInvisibleFunction)(FEObject* object);
    typedef bool        (__cdecl* IsScriptSetFunction)(FEObject* object, DWORD script);
    typedef void        (__cdecl* SetScriptFunction)(FEObject* object, DWORD script, bool force);
    typedef bool        (__thiscall* ServiceMethod)(void* behavior, HSIMSERVICE__* hCon, void* pkt);

    const BYTE kOnDrivingCode[]           = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
    const BYTE kCanSeeTargetCode[]        = { 0x89, 0x54, 0x24, 0x24, 0xD9, 0x44, 0x24, 0x24 };
    const BYTE kRespawnTimerCode[]        = { 0x89, 0x8F, 0xE0, 0x00, 0x00, 0x00 };
    const BYTE kApplyBreakerZonesCode[]   = { 0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9 };
    const BYTE kRecycleCode[]             = { 0x8A, 0x44, 0x24, 0x13, 0x84, 0xC0 };
    const BYTE kSetHandleCode[]           = { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x10 };
    const BYTE kAddNewHeliVTableCode[]    = { 0xC7, 0x06, 0x78, 0x22, 0x8B, 0x00 };

    const Patch::CallSite     kOnDrivingAvoidCamera                 = { 0x00417B02, AVOID_CAMERA };
    const Patch::FloatOperand kOnDrivingStoppingRatio               = { 0x00417C88, { 0xD8, 0x0D }, 0x008910FC };
    const Patch::FloatOperand kOnDrivingBrakeSpeedScale             = { 0x00417CA7, { 0xD8, 0x0D }, 0x00891054 };
    const Patch::FloatOperand kCanSeeTargetFacing                   = { 0x004171FA, { 0xD8, 0x1D }, 0x00890968 };
    const Patch::FloatPush    kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABF, GAME_SPAWN_DISTANCE };
    const Patch::FloatOperand kCopManagerChopperDespawn             = { 0x0043180E, { 0xD9, 0x05 }, 0x00891988 };
    const Patch::ByteStore    kHidingSpotHeliStore                  = { 0x0042995C, { 0xC6, 0x86, 0x85, 0x07, 0x00, 0x00 }, 1 };
    const Patch::CallSite     kPVehicleConstructFind                = { 0x00689A09, COLLECTIONS_FIND };
    const Patch::CallSite     kCubicCameraChopperNoise              = { CUBIC_CAMERA_CHOPPER_NOISE_CALL, CAMERA_MOVER_CHOPPER_NOISE };
    const Patch::JumpSite     kPursuitBoardNoCopMessage             = { PURSUIT_BOARD_NO_COP_MESSAGE, { 0x0F, 0x85 }, PURSUIT_BOARD_SKIP_COP_MESSAGE };
    const Patch::CallSite     kUpdateUpdateCopElements              = { 0x0059DCFD, MINIMAP_UPDATE_COP_ELEMENTS };
    const Patch::IndirectCall kUpdateCopElementsIsActive            = { 0x00579E94, { 0xFF, 0x90, 0x88, 0x00, 0x00, 0x00 } };

    const char* const kBreakerBurstGroup  = "fxex_gasstation";
    const char* const kBreakerFlamesGroup = "michael_bay";
    const char* const kBreakerSmallFlames = "fxfire_sm1";
    const char* const kBreakerFlareUps    = "fxex_large1";
    const char* const kBreakerSmokeGroup  = "fxcar_cop_death1";

    C_ASSERT(offsetof(AIVehicleHelicopter, mIOwner) == 0x34);
    C_ASSERT(offsetof(AIVehicleHelicopter, mDriveSpeed) == 0x84);
    C_ASSERT(offsetof(AIVehicleHelicopter, mDest) == 0x88);
    C_ASSERT(offsetof(AIVehicleHelicopter, mIAIHelicopter) == 0x7A4);
    C_ASSERT(offsetof(AIVehicleHelicopter, mDestinationVelocity) == 0x7AC);
    C_ASSERT(offsetof(AIVehicleHelicopter, mLookAtPosition) == 0x7B8);
    C_ASSERT(offsetof(AIVehicleHelicopter, mLastPlaceHeliSawPerp) == 0x7C4);
    C_ASSERT(offsetof(AIVehicleHelicopter, mHeight) == 0x7D0);
    C_ASSERT(offsetof(AIVehicleHelicopter, mStrafeToDest) == 0x7D4);
    C_ASSERT(offsetof(AIVehicleHelicopter, mPerpHiddenFromMe) == 0x7D5);
    C_ASSERT(offsetof(AIVehicleHelicopter, mHeliFuelTimeRemaining) == 0x7D8);
    C_ASSERT(offsetof(AIVehicleHelicopter, mShadowScale) == 0x7DC);
    C_ASSERT(offsetof(AIVehicleHelicopter, mDustStormIntensity) == 0x7E0);
    C_ASSERT(offsetof(AIVehicleHelicopter, mHeliSheetCoord) == 0x7F0);
    C_ASSERT(offsetof(AIVehicleHelicopter, mISimpleChopper) == 0x8B0);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fFlags) == 0xE);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fHalfDimensions) == 0x10);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fNumChildren) == 0x16);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fChildIndex) == 0x1E);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fRadius) == 0x20);
    C_ASSERT(offsetof(CollisionGeometry::Bounds, fCollection) == 0x2C);
    C_ASSERT(sizeof(CollisionGeometry::Bounds) == 0x30);
    C_ASSERT(sizeof(CollisionGeometry::Collection) == 0x10);
    C_ASSERT(offsetof(CSTATEMGR_Helicopter, m_CurNumStates) == 0x14);
    C_ASSERT(offsetof(CSTATEMGR_Helicopter, bIsInitialized) == 0x18);
    C_ASSERT(offsetof(FEMinList, Head) == 0x8);
    C_ASSERT(sizeof(FEMinList) == 0x10);
    C_ASSERT(offsetof(FEObject, Type) == 0x18);
    C_ASSERT(offsetof(FEObject, Flags) == 0x1C);
    C_ASSERT(offsetof(FEObject, pData) == 0x2C);
    C_ASSERT(offsetof(FEObject, Responses) == 0x34);
    C_ASSERT(offsetof(FEObject, Scripts) == 0x44);
    C_ASSERT(offsetof(FEObject, Cached) == 0x58);
    C_ASSERT(sizeof(FEObject) == 0x5C);
    C_ASSERT(offsetof(FEGroup, Children) == 0x5C);
    C_ASSERT(sizeof(FEGroup) == 0x6C);
    C_ASSERT(offsetof(FEMultiImage, ImageFlags) == 0x5C);
    C_ASSERT(offsetof(FEMultiImage, hTexture) == 0x60);
    C_ASSERT(sizeof(FEMultiImage) == 0x78);
    C_ASSERT(offsetof(FEPackage, Objects) == 0x44);
    C_ASSERT(sizeof(bPList) == 0x8);
    C_ASSERT(offsetof(Minimap, Objects) == 0x8);
    C_ASSERT(offsetof(Minimap, pPackageName) == 0x10);
    C_ASSERT(offsetof(Minimap, mHeliElementArt) == 0xC8);
    C_ASSERT(offsetof(Minimap, mHeliLineOfSiteArt) == 0xCC);
    C_ASSERT(offsetof(SoundHeli, mVehicle) == 0x5C);

    struct Brake {
        float stoppingRatio;
        float speedScale;
    };

    struct HelicopterState {
        HelicopterState() : lastOwner(NULL), lastFuel(0) {}

        ISimable*        lastOwner;
        float            lastFuel;
        HeliSheet::State sheet;
    };

    struct CallSign {
        HSIMABLE__* handle;
        int         unit;
    };

    struct __declspec(align(16)) HeliCollision {
        CollisionGeometry::Collection header;
        CollisionGeometry::Bounds     bounds[HELI_COLLISION_BOUNDS];
    };

    const bMatrix4 kIdentityMatrix = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };

    class EmitterEffect {
      public:
        EmitterEffect() : mReady(false), mGroup(NULL), mKey(0) {
            memset(mEffect, 0, sizeof(mEffect));
        }

        bool Play(const char* group, const bMatrix4& world, float dT) {
            if (!mReady) {
                memcpy(mEffect + EFFECT_LOCAL_MATRIX, &kIdentityMatrix, sizeof(kIdentityMatrix));
                mReady = true;
            }
            if (group != mGroup) {
                mGroup = group;
                mKey = ((StringToKeyFunction)ATTRIB_STRING_TO_KEY)(group);
            }
            ((EffectUpdateMethod)EFFECT_UPDATE)(mEffect, &world, mKey, dT, FULL_INTENSITY, NULL);
            return EmitterGroup() != NULL;
        }

        void Remove() {
            if (!mReady) return;
            mGroup = NULL;
            ((EffectUpdateMethod)EFFECT_UPDATE)(mEffect, NULL, 0, 0, 0, NULL);
        }

        const void* EmitterGroup() const {
            const void* group = NULL;
            memcpy(&group, mEffect + EFFECT_EMITTER_GROUP, sizeof(group));
            return group;
        }

      private:
        __declspec(align(16)) unsigned char mEffect[EFFECT_SIZE];
        bool        mReady;
        const char* mGroup;
        DWORD       mKey;
    };

    class BreakerFire {
      public:
        BreakerFire() : mVehicle(NULL), mWasDestroyed(false), mBurning(false), mSmallFlames(false), mBurnStart(0), mBurstEnd(NOT_BURSTING), mLastBurnTime(0) {}

        const IVehicle* Vehicle() const {
            return mVehicle;
        }

        bool IsDown() const {
            return mBurning;
        }

        float DownSince() const {
            return mBurnStart;
        }

        void Watch(const IVehicle* vehicle) {
            mVehicle = vehicle;
            mWasDestroyed = vehicle->IsDestroyed();
        }

        void Forget() {
            PutOut();
            mVehicle = NULL;
            mWasDestroyed = false;
        }

        void Update() {
            if (!mVehicle->IsActive()) {
                Forget();
                return;
            }
            bool destroyed = mVehicle->IsDestroyed();
            if (destroyed && !mWasDestroyed) Ignite();
            if (!destroyed && mBurning) PutOut();
            mWasDestroyed = destroyed;
            if (mBurning) Burn();
        }

      private:
        void Ignite() {
            mBurning = true;
            mSmallFlames = false;
            mBurnStart = Sim::GetTime();
            mLastBurnTime = mBurnStart;
            mBurstEnd = mBurnStart + BREAKER_BURST_SECONDS;
        }

        void PutOut() {
            mBurst.Remove();
            mFlames.Remove();
            mFlareUps.Remove();
            mSmoke.Remove();
            mBurning = false;
        }

        void Burn() {
            float now = Sim::GetTime();
            if (now < mBurnStart) {
                PutOut();
                return;
            }
            float dT = Clamp(now - mLastBurnTime, 0, Seconds(LONGEST_EFFECT_STEP_MS));
            mLastBurnTime = now;

            const UMath::Vector3* position = mVehicle->GetPosition();
            if (position == NULL || !UMath::IsFinite(*position)) return;
            bMatrix4 world = kIdentityMatrix;
            world.m[12] = position->z;
            world.m[13] = -position->x;
            world.m[14] = position->y;

            if (now < mBurstEnd)
                mBurst.Play(kBreakerBurstGroup, world, dT);
            else
                mBurst.Remove();

            if (!mSmallFlames && !mFlames.Play(kBreakerFlamesGroup, world, dT)) mSmallFlames = true;
            if (mSmallFlames) {
                mFlames.Play(kBreakerSmallFlames, world, dT);
                mFlareUps.Play(kBreakerFlareUps, world, dT);
            }
            mSmoke.Play(kBreakerSmokeGroup, world, dT);
        }

        EmitterEffect   mBurst;
        EmitterEffect   mFlames;
        EmitterEffect   mFlareUps;
        EmitterEffect   mSmoke;
        const IVehicle* mVehicle;
        bool            mWasDestroyed;
        bool            mBurning;
        bool            mSmallFlames;
        float           mBurnStart;
        float           mBurstEnd;
        float           mLastBurnTime;
    };

    struct HeliKill {
        HeliKill() : pursuit(NULL), time(0), type(0), rep(0) {}

        const void* pursuit;
        float       time;
        DWORD       type;
        int         rep;
    };

    class ArtCopies {
      public:
        ArtCopies() : mOriginal(NULL), mCount(0), mMade(0) {
            for (int i = 0; i < EXTRA_HELICOPTERS; ++i) mCopies[i] = NULL;
        }

        int Check(const FEObject* original);
        void Grow(FEPackage* package, bPList& objects, FEObject* original, int wanted);
        void HideFrom(int index);

        int Count() const {
            return mCount;
        }

        FEObject* Get(int index) const {
            return mCopies[index];
        }

      private:
        const FEObject* mOriginal;
        FEObject*       mCopies[EXTRA_HELICOPTERS];
        int             mCount;
        int             mMade;
    };

    Brake                                                  gBrake;
    bool                                                   gSpawnPatched      = false;
    float                                                  gSpawnDistance     = 0;
    float                                                  gSightFacing       = FRONT_FACING;
    bool                                                   gSightPatched      = false;
    float                                                  gVisualSphere      = 0;
    bool                                                   gHidingPatched     = false;
    bool                                                   gHidingSpots       = true;
    float                                                  gDespawnDistance   = 0;
    PerHelicopter<HelicopterState, MOST_HELICOPTERS * 2>   gHelicopters;
    HelicopterState                                        gUnknownHelicopter;
    HelicopterState*                                       gHeli              = &gUnknownHelicopter;
    CallSign                                              gCallSigns[MOST_CALL_SIGNS];
    int                                                    gNextCallSign      = EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
    HeliCollision                                          gHeliCollision;
    BreakerFire                                            gBreakerFires[MOST_HELICOPTERS];
    float                                                  gTopUpWaitStart    = 0;
    const IVehicle*                                        gMinimapHelicopter = NULL;
    const FEObject*                                        gNoCopiesFor       = NULL;
    ArtCopies                                              gHeliArtCopies;
    ArtCopies                                              gHeliLineOfSiteArtCopies;
    char                                                   gCopHeliMessage[MOST_MESSAGE_TEXT];
    HeliKill                                               gHeliKill;
    DWORD                                                  gShowCopMessage    = PURSUIT_BOARD_SHOW_COP_MESSAGE;
    DWORD                                                  gSkipCopMessage    = PURSUIT_BOARD_SKIP_COP_MESSAGE;

    HSIMABLE__*                         OwnerHandle(const IVehicle* vehicle);
    int                                  FlyingHelicopters(HSIMABLE__** handles);
    CallSign*                            FindCallSign(const HSIMABLE__* handle);
    int                                  OtherCallSign(int unit);
    int                                  NextCallSign();
    void                                 TrackCallSigns();
    CollisionGeometry::Bounds            MakeBox(const CollisionGeometry::Bounds& root, const short* offsetMm, const short* halfMm, WORD flags);
    const CollisionGeometry::Collection* HeliCollisionFrom(const CollisionGeometry::Collection* original);
    float                                TimeDown(const IVehicle* vehicle);
    const char* __cdecl                  CopMessage(DWORD copType);
    void                                 NoCopMessage();
    bool                                 MinimapShowNonPursuitCops();
    FEMinList*                           FindParentList(FEMinList& list, const FEObject* object, int depth);
    void                                 CopyArtData(const FEObject* from, FEObject* to, int depth);
    void                                 RegisterArt(bPList& objects, FEObject* object, int depth);

}

AIVehicleHelicopter::Settings AIVehicleHelicopter::sSettings;

AIVehicleHelicopter::Settings::Settings()
    : StoppingRatio(1.55f),
      BrakeSpeedScale(0.4f),
      LineOfSight(0.0f),
      FuelTime(0.0f),
      SpawnDistance(250.0f),
      AllAroundSight(75.0f),
      FrontSightOnly(true),
      RespawnDelay(0.0f),
      HidingSpots(true),
      DespawnDistance(600.0f),
      Helicopters(1.0f),
      Falcon2(false),
      RecycleTime(0.0f),
      RecycleDistance(0.0f) {
}

void AIVehicleHelicopter::InstallPatches() {
    Refresh();

    Patch::Begin();
    Patch::RedirectFloat(kOnDrivingStoppingRatio, &gBrake.stoppingRatio);
    Patch::RedirectFloat(kOnDrivingBrakeSpeedScale, &gBrake.speedScale);
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectCall(kOnDrivingAvoidCamera, MethodAddress(&AIVehicleHelicopter::AvoidCameraHook));
    Patch::Commit();

    Patch::Begin();
    Patch::PushFloat(kSpawnPursuitHelicopterIncNavPosition, sSettings.SpawnDistance);
    gSpawnPatched = Patch::Commit();
    gSpawnDistance = sSettings.SpawnDistance;

    Patch::Begin();
    Patch::DataFloat(HELI_VISUAL_SPHERE, GAME_HELI_VISUAL_SPHERE, sSettings.AllAroundSight);
    Patch::RedirectFloat(kCanSeeTargetFacing, &gSightFacing);
    gSightPatched = Patch::Commit();
    gVisualSphere = sSettings.AllAroundSight;

    Patch::Begin();
    Patch::RedirectFloat(kCopManagerChopperDespawn, &gDespawnDistance);
    Patch::Commit();

    Patch::Begin();
    Patch::StoreByte(kHidingSpotHeliStore, (BYTE)(sSettings.HidingSpots ? 1 : 0));
    gHidingPatched = Patch::Commit();
    gHidingSpots = sSettings.HidingSpots;
}

bool AIVehicleHelicopter::HookOnDriving() {
    return Detour::Install(ON_DRIVING, kOnDrivingCode, sizeof(kOnDrivingCode), &OnDrivingEntry);
}

bool AIVehicleHelicopter::HookCanSeeTarget() {
    return Detour::Install(CAN_SEE_TARGET_LOS_DISTANCE, kCanSeeTargetCode, sizeof(kCanSeeTargetCode), &CanSeeTargetEntry);
}

bool AIVehicleHelicopter::HookRespawnTimer() {
    return Detour::Install(REMOVE_VEHICLE_SPAWN_HELI_TIMER, kRespawnTimerCode, sizeof(kRespawnTimerCode), &RespawnTimerEntry);
}

void AIVehicleHelicopter::Refresh() {
    gBrake.stoppingRatio = sSettings.StoppingRatio;
    gBrake.speedScale = sSettings.BrakeSpeedScale;
    gSightFacing = (float)(sSettings.FrontSightOnly ? FRONT_FACING : ANY_FACING);
    gDespawnDistance = sSettings.DespawnDistance;
    if (gSightPatched && sSettings.AllAroundSight != gVisualSphere) {
        *(float*)HELI_VISUAL_SPHERE = sSettings.AllAroundSight;
        gVisualSphere = sSettings.AllAroundSight;
    }

    if (gHidingPatched && sSettings.HidingSpots != gHidingSpots) {
        if (Patch::RewriteStoredByte(kHidingSpotHeliStore, (BYTE)(gHidingSpots ? 1 : 0), (BYTE)(sSettings.HidingSpots ? 1 : 0)))
            gHidingSpots = sSettings.HidingSpots;
        else
            gHidingPatched = false;
    }

    if (!gSpawnPatched || sSettings.SpawnDistance == gSpawnDistance) return;
    if (!Patch::RewritePushedFloat(kSpawnPursuitHelicopterIncNavPosition, gSpawnDistance, sSettings.SpawnDistance)) {
        gSpawnPatched = false;
        return;
    }
    gSpawnDistance = sSettings.SpawnDistance;
}

int AIVehicleHelicopter::Helicopters() {
    return ClampInt((int)((double)sSettings.Helicopters * 2 + 1) / 2, 1, MOST_HELICOPTERS);
}

void __cdecl AIVehicleHelicopter::OnDrivingEntry(Detour::Registers* registers) {
    AIVehicleHelicopter* heli = (AIVehicleHelicopter*)registers->ecx;
    if (heli == NULL || heli->mIOwner == NULL) return;
    gHeli = &gHelicopters.Use(heli);
    heli->UpdateHelicopterOptions();
}

void __cdecl AIVehicleHelicopter::CanSeeTargetEntry(Detour::Registers* registers) {
    if (sSettings.LineOfSight <= 0) return;
    memcpy(&registers->edx, &sSettings.LineOfSight, sizeof(registers->edx));
}

void __cdecl AIVehicleHelicopter::RespawnTimerEntry(Detour::Registers* registers) {
    if (sSettings.RespawnDelay <= 0) return;
    memcpy(&registers->ecx, &sSettings.RespawnDelay, sizeof(registers->ecx));
}

void AIVehicleHelicopter::UpdateHelicopterOptions() {
    IRigidBody* rigidBody = GetRigidBody();
    if (rigidBody == NULL) return;
    UMath::Vector3 position = rigidBody->GetPosition();
    if (!UMath::IsFinite(position) || !Memory::IsFinite(mHeliFuelTimeRemaining)) return;

    AIPerpVehicle::UpdateHeat();

    bool newHelicopter = mIOwner != gHeli->lastOwner || mHeliFuelTimeRemaining > gHeli->lastFuel + NEW_FUEL_MARGIN;
    if (newHelicopter)
        BeginHelicopter();
    else
        gHeli->lastFuel = mHeliFuelTimeRemaining;

    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(rigidBody);
    bool striking = pursuit != NULL && pursuit->IsStriking();
    gBrake.stoppingRatio = striking ? (float)NO_BRAKE_RATIO : sSettings.StoppingRatio;
    gBrake.speedScale = striking ? (float)1 : sSettings.BrakeSpeedScale;
    HeliSheet::Update(gHeli->sheet, *this, position, rigidBody->GetLinearVelocity(), pursuit);
    if (!SimpleChopper::IsWreck(mISimpleChopper)) KeepApart(rigidBody);
    SimpleChopper::ApplySpeedCap(mISimpleChopper);
}

void AIVehicleHelicopter::BeginHelicopter() {
    gHeli->lastOwner = mIOwner;
    gHeli->sheet = HeliSheet::State();

    gHeli->lastFuel = mHeliFuelTimeRemaining;
    if (sSettings.FuelTime > 0) {
        mHeliFuelTimeRemaining = sSettings.FuelTime;
        gHeli->lastFuel = sSettings.FuelTime;
    }
}

void AIVehicleHelicopter::KeepApart(IRigidBody* rigidBody) const {
    const IVehicle::List& vehicles = IVehicle::AICops();
    if (vehicles.mBegin == NULL) return;
    UMath::Vector3 position = rigidBody->GetPosition();
    UMath::Vector3 velocity = rigidBody->GetLinearVelocity();
    bool pushed = false;
    for (size_t i = 0; i < vehicles.mSize && i < MOST_COP_VEHICLES; ++i) {
        const IVehicle* vehicle = vehicles.mBegin[i];
        if (vehicle == NULL || !vehicle->IsActive() || !vehicle->IsChopper() || vehicle->IsDestroyed()) continue;
        ISimable* simable = vehicle->GetSimable();
        IRigidBody* other = simable != NULL ? simable->GetRigidBody() : NULL;
        if (other == NULL || other == rigidBody) continue;

        UMath::Vector3 apart = position - other->GetPosition();
        float distance = sqrtf(apart.x * apart.x + apart.z * apart.z);
        if (fabsf(apart.y) >= APART_HEIGHT || distance >= APART_DISTANCE || !UMath::IsFinite(apart)) continue;

        bool lowerAddress = (DWORD)rigidBody < (DWORD)other;
        UMath::Vector3 away = distance > Metres(CLOSEST_APART_MM) ? UMath::Vector(apart.x / distance, 0, apart.z / distance)
                                                                   : UMath::Vector((float)(lowerAddress ? 1 : -1), 0, 0);
        const UMath::Vector3& otherVelocity = other->GetLinearVelocity();
        float separating = (velocity.x - otherVelocity.x) * away.x + (velocity.z - otherVelocity.z) * away.z;
        float wanted = (APART_DISTANCE - distance) * APART_STIFFNESS;
        if (separating >= wanted) continue;
        velocity = velocity + away * ((wanted - separating) / 2);
        pushed = true;
    }

    if (pushed) rigidBody->SetLinearVelocity(velocity);
}

void AIVehicleHelicopter::AvoidCameraHook(UMath::Vector3& dest) {
    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(GetRigidBody());
    if (pursuit != NULL && pursuit->IsStriking()) return;
    ((PointerMethod)AVOID_CAMERA)(this, &dest);
}

int IVehicle::ReadList(DWORD address, IVehicle** out, int most) {
    List list;
    memset(&list, 0, sizeof(list));
    if (!Memory::Read(address, &list, sizeof(list)) || list.mBegin == NULL || list.mSize == 0 || list.mSize > (size_t)most) return 0;
    int count = (int)list.mSize;
    for (int i = 0; i < count; ++i)
        if (!Memory::Read((DWORD)(list.mBegin + i), &out[i], sizeof(out[i]))) out[i] = NULL;
    return count;
}

bool IVehicle::IsActiveOnMinimap() const {
    return IsActive() && (this == gMinimapHelicopter || !IsChopper());
}

bool EAXAirSupport::HookSetHandle() {
    return Detour::Install(EAX_CHARACTER_SET_HANDLE, kSetHandleCode, sizeof(kSetHandleCode), &SetHandleEntry);
}

bool EAXAirSupport::HookAddNewHeli() {
    return Detour::Install(ADD_NEW_HELI_VTABLE_STORE, kAddNewHeliVTableCode, sizeof(kAddNewHeliVTableCode), &AddNewHeliEntry);
}

void __cdecl EAXAirSupport::SetHandleEntry(Detour::Registers* registers) {
    DWORD vtable = 0;
    HSIMABLE__* handle = NULL;
    if (registers->ecx == 0 || !Memory::Read(registers->ecx, &vtable, sizeof(vtable)) || vtable != EAX_AIR_SUPPORT_VTABLE
        || !Memory::Read(registers->esp + SET_HANDLE_ARGUMENT, &handle, sizeof(handle)))
        return;
    TrackCallSigns();
    ((EAXAirSupport*)registers->ecx)->UseCallSign(handle);
}

void __cdecl EAXAirSupport::AddNewHeliEntry(Detour::Registers* registers) {
    EAXAirSupport* heli = (EAXAirSupport*)registers->esi;
    TrackCallSigns();
    heli->UseCallSign(heli->GetHandle());
}

void EAXAirSupport::UpdateCallSigns() {
    TrackCallSigns();
    EAXAirSupport* heli = SoundAIHeli();
    if (heli != NULL) heli->UseCallSign(heli->GetHandle());
}

void EAXAirSupport::UseCallSign(HSIMABLE__* handle) {
    const CallSign* callSign = handle != NULL ? FindCallSign(handle) : NULL;
    if (!AIVehicleHelicopter::sSettings.Falcon2)
        SetUnitNumber(Type_speaker_call_sign_id_CallSign01);
    else if (callSign != NULL)
        SetUnitNumber(callSign->unit);
}

void CollisionGeometry::Collections::InstallPatches() {
    Patch::Begin();
    Patch::RedirectCall(kPVehicleConstructFind, MethodAddress(&Collections::VehicleFind));
    Patch::Commit();
}

const CollisionGeometry::Collection* CollisionGeometry::Collections::Find(DWORD name) {
    return (const Collection*)((CollectionFindMethod)COLLECTIONS_FIND)(this, name);
}

const CollisionGeometry::Collection* CollisionGeometry::Collections::VehicleFind(DWORD name) {
    const Collection* found = Find(name);
    if (found == NULL || name != COP_HELI_COLLECTION || found->fNameHash != COP_HELI_COLLECTION || found->fNumBounds != 1) return found;
    return HeliCollisionFrom(found);
}

bool AICopManager::HookApplyBreakerZones() {
    return Detour::Install(APPLY_BREAKER_ZONES, kApplyBreakerZonesCode, sizeof(kApplyBreakerZonesCode), &ApplyBreakerZonesEntry);
}

bool AICopManager::HookRecycle() {
    return Detour::Install(UPDATE_PATROLS_RECYCLE, kRecycleCode, sizeof(kRecycleCode), &RecycleEntry);
}

void __cdecl AICopManager::RecycleEntry(Detour::Registers* registers) {
    const AIVehicleHelicopter::Settings& settings = AIVehicleHelicopter::sSettings;
    const IVehicle* vehicle = (const IVehicle*)registers->esi;
    if (settings.RecycleTime <= 0 || vehicle == NULL || !vehicle->IsChopper() || !vehicle->IsDestroyed()) return;

    float down = TimeDown(vehicle);
    const UMath::Vector3* position = vehicle->GetPosition();
    IRigidBody* player = GetLocalPlayerRigidBody();
    if (down < 0 || position == NULL || player == NULL) return;

    BYTE recycle = (BYTE)(down >= settings.RecycleTime && UMath::Distancexz(*position, player->GetPosition()) >= settings.RecycleDistance ? 1 : 0);
    Memory::WriteData(registers->esp + UPDATE_PATROLS_SHOULD_RESPAWN, &recycle, sizeof(recycle));
}

void __cdecl AICopManager::ApplyBreakerZonesEntry(Detour::Registers* registers) {
    AICopManager* manager = (AICopManager*)registers->ecx;
    if (manager == NULL) return;
    manager->UpdateBreakerFire();
    manager->UpdateHelicopterTopUp();
    EAXAirSupport::UpdateCallSigns();
}

void AICopManager::UpdateBreakerFire() {
    for (int f = 0; f < MOST_HELICOPTERS; ++f)
        if (gBreakerFires[f].Vehicle() != NULL && !HasVehicle(gBreakerFires[f].Vehicle())) gBreakerFires[f].Forget();

    IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
    int count = IVehicle::ReadList((DWORD)this + COP_MANAGER_IVEHICLE_LIST, vehicles, MOST_COP_VEHICLES);
    for (int i = 0; i < count; ++i) {
        const IVehicle* vehicle = vehicles[i];
        if (vehicle == NULL) continue;
        bool watched = false;
        for (int f = 0; f < MOST_HELICOPTERS; ++f) watched = watched || gBreakerFires[f].Vehicle() == vehicle;
        if (watched || !vehicle->IsChopper() || !vehicle->IsActive()) continue;
        for (int f = 0; f < MOST_HELICOPTERS; ++f) {
            if (gBreakerFires[f].Vehicle() != NULL) continue;
            gBreakerFires[f].Watch(vehicle);
            break;
        }
    }

    for (int f = 0; f < MOST_HELICOPTERS; ++f)
        if (gBreakerFires[f].Vehicle() != NULL) gBreakerFires[f].Update();
}

bool AICopManager::HasVehicle(const IVehicle* vehicle) const {
    IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
    int count = IVehicle::ReadList((DWORD)this + COP_MANAGER_IVEHICLE_LIST, vehicles, MOST_COP_VEHICLES);
    for (int i = 0; i < count; ++i)
        if (vehicles[i] == vehicle) return true;
    return false;
}

void AICopManager::UpdateHelicopterTopUp() {
    float now = Sim::GetTime();
    int wanted = AIVehicleHelicopter::Helicopters();
    int active = GetNumActiveCopHelicopters();
    if (wanted <= 1 || active < 1 || active >= wanted || now < gTopUpWaitStart) {
        gTopUpWaitStart = now;
        return;
    }
    if (now - gTopUpWaitStart < TOP_UP_SECONDS) return;
    gTopUpWaitStart = now;

    IPursuit* ipursuit = GetPlayerPursuit();
    if (ipursuit == NULL) return;
    AIVehicleHelicopter** heliVehicle = (AIVehicleHelicopter**)HELI_VEHICLE;
    AIVehicleHelicopter* previous = *heliVehicle;
    *heliVehicle = NULL;
    if (!SpawnPursuitHelicopter(ipursuit)) *heliVehicle = previous;
}

int AICopManager::GetNumActiveCopHelicopters() const {
    int count = 0;
    Memory::Read((DWORD)this + COP_MANAGER_ACTIVE_HELICOPTERS, &count, sizeof(count));
    return count;
}

IPursuit* AICopManager::GetPlayerPursuit() const {
    DWORD head = 0;
    DWORD node = 0;
    if (!Memory::Read((DWORD)this + COP_MANAGER_IPURSUIT_LIST, &head, sizeof(head)) || head == 0 || !Memory::Read(head + PURSUIT_LIST_NEXT, &node, sizeof(node)))
        return NULL;
    for (int i = 0; node != 0 && node != head && i < MOST_PURSUITS; ++i) {
        IPursuit* ipursuit = NULL;
        if (!Memory::Read(node + PURSUIT_LIST_VALUE, &ipursuit, sizeof(ipursuit))) return NULL;
        if (ipursuit != NULL && ipursuit->IsPlayerPursuit()) return ipursuit;
        if (!Memory::Read(node + PURSUIT_LIST_NEXT, &node, sizeof(node))) return NULL;
    }
    return NULL;
}

bool AICopManager::SpawnPursuitHelicopter(IPursuit* ipursuit) {
    return ((SpawnPursuitHelicopterMethod)SPAWN_PURSUIT_HELICOPTER)(this, ipursuit);
}

void CSTATEMGR_Helicopter::InstallPatches() {
    Patch::Begin();
    Patch::RedirectVirtual(CSTATEMGR_HELICOPTER_VTABLE_ENTER, CSTATEMGR_HELICOPTER_ENTER_WORLD, MethodAddress(&CSTATEMGR_Helicopter::EnterWorld));
    Patch::Commit();
}

void CSTATEMGR_Helicopter::EnterWorld(int esgm) {
    ((IntMethod)CSTATEMGR_HELICOPTER_ENTER_WORLD)(this, esgm);
    while (m_CurNumStates < MOST_HELICOPTERS) {
        CSTATE_Base* state = CreateState(HELICOPTER_STATE_INST_TYPE, HELICOPTER_SFX_FLAGS);
        if (state == NULL) break;
        state->Setup(HELICOPTER_SFX_FLAGS);
    }
}

void SoundHeli::InstallPatches() {
    Patch::Begin();
    Patch::RedirectVirtual(SOUND_HELI_VTABLE_ON_SERVICE, SOUND_HELI_ON_SERVICE, MethodAddress(&SoundHeli::OnService));
    Patch::Commit();
}

bool SoundHeli::OnService(HSIMSERVICE__* hCon, void* pkt) {
    if (mVehicle != NULL && mVehicle->IsDestroyed()) return false;
    return ((ServiceMethod)SOUND_HELI_ON_SERVICE)(this, hCon, pkt);
}

void AIPursuit::InstallPatches() {
    Patch::Begin();
    Patch::RedirectVirtual(AI_PURSUIT_VTABLE_INC_NUM_COPS, AI_PURSUIT_INC_NUM_COPS_DESTROYED, MethodAddress(&AIPursuit::IncNumCopsDestroyed));
    Patch::Commit();
}

void AIPursuit::IncNumCopsDestroyed(IVehicle* ivehicle) {
    DWORD pursuit = (DWORD)this;
    int before = *(const int*)(pursuit + AI_PURSUIT_COPS_DESTROYED);
    ((PointerMethod)AI_PURSUIT_INC_NUM_COPS_DESTROYED)(this, ivehicle);
    if (gCopHeliMessage[0] == '\0' || *(const int*)(pursuit + AI_PURSUIT_COPS_DESTROYED) == before || !((const IPursuit*)this)->IsPlayerPursuit()) return;

    float now = Sim::GetTime();
    DWORD* type = (DWORD*)(pursuit + AI_PURSUIT_RECENT_COP_TYPE);
    int* rep = (int*)(pursuit + AI_PURSUIT_RECENT_REP_POINTS);
    if (ivehicle != NULL && ivehicle->IsChopper()) {
        gHeliKill.pursuit = this;
        gHeliKill.time = now;
        gHeliKill.type = *type;
        gHeliKill.rep = *rep;
    } else if (gHeliKill.pursuit == this && gHeliKill.time == now) {
        *type = gHeliKill.type;
        *rep = gHeliKill.rep;
    }
}

void PursuitBoard::InstallPatches() {
    Patch::Begin();
    Patch::RedirectJump(kPursuitBoardNoCopMessage, (const void*)&NoCopMessage);
    Patch::Commit();
}

void PursuitBoard::SetCopHeliMessage(const char* message) {
    strncpy_s(gCopHeliMessage, sizeof(gCopHeliMessage), message, _TRUNCATE);
}

void CameraMover::InstallPatches() {
    Patch::Begin();
    Patch::RedirectCall(kCubicCameraChopperNoise, MethodAddress(&CameraMover::ChopperNoise));
    Patch::Commit();
}

void CameraMover::ChopperNoise(bMatrix4*, float, bool) {
}

void Minimap::InstallPatches() {
    Patch::Begin();
    Patch::RedirectCall(kUpdateUpdateCopElements, MethodAddress(&Minimap::UpdateHelicopterElements));
    Patch::RedirectIndirectCall(kUpdateCopElementsIsActive, MethodAddress(&IVehicle::IsActiveOnMinimap));
    Patch::Commit();
}

void Minimap::UpdateCopElements(IVehicle* ivehicle) {
    ((PointerMethod)MINIMAP_UPDATE_COP_ELEMENTS)(this, ivehicle);
}

void Minimap::UpdateHelicopterElements(IVehicle* ivehicle) {
    IVehicle* helicopters[EXTRA_HELICOPTERS] = { NULL };
    int count = 0;
    gMinimapHelicopter = NULL;
    const IVehicle::List& vehicles = IVehicle::AICops();
    for (size_t i = 0; vehicles.mBegin != NULL && i < vehicles.mSize && i < MOST_COP_VEHICLES; ++i) {
        IVehicle* vehicle = vehicles.mBegin[i];
        if (vehicle == NULL || !vehicle->IsActive() || !vehicle->IsChopper()) continue;
        if (gMinimapHelicopter == NULL)
            gMinimapHelicopter = vehicle;
        else if (count < EXTRA_HELICOPTERS)
            helicopters[count++] = vehicle;
    }

    UpdateCopElements(ivehicle);
    if (mHeliElementArt == NULL || mHeliLineOfSiteArt == NULL) return;

    int artCopies = gHeliArtCopies.Check(mHeliElementArt);
    int sightCopies = gHeliLineOfSiteArtCopies.Check(mHeliLineOfSiteArt);
    int copies = sightCopies < artCopies ? sightCopies : artCopies;
    if (count > 0 && !ShowsPursuitCops(ivehicle)) count = 0;
    if (count > copies && mHeliElementArt != gNoCopiesFor) {
        cFEng* feng = cFEng::Get();
        FEPackage* package = feng != NULL && pPackageName != NULL ? feng->FindPackage(pPackageName) : NULL;
        gHeliArtCopies.Grow(package, Objects, mHeliElementArt, count);
        gHeliLineOfSiteArtCopies.Grow(package, Objects, mHeliLineOfSiteArt, count);
        copies = gHeliLineOfSiteArtCopies.Count() < gHeliArtCopies.Count() ? gHeliLineOfSiteArtCopies.Count() : gHeliArtCopies.Count();
        if (copies < count) gNoCopiesFor = mHeliElementArt;
    }

    int shown = copies < count ? copies : count;
    for (int i = 0; i < shown; ++i) UpdateHelicopterElement(helicopters[i], gHeliArtCopies.Get(i), gHeliLineOfSiteArtCopies.Get(i));
    gHeliArtCopies.HideFrom(shown);
    gHeliLineOfSiteArtCopies.HideFrom(shown);
}

void Minimap::UpdateHelicopterElement(IVehicle* helicopter, FEObject* heliArt, FEObject* lineOfSiteArt) {
    bVector2 targetPos = MakePoint(0, 0);
    bVector2 targetDir = MakePoint(1, 0);
    ((GetVehicleVectorsFunction)GET_VEHICLE_VECTORS)(&targetPos, &targetDir, helicopter->GetSimable());

    const IPursuitAI* ipursuitai = helicopter->GetPursuitAI();
    if (ipursuitai != NULL && (MinimapShowNonPursuitCops() || ipursuitai->WasWithinEngagementRadius())) {
        bool tracking = ipursuitai->GetPursuitTarget() != NULL && ipursuitai->GetTimeSinceTargetSeen() <= Seconds(TRACKING_MS);
        DWORD script = tracking ? FEHASH_TRACKING : FEHASH_INIT;
        if (!((IsScriptSetFunction)FENG_IS_SCRIPT_SET)(lineOfSiteArt, script)) ((SetScriptFunction)FENG_SET_SCRIPT)(lineOfSiteArt, script, true);
    }
    UpdateElementArt(&targetPos, &targetDir, heliArt, false);
    UpdateElementArt(&targetPos, &targetDir, lineOfSiteArt, false);
}

void Minimap::UpdateElementArt(bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse) {
    ((UpdateElementArtMethod)MINIMAP_UPDATE_ELEMENT_ART)(this, elementPos, elementDir, elementArt, pulse);
}

bool Minimap::ShowsPursuitCops(IVehicle* ivehicle) const {
    if (MinimapShowNonPursuitCops()) return true;
    IVehicleAI* ai = ivehicle != NULL ? ivehicle->GetAIVehiclePtr() : NULL;
    IPursuit* ipursuit = ai != NULL ? ai->GetPursuit() : NULL;
    return ipursuit != NULL && !ipursuit->IsPursuitBailed();
}

namespace {

    HSIMABLE__* OwnerHandle(const IVehicle* vehicle) {
        const ISimable* simable = vehicle->GetSimable();
        return simable != NULL ? simable->GetOwnerHandle() : NULL;
    }

    int FlyingHelicopters(HSIMABLE__** handles) {
        IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
        int count = IVehicle::ReadList(AI_COPS, vehicles, MOST_COP_VEHICLES);
        int flying = 0;
        for (int i = 0; i < count && flying < MOST_CALL_SIGNS; ++i) {
            const IVehicle* vehicle = vehicles[i];
            if (vehicle == NULL || !vehicle->IsChopper() || !vehicle->IsActive() || vehicle->IsDestroyed()) continue;
            HSIMABLE__* handle = OwnerHandle(vehicle);
            if (handle != NULL) handles[flying++] = handle;
        }
        return flying;
    }

    CallSign* FindCallSign(const HSIMABLE__* handle) {
        for (int i = 0; i < MOST_CALL_SIGNS; ++i)
            if (gCallSigns[i].handle == handle) return &gCallSigns[i];
        return NULL;
    }

    int OtherCallSign(int unit) {
        return unit == EAXAirSupport::Type_speaker_call_sign_id_CallSign01 ? EAXAirSupport::Type_speaker_call_sign_id_CallSign02
                                                                          : EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
    }

    int NextCallSign() {
        bool taken[EAXAirSupport::Type_speaker_call_sign_id_CallSign02 + 1] = { false };
        for (int i = 0; i < MOST_CALL_SIGNS; ++i)
            if (gCallSigns[i].handle != NULL) taken[gCallSigns[i].unit] = true;
        int unit = gNextCallSign;
        if (taken[unit] && !taken[OtherCallSign(unit)]) unit = OtherCallSign(unit);
        gNextCallSign = OtherCallSign(unit);
        return unit;
    }

    void TrackCallSigns() {
        HSIMABLE__* flying[MOST_CALL_SIGNS] = { NULL };
        int count = FlyingHelicopters(flying);
        for (int c = 0; c < MOST_CALL_SIGNS; ++c) {
            if (gCallSigns[c].handle == NULL) continue;
            bool stillFlying = false;
            for (int i = 0; i < count; ++i) stillFlying = stillFlying || flying[i] == gCallSigns[c].handle;
            if (!stillFlying) {
                gCallSigns[c].handle = NULL;
                gCallSigns[c].unit = 0;
            }
        }
        for (int i = 0; i < count; ++i) {
            if (FindCallSign(flying[i]) != NULL) continue;
            CallSign* slot = FindCallSign(NULL);
            if (slot == NULL) break;
            slot->unit = NextCallSign();
            slot->handle = flying[i];
        }
    }

    CollisionGeometry::Bounds MakeBox(const CollisionGeometry::Bounds& root, const short* offsetMm, const short* halfMm, WORD flags) {
        CollisionGeometry::Bounds box;
        memset(&box, 0, sizeof(box));
        box.fOrientation[3] = QUATERNION_ONE;
        short largest = 0;
        for (int i = 0; i < 3; ++i) {
            box.fPosition[i] = offsetMm[i];
            box.fHalfDimensions[i] = halfMm[i];
            if (halfMm[i] > largest) largest = halfMm[i];
        }
        box.fFlags = flags;
        box.fPCloudIndex = -1;
        box.fChildIndex = -1;
        box.fRadius = Metres(largest);
        box.fSurface = root.fSurface;
        box.fCollection = &gHeliCollision.header;
        return box;
    }

    const CollisionGeometry::Collection* HeliCollisionFrom(const CollisionGeometry::Collection* original) {
        const CollisionGeometry::Bounds* root = (const CollisionGeometry::Bounds*)(original + 1);
        if (root->fNumChildren != 0) return original;

        gHeliCollision.header = *original;
        gHeliCollision.header.fNumBounds = HELI_COLLISION_BOUNDS;
        gHeliCollision.bounds[0] = *root;
        gHeliCollision.bounds[0].fNumChildren = HELI_COLLISION_BOUNDS - 1;
        gHeliCollision.bounds[0].fChildIndex = 1;
        gHeliCollision.bounds[0].fCollection = &gHeliCollision.header;
        gHeliCollision.bounds[1] = MakeBox(*root, CollisionGeometry::kHeliCabinOffsetMm, CollisionGeometry::kHeliCabinHalfMm,
                                           BOUNDS_BOX | BOUNDS_PRIM_VS_WORLD | BOUNDS_PRIM_VS_OBJECTS | BOUNDS_PRIM_VS_GROUND);
        gHeliCollision.bounds[2] = MakeBox(*root, CollisionGeometry::kHeliTailOffsetMm, CollisionGeometry::kHeliTailHalfMm,
                                           BOUNDS_BOX | BOUNDS_PRIM_VS_WORLD | BOUNDS_PRIM_VS_OBJECTS);
        return &gHeliCollision.header;
    }

    const char* __cdecl CopMessage(DWORD copType) {
        return gCopHeliMessage[0] != '\0' && gHeliKill.type != 0 && copType == gHeliKill.type ? gCopHeliMessage : NULL;
    }

    __declspec(naked) void NoCopMessage() {
        __asm {
            push esi
            call CopMessage
            add  esp, 4
            test eax, eax
            jz   skip
            jmp  dword ptr [gShowCopMessage]
        skip:
            jmp  dword ptr [gSkipCopMessage]
        }
    }

    float TimeDown(const IVehicle* vehicle) {
        for (int f = 0; f < MOST_HELICOPTERS; ++f)
            if (gBreakerFires[f].Vehicle() == vehicle && gBreakerFires[f].IsDown()) return Sim::GetTime() - gBreakerFires[f].DownSince();
        return -1;
    }

    bool MinimapShowNonPursuitCops() {
        return *(const bool*)MINIMAP_SHOW_NON_PURSUIT_COPS;
    }

    FEMinList* FindParentList(FEMinList& list, const FEObject* object, int depth) {
        for (FEObject* child = list.Head; child != NULL; child = child->Next) {
            if (child == object) return &list;
            if (child->Type != FEObject::FE_Group || depth >= DEEPEST_ART) continue;
            FEMinList* found = FindParentList(((FEGroup*)child)->Children, object, depth + 1);
            if (found != NULL) return found;
        }
        return NULL;
    }

    void CopyArtData(const FEObject* from, FEObject* to, int depth) {
        to->Cached = NULL;
        to->NameHash = from->NameHash;
        if (from->pData != NULL && to->pData != NULL && from->DataSize == to->DataSize) memcpy(to->pData, from->pData, from->DataSize);
        if (from->Type == FEObject::FE_MultiImage && to->Type == FEObject::FE_MultiImage) {
            const FEMultiImage* image = (const FEMultiImage*)from;
            FEMultiImage* copy = (FEMultiImage*)to;
            memcpy(copy->hTexture, image->hTexture, sizeof(image->hTexture));
            memcpy(copy->TextureFlags, image->TextureFlags, sizeof(image->TextureFlags));
        }
        if (from->Type != FEObject::FE_Group || to->Type != FEObject::FE_Group || depth >= DEEPEST_ART) return;
        const FEObject* child = ((const FEGroup*)from)->Children.Head;
        FEObject* copy = ((FEGroup*)to)->Children.Head;
        for (; child != NULL && copy != NULL; child = child->Next, copy = copy->Next) CopyArtData(child, copy, depth + 1);
    }

    void RegisterArt(bPList& objects, FEObject* object, int depth) {
        if (object->Type != FEObject::FE_Group) {
            objects.AddTail(object);
            return;
        }
        if (depth >= DEEPEST_ART) return;
        for (FEObject* child = ((FEGroup*)object)->Children.Head; child != NULL; child = child->Next) RegisterArt(objects, child, depth + 1);
    }

    int ArtCopies::Check(const FEObject* original) {
        if (original != mOriginal) {
            mOriginal = original;
            mCount = 0;
            mMade = 0;
            return 0;
        }
        const FEObject* previous = original;
        int linked = 0;
        while (linked < mCount && previous->Next == mCopies[linked] && mCopies[linked]->NameHash == original->NameHash && mCopies[linked]->Type == original->Type)
            previous = mCopies[linked++];
        mCount = linked;
        return linked;
    }

    void ArtCopies::Grow(FEPackage* package, bPList& objects, FEObject* original, int wanted) {
        if (mCount >= wanted || package == NULL) return;
        FEMinList* parent = FindParentList(package->Objects, original, 0);
        if (parent == NULL) return;
        FEObject* previous = mCount > 0 ? mCopies[mCount - 1] : original;
        while (mCount < wanted) {
            if (mMade >= MOST_COPIES_MADE) return;
            ++mMade;
            FEObject* copy = original->Clone(false);
            if (copy == NULL) return;
            CopyArtData(original, copy, 0);
            parent->AddNode(previous, copy);
            RegisterArt(objects, copy, 0);
            mCopies[mCount++] = copy;
            previous = copy;
        }
    }

    void ArtCopies::HideFrom(int index) {
        for (int i = index; i < mCount; ++i) ((SetInvisibleFunction)FENG_SET_INVISIBLE)(mCopies[i]);
    }

}

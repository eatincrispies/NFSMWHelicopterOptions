#include <math.h>
#include "AIVehicleHelicopter.hpp"
#include "AIActionHeliPursuit.hpp"
#include "HeliSheet.hpp"
#include "SimpleChopper.hpp"

namespace {

    #define HELI_VEHICLE                           0x0090D61C
    #define UPDATE_FUEL                            0x00423510
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
    #define SCHEDULE_SPEECH_PART_II                0x00713B20
    #define SCHEDULE_SPEECH_DATA                   0x0C
    #define SCHEDULE_SPEECH_INTERFACE              0x10
    #define SCHEDULE_SPEECH_SPEAKER                0x18
    #define SETUP_ATTMPT_VEH_STP_ID                0x00901BEC
    #define ATTMPT_VEH_STP_PURSUIT_TYPE            0x04
    #define HELI_SPECIFIC_HELI_BAILOUT_ID          0x00901EDC
    #define HELI_BAILOUT_TYPE                      0x04
    #define EAX_AIR_SUPPORT_LOST_VISUAL            0x00717B70
    #define EAX_AIR_SUPPORT_VTABLE_LOST_VISUAL     0x008B234C
    #define SYNC_CARS_TO_ACTORS_CHOPPER_CHECK      0x007212EE
    #define SYNC_CARS_TO_ACTORS_CHOPPER_LOS        0x007213CF
    #define SOUND_AI_FIND_CLOSEST_COP              0x00708390
    #define SOUND_AI_911_STATE                     0x100
    #define MISC_SPEECH_GET_LOCATION               0x007018C0
    #define LOCATION_REGION_CITY                   8
    #define HAZARD_GAP_SECONDS                     30
    #define HAZARD_REPEAT_SECONDS                  120
    #define EAX_COP_BACKUP_ARRIVES                 0x007186E0
    #define ARRIVAL_DELAY_MS                       4000

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
    #define DESPAWN_LEAD                           75
    #define DESPAWN_SLOWEST_SPEED                  1
    #define SPAWN_PURSUIT_HELICOPTER               0x004269A0
    #define COP_MANAGER_ACTIVE_HELICOPTERS         0x09C
    #define COP_MANAGER_IVEHICLE_LIST              0x0C4
    #define COP_MANAGER_IPURSUIT_LIST              0x128
    #define PURSUIT_LIST_NEXT                      0x0
    #define PURSUIT_LIST_VALUE                     0x8
    #define MOST_PURSUITS                          32
    #define TOP_UP_SECONDS                         5
    #define TOP_UP_RETRY_MS                        1000

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

    #define DRAW_HELI_ON_TASK                      0x00693990
    #define DRAW_HELI_VTABLE_ON_TASK               0x008AD618

    #define CAMERA_MOVER_CHOPPER_NOISE             0x00470A60
    #define CUBIC_CAMERA_CHOPPER_NOISE_CALL        0x0047DD7D

    #define MINIMAP_UPDATE_COP_ELEMENTS            0x00579E00
    #define MINIMAP_UPDATE_ELEMENT_ART             0x005675D0
    #define GET_VEHICLE_VECTORS                    0x00579840
    #define FENG_SET_INVISIBLE                     0x00514C70
    #define FENG_IS_SCRIPT_SET                     0x00514DA0
    #define FENG_SET_SCRIPT                        0x00514D10
    #define FENG_GET_OBJECT_COLOR                  0x00515830
    #define FENG_SET_COLOR                         0x005157E0
    #define FECOLOR_TO_U32                         0x005B2D80
    #define COLOR_ALPHA_SHIFT                      24
    #define COLOR_RGB                              0x00FFFFFF
    #define CONE_FADE_MS                           1000
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

    #define AI_VEHICLE_PURSUIT_PURSUIT_REQUEST     0x00409890
    #define AI_VEHICLE_HELICOPTER_VTABLE_REQUEST   0x00891F8C
    #define GET_AVAILABLE_COP_VEHICLE_BY_CLASS     0x00426610
    #define GET_AVAILABLE_COP_VEHICLE_BY_NAME      0x0041ECD0
    #define GET_COP_VEHICLE_BY_NAME_ARGUMENT       0x08
    #define INC_NUM_COPS_DESTROYED_COMBO           0x00418FE4
    #define AI_PURSUIT_COP_DESTROYED_REP_POINTS    0x0F4
    #define ATTRIB_FIND_COLLECTION                 0x00455FD0
    #define ATTRIB_COLLECTION_GET_DATA             0x00454190
    #define PVEHICLE_CLASS_KEY                     0x4A97EC8F
    #define PVEHICLE_CLASS_ATTRIBUTE               0x0EF6DDF2
    #define PVEHICLE_CLASS_VALUE                   0x08
    #define VAULT_CHOPPER                          0xB80933AA
    #define VEHICLE_SPAWNING_ENABLED               0x00410E20
    #define COP_MANAGER_ICOPMGR                    0x054
    #define UTIL_GENERATE_MATRIX                   0x006F1220
    #define SOUND_AI_FORCE_911_STATE               0x0070AD40
    #define FORCE_911_STATE_CALL                   0x0071EB85
    #define EAX_DISPATCH_REPORT_911                0x00717560
    #define REPORT_911_CALL                        0x0071D6A1
    #define SOUND_AI_PURSUIT_SPECS                 0x190
    #define ATTRIB_INSTANCE_LAYOUT                 0x004
    #define PURSUITLEVELS_LIFETIME_911             0x006FEA20
    #define ICOPMGR_INSTANCE                       0x0092C67C
    #define ICOPMGR_DISABLE_COPS                   0x0090D5EC
    #define REPORT_FRESH_SECONDS                   30
    #define REPORT_SIGHT_MS                        250
    #define REPORT_POINT_RANGE                     30
    #define REPORT_RADIUS_GROWTH                   40
    #define REPORT_POINTS_PER_TURN                 8
    #define REPORT_HEIGHT_NEAR                     5
    #define REPORT_HEIGHT_FAR                      25
    #define REPORT_DRIVE_FLAGS                     7
    #define RADIANS_PER_TURN                       6.2831853f

    typedef void        (__thiscall* FloatMethod)(void* object, float value);
    typedef bool        (__cdecl* GetLocationFunction)(int road, int* region, int* location);
    typedef DWORD       (__cdecl* StringToKeyFunction)(const char* name);
    typedef void        (__thiscall* EffectUpdateMethod)(void* effect, const void* world, DWORD key, float dT, float intensity, const void* extra);
    typedef const void* (__thiscall* CollectionFindMethod)(void* collections, DWORD name);
    typedef bool        (__thiscall* SpawnPursuitHelicopterMethod)(void* manager, IPursuit* ipursuit);
    typedef void        (__thiscall* UpdateElementArtMethod)(void* minimap, bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse);
    typedef void        (__cdecl* GetVehicleVectorsFunction)(bVector2* pos, bVector2* dir, ISimable* isimable);
    typedef void        (__cdecl* SetInvisibleFunction)(FEObject* object);
    typedef bool        (__cdecl* IsScriptSetFunction)(FEObject* object, DWORD script);
    typedef void        (__cdecl* SetScriptFunction)(FEObject* object, DWORD script, bool force);
    typedef void*       (__cdecl* GetObjectColorFunction)(FEColor* result, FEObject* obj);
    typedef void        (__cdecl* SetColorFunction)(FEObject* object, DWORD color);
    typedef DWORD       (__thiscall* ColorValueMethod)(const FEColor* color);
    typedef bool        (__thiscall* ServiceMethod)(void* behavior, HSIMSERVICE__* hCon, void* pkt);
    typedef bool        (__thiscall* TaskMethod)(void* behavior, HSIMTASK__* hTask, float dT);
    typedef AITarget*   (__thiscall* PursuitRequestMethod)(void* ipursuitai);
    typedef IVehicle*   (__thiscall* GetAvailableCopVehicleMethod)(void* manager, DWORD vehicleClass, bool bValidOnesOnly);
    typedef IVehicle*   (__thiscall* GetCopVehicleByNameMethod)(void* manager, const char* name);
    typedef bool        (__thiscall* VehicleSpawningEnabledMethod)(void* icopmgr, bool isdespawn);
    typedef void*       (__cdecl* GenerateMatrixFunction)(UMath::Matrix4* result, const UMath::Vector3* fwd, const UMath::Vector3* up);
    typedef const float* (__thiscall* AttribFloatQuery)(const void* instance);
    typedef EAXCop*     (__thiscall* FindClosestCopMethod)(void* soundAI, bool enforceLOS, bool includeHeli);
    typedef const void* (__cdecl* FindCollectionFunction)(DWORD classKey, DWORD collectionKey);
    typedef const void* (__thiscall* CollectionGetDataMethod)(const void* collection, DWORD key, DWORD index);

    const BYTE kOnDrivingCode[]           = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
    const BYTE kCanSeeTargetCode[]        = { 0x89, 0x54, 0x24, 0x24, 0xD9, 0x44, 0x24, 0x24 };
    const BYTE kRespawnTimerCode[]        = { 0x89, 0x8F, 0xE0, 0x00, 0x00, 0x00 };
    const BYTE kApplyBreakerZonesCode[]   = { 0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF9 };
    const BYTE kRecycleCode[]             = { 0x8A, 0x44, 0x24, 0x13, 0x84, 0xC0 };
    const BYTE kSetHandleCode[]           = { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x10 };
    const BYTE kAddNewHeliVTableCode[]    = { 0xC7, 0x06, 0x78, 0x22, 0x8B, 0x00 };
    const BYTE kScheduleSpeechCode[]      = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    const BYTE kChopperCheckCode[]        = { 0x8B, 0x00, 0x3B, 0x05, 0xF4, 0xC4, 0x92, 0x00 };
    const BYTE kChopperLOSCode[]          = { 0x8B, 0x8D, 0xE0, 0x00, 0x00, 0x00 };
    const BYTE kRepPointsComboCode[]      = { 0x8B, 0xBE, 0xF4, 0x00, 0x00, 0x00 };
    const BYTE kCopVehicleByNameCode[]    = { 0x83, 0xEC, 0x60, 0x53, 0x55, 0x56 };
    const DWORD kNoVehicleClass           = 0;
    const DWORD kCutOffroadPlaces         = EAXCop::Type_offroad_moment_id_hospital | EAXCop::Type_offroad_moment_id_junkyard
                                          | EAXCop::Type_offroad_moment_id_subway | EAXCop::Type_offroad_moment_id_hotel
                                          | EAXCop::Type_offroad_moment_id_museum | EAXCop::Type_offroad_moment_id_hydro_plant;
    const BYTE kHighwayRoads[]            = { 1, 49, 64, 82 };
    const BYTE kInterchangeRoads[]        = { 2, 4, 5, 6, 65, 81, 84 };

    const Patch::CallSite     kUpdateUpdateFuel                     = { 0x0042AE37, UPDATE_FUEL };
    const Patch::CallSite     kOnDrivingAvoidCamera                 = { 0x00417B02, AVOID_CAMERA };
    const Patch::FloatOperand kOnDrivingStoppingRatio               = { 0x00417C88, { 0xD8, 0x0D }, 0x008910FC };
    const Patch::FloatOperand kOnDrivingBrakeSpeedScale             = { 0x00417CA7, { 0xD8, 0x0D }, 0x00891054 };
    const Patch::FloatOperand kCanSeeTargetFacing                   = { 0x004171FA, { 0xD8, 0x1D }, 0x00890968 };
    const Patch::FloatPush    kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABF, GAME_SPAWN_DISTANCE };
    const Patch::FloatOperand kCopManagerChopperDespawn             = { 0x0043180E, { 0xD9, 0x05 }, 0x00891988 };
    const Patch::ByteStore    kHidingSpotHeliStore                  = { 0x0042995C, { 0xC6, 0x86, 0x85, 0x07, 0x00, 0x00 }, 1 };
    const Patch::CallSite     kPVehicleConstructFind                = { 0x00689A09, COLLECTIONS_FIND };
    const Patch::CallSite     kCubicCameraChopperNoise              = { CUBIC_CAMERA_CHOPPER_NOISE_CALL, CAMERA_MOVER_CHOPPER_NOISE };
    const Patch::CallSite     kUpdateUpdateCopElements              = { 0x0059DCFD, MINIMAP_UPDATE_COP_ELEMENTS };
    const Patch::IndirectCall kUpdateCopElementsIsActive            = { 0x00579E94, { 0xFF, 0x90, 0x88, 0x00, 0x00, 0x00 } };
    const Patch::CallSite     kSyncPlayersReport911                 = { REPORT_911_CALL, EAX_DISPATCH_REPORT_911 };
    const Patch::CallSite     kMessageEventCompleteForce911State    = { FORCE_911_STATE_CALL, SOUND_AI_FORCE_911_STATE };
    const Patch::IndirectCall kAddNewHeliBackupArrives              = { 0x0070DB12, { 0xFF, 0x92, 0xF4, 0x00, 0x00, 0x00 } };
    const Patch::IndirectCall kSyncPursuitBackupArrives             = { 0x00720DA7, { 0xFF, 0x92, 0xF4, 0x00, 0x00, 0x00 } };
    const Patch::IndirectCall kUpdateIntentToBail                   = { 0x00709FAE, { 0xFF, 0x90, 0x0C, 0x02, 0x00, 0x00 } };
    const Patch::IndirectCall kSetActiveBailout                     = { 0x0070A150, { 0xFF, 0x90, 0x04, 0x01, 0x00, 0x00 } };
    const Patch::IndirectCall kOnCollisionBailout                   = { 0x0071BCF5, { 0xFF, 0x90, 0x04, 0x01, 0x00, 0x00 } };
    const Patch::IndirectCall kSpotterWaitSpotterReply              = { 0x0071A291, { 0xFF, 0x90, 0x1C, 0x01, 0x00, 0x00 } };
    const Patch::IndirectCall kSpotterWaitSpotted                   = { 0x0071A29D, { 0xFF, 0x92, 0x40, 0x01, 0x00, 0x00 } };
    const Patch::CallSite     kAssessOffroadFindClosestCop          = { 0x0070AB7D, SOUND_AI_FIND_CLOSEST_COP };
    const Patch::IndirectCall kAssessOffroadOffroad                 = { 0x0070ABAF, { 0xFF, 0x92, 0x50, 0x01, 0x00, 0x00 } };

    const char* const kCopHeli            = "copheli";
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
    C_ASSERT(offsetof(DrawHeli, mVehicle) == 0x40);
    C_ASSERT(offsetof(DrawHeli, mEffectPaused) == 0xCC);
    C_ASSERT(offsetof(DrawHeli, mWashTask) == 0xEC);

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
        float       since;
    };

    struct HazardCall {
        bool  heard;
        float at;
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

    class ReportSearch {
      public:
        ReportSearch()
            : mVehicle(NULL), mPursuitAI(NULL), mHaveReport(false), mPending(false), mSpotted(false), mLeaving(false), mHavePoint(false),
              mReportedAt(0), mRequestedAt(0), mExpires(0), mNextSight(0), mTurn(0), mRadius(0), mSpeed(0) {
            mReported = UMath::Vector(0, 0, 0);
            mCentre = mReported;
            mPoint = mReported;
        }

        void Reported(const UMath::Vector3& position);
        void Request(float lifetime);
        void Update(AICopManager* manager);

        bool HoldsBack(const IPursuitAI* ipursuitai) const {
            return mVehicle != NULL && !mSpotted && ipursuitai == mPursuitAI;
        }

        bool Keeps(const IVehicle* vehicle) const {
            return vehicle != NULL && vehicle == mVehicle && !mLeaving;
        }

        bool Searching(const ISimable* owner) const {
            return mVehicle != NULL && !mSpotted && !mLeaving && owner != NULL && mVehicle->GetSimable() == owner;
        }

        IVehicle* Helicopter() const {
            return mVehicle;
        }

      private:
        void Begin(IVehicle* vehicle, float now);
        void Forget();
        void Fly(IVehicleAI* ai);
        void NextPoint(IAIHelicopter* iah);

        IVehicle*      mVehicle;
        IPursuitAI*    mPursuitAI;
        bool           mHaveReport;
        bool           mPending;
        bool           mSpotted;
        bool           mLeaving;
        bool           mHavePoint;
        UMath::Vector3 mReported;
        UMath::Vector3 mCentre;
        UMath::Vector3 mPoint;
        float          mReportedAt;
        float          mRequestedAt;
        float          mExpires;
        float          mNextSight;
        float          mTurn;
        float          mRadius;
        float          mSpeed;
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
    int                                                    gSpeakingCallSign  = EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
    HSIMABLE__*                                            gArrival           = NULL;
    HSIMABLE__*                                            gFirstArrival      = NULL;
    HazardCall                                             gHazardCalls[EAXAirSupport::kHazardSlots];
    HazardCall                                             gAnyHazardCall;
    int                                                    gLastPlayerRoad    = 0;
    HSIMABLE__*                                            gWreckHandle       = NULL;
    int                                                    gWreckUnit         = EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
    bool                                                   gWreckPending      = false;
    bool                                                   gWreckCall         = false;
    HeliCollision                                          gHeliCollision;
    BreakerFire                                            gBreakerFires[MOST_HELICOPTERS];
    float                                                  gTopUpNext         = 0;
    const IVehicle*                                        gMinimapHelicopter = NULL;
    const FEObject*                                        gNoCopiesFor       = NULL;
    ArtCopies                                              gHeliArtCopies;
    ArtCopies                                              gHeliLineOfSiteArtCopies;
    ReportSearch                                           gReportSearch;

    HSIMABLE__*                         OwnerHandle(const IVehicle* vehicle);
    int                                  FlyingHelicopters(HSIMABLE__** handles);
    CallSign*                            FindCallSign(const HSIMABLE__* handle);
    bool                                 CallSignFlying(int unit);
    bool                                 CallSignSpeaking(int unit);
    int                                  TrackedCallSigns();
    int                                  OtherCallSign(int unit);
    int                                  NextCallSign();
    void                                 TrackCallSigns();
    bool                                 JustArrived(const CallSign* callSign);
    const IVehicle*                      VehicleFor(const HSIMABLE__* handle);
    bool                                 Chasing(const IVehicle* vehicle);
    bool                                 AnyHelicopterChasing();
    bool                                 PursuitHelicopterFlying();
    bool                                 IsRoad(int road, const BYTE* roads, int count);
    bool                                 InCity(int road);
    bool                                 HelicoptersClose();
    bool                                 HeardWithin(const HazardCall& call, float seconds);
    bool                                 LeavingNearPlayer(const IVehicle* vehicle);
    CollisionGeometry::Bounds            MakeBox(const CollisionGeometry::Bounds& root, const short* offsetMm, const short* halfMm, WORD flags);
    const CollisionGeometry::Collection* HeliCollisionFrom(const CollisionGeometry::Collection* original);
    float                                TimeDown(const IVehicle* vehicle);
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
      FuelTime(10.0f),
      SpawnDistance(250.0f),
      AllAroundSight(75.0f),
      FrontSightOnly(true),
      RespawnDelay(0.0f),
      HidingSpots(true),
      DespawnDistance(600.0f),
      Helicopters(1.0f),
      Falcon2(true),
      RecycleTime(10.0f),
      RecycleDistance(150.0f),
      Disp911Report(false),
      Bounty(100000.0f) {
    strcpy_s(Pvehicle, sizeof(Pvehicle), kCopHeli);
    const float fuelTime[HEAT_LEVELS]    = { 10.0f, 10.0f, 10.0f, 60.0f, 90.0f, 180.0f, 300.0f, 300.0f, 300.0f, 300.0f };
    const float helicopters[HEAT_LEVELS] = { 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f };
    const bool  falcon2[HEAT_LEVELS]     = { true, true, true, false, true, true, true, true, true, true };
    const bool  disp911Report[HEAT_LEVELS] = { false, false, false, true, true, true, true, true, true, true };
    memcpy(HeatFuelTime, fuelTime, sizeof(HeatFuelTime));
    memcpy(HeatHelicopters, helicopters, sizeof(HeatHelicopters));
    memcpy(HeatFalcon2, falcon2, sizeof(HeatFalcon2));
    memcpy(HeatDisp911Report, disp911Report, sizeof(HeatDisp911Report));
}

void AIVehicleHelicopter::InstallPatches() {
    Refresh();

    Patch::Begin();
    Patch::RedirectVirtual(AI_VEHICLE_HELICOPTER_VTABLE_REQUEST, AI_VEHICLE_PURSUIT_PURSUIT_REQUEST, MethodAddress(&IPursuitAI::PursuitRequest));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectCall(kUpdateUpdateFuel, MethodAddress(&AIVehicleHelicopter::UpdateFuel));
    Patch::Commit();

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

void AIVehicleHelicopter::UpdateFuel(float dT) {
    if (gReportSearch.Searching(mIOwner)) return;
    ((FloatMethod)UPDATE_FUEL)(this, dT);
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

void EAXAirSupport::InstallPatches() {
    Patch::Begin();
    Patch::RedirectIndirectCall(kAddNewHeliBackupArrives, MethodAddress(&EAXAirSupport::DelayArrival));
    Patch::RedirectIndirectCall(kSyncPursuitBackupArrives, MethodAddress(&EAXAirSupport::DelayArrival));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectVirtual(EAX_AIR_SUPPORT_VTABLE_LOST_VISUAL, EAX_AIR_SUPPORT_LOST_VISUAL, MethodAddress(&EAXAirSupport::LostVisual));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectIndirectCall(kUpdateIntentToBail, MethodAddress(&EAXAirSupport::WarnLowFuel));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectIndirectCall(kSetActiveBailout, MethodAddress(&EAXAirSupport::DeactivateBailout));
    Patch::Commit();
}

bool EAXAirSupport::HookSetHandle() {
    return Detour::Install(EAX_CHARACTER_SET_HANDLE, kSetHandleCode, sizeof(kSetHandleCode), &SetHandleEntry);
}

bool EAXAirSupport::HookAddNewHeli() {
    return Detour::Install(ADD_NEW_HELI_VTABLE_STORE, kAddNewHeliVTableCode, sizeof(kAddNewHeliVTableCode), &AddNewHeliEntry);
}

bool EAXAirSupport::HookScheduleSpeech() {
    return Detour::Install(SCHEDULE_SPEECH_PART_II, kScheduleSpeechCode, sizeof(kScheduleSpeechCode), &ScheduleSpeechEntry);
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

void __cdecl EAXAirSupport::ScheduleSpeechEntry(Detour::Registers* registers) {
    EAXAirSupport* heli = SoundAIHeli();
    EAXAirSupport* speaker = NULL;
    if (heli == NULL || !Memory::Read(registers->esp + SCHEDULE_SPEECH_SPEAKER, &speaker, sizeof(speaker)) || speaker != heli) return;
    heli->FixLineTags(registers);
    if (!AIVehicleHelicopter::sSettings.Falcon2) return;
    int other = OtherCallSign(gSpeakingCallSign);
    if (!CallSignSpeaking(other)) return;
    gSpeakingCallSign = other;
    heli->SetUnitNumber(other);
}

void EAXAirSupport::UpdateCallSigns() {
    TrackCallSigns();
    EAXAirSupport* heli = SoundAIHeli();
    if (heli == NULL) {
        gWreckPending = false;
        return;
    }
    heli->AnnounceWreck();
    heli->UseCallSign(heli->GetHandle());
    heli->AnnounceFirstArrival();
    heli->AnnounceArrival();
}

void EAXAirSupport::BackupArrives() {
    ((VoidMethod)EAX_COP_BACKUP_ARRIVES)(this);
}

void EAXAirSupport::FixLineTags(Detour::Registers* registers) {
    DWORD interfaceId = 0;
    DWORD data = 0;
    if (!Memory::Read(registers->esp + SCHEDULE_SPEECH_INTERFACE, &interfaceId, sizeof(interfaceId))
        || !Memory::Read(registers->esp + SCHEDULE_SPEECH_DATA, &data, sizeof(data)) || data == 0)
        return;
    if (interfaceId == SETUP_ATTMPT_VEH_STP_ID) {
        int pursuitType = Type_pursuit_type_Possible_Wanted;
        Memory::WriteData(data + ATTMPT_VEH_STP_PURSUIT_TYPE, &pursuitType, sizeof(pursuitType));
    }
    if (interfaceId == HELI_SPECIFIC_HELI_BAILOUT_ID && gWreckCall) {
        int bailoutType = Type_heli_bailout_type_damage_sustained;
        Memory::WriteData(data + HELI_BAILOUT_TYPE, &bailoutType, sizeof(bailoutType));
    }
}

void EAXAirSupport::LostVisual() {
    if (!FollowsChasingHelicopter()) return;
    ((VoidMethod)EAX_AIR_SUPPORT_LOST_VISUAL)(this);
}

void EAXAirSupport::WarnLowFuel() {
    if (FollowsWreck()) return;
    Say(EAX_AIR_SUPPORT_INTENT_TO_BAIL);
}

void EAXAirSupport::DeactivateBailout() {
    if (FollowsWreck() || (gWreckHandle != NULL && GetHandle() == gWreckHandle)) return;
    Say(EAX_AIR_SUPPORT_BAILOUT);
}

void EAXAirSupport::AnnounceWreck() {
    if (!gWreckPending) return;
    gWreckPending = false;
    const SoundAI* soundAI = SoundAI::Get();
    if (soundAI == NULL || !soundAI->InPursuit()) return;
    if (AIVehicleHelicopter::sSettings.Falcon2) {
        gSpeakingCallSign = gWreckUnit;
        SetUnitNumber(gWreckUnit);
    }
    gWreckCall = true;
    Say(EAX_AIR_SUPPORT_BAILOUT);
    gWreckCall = false;
}

bool EAXAirSupport::FollowsWreck() {
    const IVehicle* vehicle = VehicleFor(GetHandle());
    return vehicle != NULL && vehicle->IsDestroyed();
}

bool EAXAirSupport::FollowsChasingHelicopter() {
    const IVehicle* vehicle = VehicleFor(GetHandle());
    return vehicle != NULL && Chasing(vehicle);
}

bool SoundAI::HookSyncCarsToActors() {
    return Detour::Install(SYNC_CARS_TO_ACTORS_CHOPPER_CHECK, kChopperCheckCode, sizeof(kChopperCheckCode), &ChopperCheckEntry);
}

bool SoundAI::HookChopperLOS() {
    return Detour::Install(SYNC_CARS_TO_ACTORS_CHOPPER_LOS, kChopperLOSCode, sizeof(kChopperLOSCode), &ChopperLOSEntry);
}

void __cdecl SoundAI::ChopperLOSEntry(Detour::Registers* registers) {
    const IVehicle* vehicle = (const IVehicle*)registers->esi;
    if (vehicle == NULL || Chasing(vehicle)) return;
    registers->eax &= ~(DWORD)0xFF;
}

EAXCop* SoundAI::FindClosestCop(bool enforceLOS, bool includeHeli) {
    return ((FindClosestCopMethod)SOUND_AI_FIND_CLOSEST_COP)(this, enforceLOS, includeHeli);
}

EAXCop* SoundAI::FindOffroadSpotter(bool enforceLOS, bool includeHeli) {
    EAXAirSupport* heli = EAXAirSupport::SoundAIHeli();
    if (heli != NULL && heli->IsActive() && heli->HasLOS() && heli->FollowsChasingHelicopter()) return (EAXCop*)heli;
    return FindClosestCop(enforceLOS, includeHeli);
}

void __cdecl SoundAI::ChopperCheckEntry(Detour::Registers* registers) {
    const IVehicle* vehicle = (const IVehicle*)registers->esi;
    const DWORD* vehicleClass = (const DWORD*)registers->eax;
    if (vehicle == NULL || vehicleClass == NULL || *vehicleClass != *(const DWORD*)VEHICLE_CLASS_CHOPPER || Chasing(vehicle) || !AnyHelicopterChasing())
        return;
    registers->eax = (DWORD)&kNoVehicleClass;
}

void EAXCop::InstallPatches() {
    Patch::Begin();
    Patch::RedirectIndirectCall(kSpotterWaitSpotterReply, MethodAddress(&EAXCop::SpotterReply));
    Patch::RedirectIndirectCall(kSpotterWaitSpotted, MethodAddress(&EAXCop::Spotted));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectIndirectCall(kOnCollisionBailout, MethodAddress(&EAXCop::CollisionBailout));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectCall(kAssessOffroadFindClosestCop, MethodAddress(&SoundAI::FindOffroadSpotter));
    Patch::RedirectIndirectCall(kAssessOffroadOffroad, MethodAddress(&EAXCop::ReportOffroad));
    Patch::Commit();
}

void EAXCop::ReportOffroad(unsigned int id, bool subsequent) {
    if ((id & kCutOffroadPlaces) != 0) return;
    Offroad(id, subsequent);
}

void EAXCop::CollisionBailout() {
    if ((void*)EAXAirSupport::SoundAIHeli() == (void*)this) return;
    Say(EAX_COP_BAILOUT);
}

void EAXCop::SpotterReply() {
    Say(OpensFor911() ? EAX_COP_ATTEMPT_VEHICLE_STOP : EAX_COP_SPOTTER_REPLY);
}

void EAXCop::Spotted() {
    Say(OpensFor911() ? EAX_COP_ATTEMPT_VEHICLE_STOP : EAX_COP_SPOTTED);
}

bool EAXCop::OpensFor911() {
    const SoundAI* soundAI = SoundAI::Get();
    return soundAI != NULL && (soundAI->GetFlags() & SOUND_AI_911_STATE) != 0 && (void*)EAXAirSupport::SoundAIHeli() == (void*)this;
}

void EAXAirSupport::DelayArrival() {
    TrackCallSigns();
    HSIMABLE__* handle = GetHandle();
    if (handle == NULL || FindCallSign(handle) == NULL) {
        BackupArrives();
        return;
    }
    gFirstArrival = handle;
    if (gArrival == handle) gArrival = NULL;
}

void EAXAirSupport::UseCallSign(HSIMABLE__* handle) {
    if (!AIVehicleHelicopter::sSettings.Falcon2) {
        SetUnitNumber(Type_speaker_call_sign_id_CallSign01);
        return;
    }
    const CallSign* callSign = handle != NULL && handle != gArrival ? FindCallSign(handle) : NULL;
    if (!CallSignSpeaking(gSpeakingCallSign) && callSign != NULL) gSpeakingCallSign = callSign->unit;
    if (CallSignSpeaking(gSpeakingCallSign)) SetUnitNumber(gSpeakingCallSign);
}

void EAXAirSupport::AnnounceFirstArrival() {
    const CallSign* arrival = gFirstArrival != NULL ? FindCallSign(gFirstArrival) : NULL;
    const SoundAI* soundAI = SoundAI::Get();
    if (arrival == NULL || soundAI == NULL || !soundAI->InPursuit()) {
        gFirstArrival = NULL;
        return;
    }
    if (JustArrived(arrival) || !soundAI->CanAnnounceBackup()) return;
    gFirstArrival = NULL;
    Arrive(arrival->unit);
}

void EAXAirSupport::AnnounceArrival() {
    const CallSign* arrival = gArrival != NULL ? FindCallSign(gArrival) : NULL;
    const SoundAI* soundAI = SoundAI::Get();
    if (arrival == NULL || !AIVehicleHelicopter::sSettings.Falcon2 || soundAI == NULL || !soundAI->InPursuit()) {
        gArrival = NULL;
        return;
    }
    if (gFirstArrival != NULL || JustArrived(arrival) || !soundAI->CanAnnounceBackup()) return;
    gArrival = NULL;
    Arrive(arrival->unit);
}

void EAXAirSupport::UpdateHazardAlerts() {
    const SoundAI* soundAI = SoundAI::Get();
    if (soundAI == NULL) return;
    int road = soundAI->GetPlayerRoad();
    int previous = gLastPlayerRoad;
    if (road > 0) gLastPlayerRoad = road;

    EAXAirSupport* heli = SoundAIHeli();
    if (heli == NULL || !soundAI->PursuitActive() || !heli->IsActive()) return;
    if (road > 0 && previous > 0 && road != previous && heli->HasLOS()) {
        if (IsRoad(road, kInterchangeRoads, sizeof(kInterchangeRoads)) && !IsRoad(previous, kInterchangeRoads, sizeof(kInterchangeRoads))
            && !IsRoad(previous, kHighwayRoads, sizeof(kHighwayRoads)))
            heli->WarnOf(kHazardHighway, Type_heli_hazard_alert_type_approaching_highway);
        else if (InCity(road) && !InCity(previous))
            heli->WarnOf(kHazardCity, Type_heli_hazard_alert_type_approaching_city);
    }
    if (HelicoptersClose()) heli->WarnOf(kHazardBlimp, Type_heli_hazard_alert_type_approaching_blimp);
}

void EAXAirSupport::WarnOf(HazardSlot slot, int type) {
    if (HeardWithin(gAnyHazardCall, HAZARD_GAP_SECONDS) || HeardWithin(gHazardCalls[slot], HAZARD_REPEAT_SECONDS)) return;
    float now = Sim::GetTime();
    gAnyHazardCall.heard = true;
    gAnyHazardCall.at = now;
    gHazardCalls[slot].heard = true;
    gHazardCalls[slot].at = now;
    HazardAlert(type);
}

void EAXAirSupport::Arrive(int unit) {
    if (AIVehicleHelicopter::sSettings.Falcon2) {
        gSpeakingCallSign = unit;
        SetUnitNumber(unit);
    }
    BackupArrives();
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

bool AICopManager::HookGetAvailableCopVehicleByName() {
    return Detour::Install(GET_AVAILABLE_COP_VEHICLE_BY_NAME, kCopVehicleByNameCode, sizeof(kCopVehicleByNameCode), &GetAvailableCopVehicleByNameEntry);
}

void __cdecl AICopManager::GetAvailableCopVehicleByNameEntry(Detour::Registers* registers) {
    const char* pvehicle = AIVehicleHelicopter::sSettings.Pvehicle;
    const char* name = NULL;
    if (_stricmp(pvehicle, kCopHeli) == 0 || !Memory::Read(registers->esp + GET_COP_VEHICLE_BY_NAME_ARGUMENT, &name, sizeof(name))
        || name == NULL || _stricmp(name, kCopHeli) != 0 || !IsHelicopterPvehicle(pvehicle))
        return;
    Memory::WriteData(registers->esp + GET_COP_VEHICLE_BY_NAME_ARGUMENT, &pvehicle, sizeof(pvehicle));
}

bool AICopManager::IsHelicopterPvehicle(const char* name) {
    DWORD key = ((StringToKeyFunction)ATTRIB_STRING_TO_KEY)(name);
    const void* collection = ((FindCollectionFunction)ATTRIB_FIND_COLLECTION)(PVEHICLE_CLASS_KEY, key);
    const BYTE* vehicleClass = collection != NULL ? (const BYTE*)((CollectionGetDataMethod)ATTRIB_COLLECTION_GET_DATA)(collection, PVEHICLE_CLASS_ATTRIBUTE, 0) : NULL;
    return vehicleClass != NULL && *(const DWORD*)(vehicleClass + PVEHICLE_CLASS_VALUE) == VAULT_CHOPPER;
}

bool AIPursuit::HookIncNumCopsDestroyed() {
    return Detour::Install(INC_NUM_COPS_DESTROYED_COMBO, kRepPointsComboCode, sizeof(kRepPointsComboCode), &IncNumCopsDestroyedEntry);
}

void __cdecl AIPursuit::IncNumCopsDestroyedEntry(Detour::Registers* registers) {
    const IVehicle* vehicle = (const IVehicle*)registers->ebx;
    if (vehicle == NULL || registers->esi == 0 || !vehicle->IsChopper()) return;
    int bounty = (int)AIVehicleHelicopter::sSettings.Bounty;
    Memory::WriteData(registers->esi + AI_PURSUIT_COP_DESTROYED_REP_POINTS, &bounty, sizeof(bounty));
}

void __cdecl AICopManager::RecycleEntry(Detour::Registers* registers) {
    const AIVehicleHelicopter::Settings& settings = AIVehicleHelicopter::sSettings;
    const IVehicle* vehicle = (const IVehicle*)registers->esi;
    if (gReportSearch.Keeps(vehicle) || (vehicle != NULL && vehicle->IsChopper() && !vehicle->IsDestroyed() && LeavingNearPlayer(vehicle))) {
        BYTE stay = 0;
        Memory::WriteData(registers->esp + UPDATE_PATROLS_SHOULD_RESPAWN, &stay, sizeof(stay));
        return;
    }
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
    gReportSearch.Update(manager);
    EAXAirSupport::UpdateCallSigns();
    EAXAirSupport::UpdateHazardAlerts();
    SoundAI* soundAI = SoundAI::Get();
    if (soundAI != NULL) soundAI->UpdateReportRoad();
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
    IPursuit* ipursuit = wanted > 1 ? GetPlayerPursuit() : NULL;
    int inPursuit = 0;
    int chasing = 0;
    if (ipursuit != NULL) CountPursuitHelicopters(ipursuit, &inPursuit, &chasing);
    if (chasing < 1 || inPursuit >= wanted || !(ipursuit->GetSpawnHeliTimer() < 0) || GetNumActiveCopHelicopters() >= MOST_HELICOPTERS
        || now < gTopUpNext - TOP_UP_SECONDS) {
        gTopUpNext = now + TOP_UP_SECONDS;
        return;
    }
    if (now < gTopUpNext) return;

    AIVehicleHelicopter** heliVehicle = (AIVehicleHelicopter**)HELI_VEHICLE;
    AIVehicleHelicopter* previous = *heliVehicle;
    *heliVehicle = NULL;
    if (SpawnPursuitHelicopter(ipursuit)) {
        gTopUpNext = now + TOP_UP_SECONDS;
        return;
    }
    *heliVehicle = previous;
    gTopUpNext = now + Seconds(TOP_UP_RETRY_MS);
}

void AICopManager::CountPursuitHelicopters(const IPursuit* ipursuit, int* inPursuit, int* chasing) const {
    IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
    int count = IVehicle::ReadList((DWORD)this + COP_MANAGER_IVEHICLE_LIST, vehicles, MOST_COP_VEHICLES);
    *inPursuit = 0;
    *chasing = 0;
    for (int i = 0; i < count; ++i) {
        IVehicle* vehicle = vehicles[i];
        IVehicleAI* ai = vehicle != NULL && vehicle->IsChopper() && vehicle->IsActive() ? vehicle->GetAIVehiclePtr() : NULL;
        if (ai == NULL || ai->GetPursuit() != ipursuit) continue;
        ++*inPursuit;
        IAIHelicopter* iah = vehicle->GetAIHelicopter();
        if (!vehicle->IsDestroyed() && iah != NULL && !(iah->GetFuelTimeRemaining() <= 0)) ++*chasing;
    }
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

bool AICopManager::CanSpawn911Helicopter() {
    return GetPlayerPursuit() == NULL && !PursuitHelicopterFlying() && GetNumActiveCopHelicopters() < MOST_HELICOPTERS && VehicleSpawningEnabled(false);
}

IVehicle* AICopManager::Spawn911Helicopter(const UMath::Vector3& reported) {
    IRigidBody* player = GetLocalPlayerRigidBody();
    IVehicle* vehicle = player != NULL ? GetAvailableCopVehicleByClass(*(const DWORD*)VEHICLE_CLASS_CHOPPER, false) : NULL;
    if (vehicle == NULL && player != NULL) vehicle = GetAvailableCopVehicleByName(kCopHeli);
    IVehicleAI* ai = vehicle != NULL ? vehicle->GetAIVehiclePtr() : NULL;
    IAIHelicopter* iah = vehicle != NULL ? vehicle->GetAIHelicopter() : NULL;
    if (ai == NULL || iah == NULL) return NULL;

    UMath::Vector3 forward = UMath::Vector(0, 0, 0);
    player->GetForwardVector(forward);
    float length = sqrtf(forward.x * forward.x + forward.z * forward.z);
    UMath::Vector3 facing = length > 0 ? UMath::Vector(forward.x / length, 0, forward.z / length) : UMath::Vector(1, 0, 0);
    float distance = AIVehicleHelicopter::sSettings.SpawnDistance;
    UMath::Vector3 position = UMath::Vector(reported.x - facing.x * distance, reported.y, reported.z - facing.z * distance);
    iah->FilterHeliAltitude(position);
    position.y += REPORT_HEIGHT_FAR;
    UMath::Matrix4 orient;
    ((GenerateMatrixFunction)UTIL_GENERATE_MATRIX)(&orient, &facing, NULL);

    vehicle->Activate();
    ISimable* simable = vehicle->GetSimable();
    IRigidBody* body = simable != NULL ? simable->GetRigidBody() : NULL;
    if (body == NULL) {
        ai->UnSpawn();
        return NULL;
    }
    body->PlaceObject(orient, position);
    ai->SetSpawned();
    ++*(int*)((DWORD)this + COP_MANAGER_ACTIVE_HELICOPTERS);
    return vehicle;
}

IVehicle* AICopManager::GetAvailableCopVehicleByClass(DWORD vehicleClass, bool bValidOnesOnly) {
    return ((GetAvailableCopVehicleMethod)GET_AVAILABLE_COP_VEHICLE_BY_CLASS)(this, vehicleClass, bValidOnesOnly);
}

IVehicle* AICopManager::GetAvailableCopVehicleByName(const char* name) {
    return ((GetCopVehicleByNameMethod)GET_AVAILABLE_COP_VEHICLE_BY_NAME)(this, name);
}

bool AICopManager::VehicleSpawningEnabled(bool isdespawn) {
    return ((VehicleSpawningEnabledMethod)VEHICLE_SPAWNING_ENABLED)((BYTE*)this + COP_MANAGER_ICOPMGR, isdespawn);
}

AITarget* IPursuitAI::PursuitRequest() {
    if (gReportSearch.HoldsBack(this)) return NULL;
    return ((PursuitRequestMethod)AI_VEHICLE_PURSUIT_PURSUIT_REQUEST)(this);
}

void SoundAI::InstallPatches() {
    Patch::Begin();
    Patch::RedirectCall(kSyncPlayersReport911, MethodAddress(&EAXDispatch::Report911));
    Patch::RedirectCall(kMessageEventCompleteForce911State, MethodAddress(&SoundAI::Force911State));
    Patch::Commit();
}

bool SoundAI::InPursuit() const {
    int focus = GetFocus();
    return focus != kNoMachine && focus != kTerminal;
}

bool SoundAI::CanAnnounceBackup() const {
    int focus = GetFocus();
    return focus != kNoMachine && focus != kPursuitFlow && focus != kTerminal;
}

void SoundAI::Force911State() {
    ((VoidMethod)SOUND_AI_FORCE_911_STATE)(this);
    AIPerpVehicle::UpdateHeat();
    const void* specs = (const BYTE*)this + SOUND_AI_PURSUIT_SPECS;
    if (!AIVehicleHelicopter::sSettings.Disp911Report || *(const void* const*)((DWORD)specs + ATTRIB_INSTANCE_LAYOUT) == NULL
        || *(const void* const*)ICOPMGR_INSTANCE == NULL || *(const int*)ICOPMGR_DISABLE_COPS != 0)
        return;
    const float* lifetime = ((AttribFloatQuery)PURSUITLEVELS_LIFETIME_911)(specs);
    if (lifetime != NULL) gReportSearch.Request(*lifetime);
}

void SoundAI::UpdateReportRoad() {
    int road = GetPlayerRoad();
    int region = 0;
    int location = 0;
    if (road <= 0 || !((GetLocationFunction)MISC_SPEECH_GET_LOCATION)(road, &region, &location)) return;
    *(int*)((DWORD)this + SOUND_AI_REPORT_ROAD) = road;
}

void EAXDispatch::Report911(int pursuit_type) {
    IRigidBody* player = GetLocalPlayerRigidBody();
    if (player != NULL) gReportSearch.Reported(player->GetPosition());
    ((IntMethod)EAX_DISPATCH_REPORT_911)(this, pursuit_type);
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

void DrawHeli::InstallPatches() {
    Patch::Begin();
    Patch::RedirectVirtual(DRAW_HELI_VTABLE_ON_TASK, DRAW_HELI_ON_TASK, MethodAddress(&DrawHeli::OnTask));
    Patch::Commit();
}

bool DrawHeli::OnTask(HSIMTASK__* hTask, float dT) {
    if (hTask == mWashTask && mVehicle != NULL && mVehicle->IsDestroyed()) {
        mEffectPaused = true;
        return true;
    }
    return ((TaskMethod)DRAW_HELI_ON_TASK)(this, hTask, dT);
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
    if (gMinimapHelicopter != NULL) FadeLineOfSiteArt(gMinimapHelicopter, mHeliLineOfSiteArt);

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
    for (int i = 0; i < shown; ++i) UpdateHelicopterElement(helicopters[i], gHeliArtCopies.Get(i), gHeliLineOfSiteArtCopies.Get(i), false);
    gHeliArtCopies.HideFrom(shown);
    gHeliLineOfSiteArtCopies.HideFrom(shown);

    IVehicle* searching = gReportSearch.Helicopter();
    if (searching != NULL && searching->IsActive() && !ShowsPursuitCops(ivehicle))
        UpdateHelicopterElement(searching, mHeliElementArt, mHeliLineOfSiteArt, true);
}

void Minimap::UpdateHelicopterElement(IVehicle* helicopter, FEObject* heliArt, FEObject* lineOfSiteArt, bool searching) {
    bVector2 targetPos = MakePoint(0, 0);
    bVector2 targetDir = MakePoint(1, 0);
    ((GetVehicleVectorsFunction)GET_VEHICLE_VECTORS)(&targetPos, &targetDir, helicopter->GetSimable());

    const IPursuitAI* ipursuitai = helicopter->GetPursuitAI();
    if (ipursuitai != NULL && (searching || MinimapShowNonPursuitCops() || ipursuitai->WasWithinEngagementRadius())) {
        bool tracking = !searching && ipursuitai->GetPursuitTarget() != NULL && ipursuitai->GetTimeSinceTargetSeen() <= Seconds(TRACKING_MS);
        DWORD script = tracking ? FEHASH_TRACKING : FEHASH_INIT;
        if (!((IsScriptSetFunction)FENG_IS_SCRIPT_SET)(lineOfSiteArt, script)) ((SetScriptFunction)FENG_SET_SCRIPT)(lineOfSiteArt, script, true);
    }
    UpdateElementArt(&targetPos, &targetDir, heliArt, false);
    UpdateElementArt(&targetPos, &targetDir, lineOfSiteArt, false);
    FadeLineOfSiteArt(helicopter, lineOfSiteArt);
}

void Minimap::FadeLineOfSiteArt(const IVehicle* helicopter, FEObject* lineOfSiteArt) {
    float down = TimeDown(helicopter);
    if (down < 0) return;
    float left = 1 - down / Seconds(CONE_FADE_MS);
    if (left <= 0) {
        ((SetInvisibleFunction)FENG_SET_INVISIBLE)(lineOfSiteArt);
        return;
    }
    FEColor color;
    ((GetObjectColorFunction)FENG_GET_OBJECT_COLOR)(&color, lineOfSiteArt);
    DWORD value = ((ColorValueMethod)FECOLOR_TO_U32)(&color);
    DWORD alpha = (DWORD)((float)(value >> COLOR_ALPHA_SHIFT) * left);
    ((SetColorFunction)FENG_SET_COLOR)(lineOfSiteArt, (value & COLOR_RGB) | (alpha << COLOR_ALPHA_SHIFT));
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

    bool CallSignFlying(int unit) {
        for (int i = 0; i < MOST_CALL_SIGNS; ++i)
            if (gCallSigns[i].handle != NULL && gCallSigns[i].unit == unit) return true;
        return false;
    }

    bool CallSignSpeaking(int unit) {
        for (int i = 0; i < MOST_CALL_SIGNS; ++i)
            if (gCallSigns[i].handle != NULL && gCallSigns[i].handle != gArrival && gCallSigns[i].unit == unit) return true;
        return false;
    }

    int TrackedCallSigns() {
        int tracked = 0;
        for (int i = 0; i < MOST_CALL_SIGNS; ++i)
            if (gCallSigns[i].handle != NULL) ++tracked;
        return tracked;
    }

    int NextCallSign() {
        if (!CallSignFlying(EAXAirSupport::Type_speaker_call_sign_id_CallSign01)) return EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
        if (!CallSignFlying(EAXAirSupport::Type_speaker_call_sign_id_CallSign02)) return EAXAirSupport::Type_speaker_call_sign_id_CallSign02;
        int unit = gNextCallSign;
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
                if (gArrival == gCallSigns[c].handle) gArrival = NULL;
                if (gFirstArrival == gCallSigns[c].handle) gFirstArrival = NULL;
                const IVehicle* vehicle = VehicleFor(gCallSigns[c].handle);
                if (vehicle != NULL && vehicle->IsDestroyed()) {
                    gWreckHandle = gCallSigns[c].handle;
                    gWreckUnit = gCallSigns[c].unit;
                    gWreckPending = true;
                }
                gCallSigns[c].handle = NULL;
                gCallSigns[c].unit = 0;
            }
        }
        for (int i = 0; i < count; ++i) {
            if (FindCallSign(flying[i]) != NULL) continue;
            CallSign* slot = FindCallSign(NULL);
            if (slot == NULL) break;
            if (gWreckHandle == flying[i]) gWreckHandle = NULL;
            bool joining = TrackedCallSigns() > 0;
            slot->unit = NextCallSign();
            slot->handle = flying[i];
            slot->since = Sim::GetTime();
            if (joining) gArrival = flying[i];
        }
        CallSign* arrival = gArrival != NULL ? FindCallSign(gArrival) : NULL;
        if (arrival == NULL || TrackedCallSigns() != 1) return;
        arrival->unit = EAXAirSupport::Type_speaker_call_sign_id_CallSign01;
        gFirstArrival = gArrival;
        gArrival = NULL;
    }

    bool JustArrived(const CallSign* callSign) {
        float now = Sim::GetTime();
        return now >= callSign->since && now < callSign->since + Seconds(ARRIVAL_DELAY_MS);
    }

    const IVehicle* VehicleFor(const HSIMABLE__* handle) {
        if (handle == NULL) return NULL;
        IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
        int count = IVehicle::ReadList(AI_COPS, vehicles, MOST_COP_VEHICLES);
        for (int i = 0; i < count; ++i)
            if (vehicles[i] != NULL && vehicles[i]->IsChopper() && OwnerHandle(vehicles[i]) == handle) return vehicles[i];
        return NULL;
    }

    bool Chasing(const IVehicle* vehicle) {
        if (!vehicle->IsActive() || vehicle->IsDestroyed()) return false;
        IAIHelicopter* iah = vehicle->GetAIHelicopter();
        return iah != NULL && iah->GetFuelTimeRemaining() > 0;
    }

    bool AnyHelicopterChasing() {
        IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
        int count = IVehicle::ReadList(AI_COPS, vehicles, MOST_COP_VEHICLES);
        for (int i = 0; i < count; ++i)
            if (vehicles[i] != NULL && vehicles[i]->IsChopper() && Chasing(vehicles[i])) return true;
        return false;
    }

    bool PursuitHelicopterFlying() {
        IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
        int count = IVehicle::ReadList(AI_COPS, vehicles, MOST_COP_VEHICLES);
        for (int i = 0; i < count; ++i) {
            const IVehicle* vehicle = vehicles[i];
            if (vehicle == NULL || !vehicle->IsChopper() || !vehicle->IsActive() || vehicle->IsDestroyed()) continue;
            IVehicleAI* ai = vehicle->GetAIVehiclePtr();
            if (ai != NULL && ai->GetPursuit() != NULL) return true;
        }
        return false;
    }

    bool IsRoad(int road, const BYTE* roads, int count) {
        for (int i = 0; i < count; ++i)
            if (roads[i] == road) return true;
        return false;
    }

    bool InCity(int road) {
        int region = 0;
        int location = 0;
        return ((GetLocationFunction)MISC_SPEECH_GET_LOCATION)(road, &region, &location) && region == LOCATION_REGION_CITY;
    }

    bool HelicoptersClose() {
        IVehicle* vehicles[MOST_COP_VEHICLES] = { NULL };
        int count = IVehicle::ReadList(AI_COPS, vehicles, MOST_COP_VEHICLES);
        UMath::Vector3 positions[MOST_HELICOPTERS];
        int helicopters = 0;
        for (int i = 0; i < count && helicopters < MOST_HELICOPTERS; ++i) {
            const IVehicle* vehicle = vehicles[i];
            if (vehicle == NULL || !vehicle->IsChopper() || !vehicle->IsActive() || vehicle->IsDestroyed()) continue;
            const UMath::Vector3* position = vehicle->GetPosition();
            if (position != NULL && UMath::IsFinite(*position)) positions[helicopters++] = *position;
        }
        for (int a = 0; a < helicopters; ++a)
            for (int b = a + 1; b < helicopters; ++b) {
                UMath::Vector3 apart = positions[a] - positions[b];
                if (apart.x * apart.x + apart.z * apart.z < APART_DISTANCE * APART_DISTANCE && fabsf(apart.y) < APART_HEIGHT)
                    return true;
            }
        return false;
    }

    bool HeardWithin(const HazardCall& call, float seconds) {
        float now = Sim::GetTime();
        return call.heard && now >= call.at && now < call.at + seconds;
    }

    bool LeavingNearPlayer(const IVehicle* vehicle) {
        IVehicleAI* ai = vehicle->GetAIVehiclePtr();
        const UMath::Vector3* position = vehicle->GetPosition();
        IRigidBody* player = GetLocalPlayerRigidBody();
        if (ai == NULL || ai->GetPursuit() != NULL || position == NULL || player == NULL) return false;

        UMath::Vector3 ahead = player->GetLinearVelocity();
        if (!(player->GetSpeedXZ() > DESPAWN_SLOWEST_SPEED)) player->GetForwardVector(ahead);
        float length = sqrtf(UMath::Dot(ahead, ahead));
        UMath::Vector3 lead = length > 0 ? ahead * (DESPAWN_LEAD / length) : UMath::Vector(0, 0, 0);
        UMath::Vector3 apart = *position - (player->GetPosition() + lead);
        return UMath::IsFinite(apart) && UMath::Dot(apart, apart) < gDespawnDistance * gDespawnDistance;
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

    void ReportSearch::Reported(const UMath::Vector3& position) {
        mReported = position;
        mReportedAt = Sim::GetTime();
        mHaveReport = UMath::IsFinite(position);
    }

    void ReportSearch::Request(float lifetime) {
        if (!Memory::IsFinite(lifetime) || lifetime <= 0) return;
        float now = Sim::GetTime();
        bool fresh = mHaveReport && now >= mReportedAt && now - mReportedAt <= REPORT_FRESH_SECONDS;
        IRigidBody* player = GetLocalPlayerRigidBody();
        if (!fresh && player == NULL) return;

        mCentre = fresh ? mReported : player->GetPosition();
        mHaveReport = false;
        mRequestedAt = now;
        if (mVehicle != NULL && !mLeaving && !mSpotted) {
            mExpires = Larger(mExpires, now + lifetime);
            mHavePoint = false;
            return;
        }
        mExpires = now + lifetime;
        mPending = true;
    }

    void ReportSearch::Update(AICopManager* manager) {
        float now = Sim::GetTime();
        IVehicleAI* ai = NULL;
        if (mVehicle != NULL && manager->HasVehicle(mVehicle) && mVehicle->IsActive() && !mVehicle->IsDestroyed()) ai = mVehicle->GetAIVehiclePtr();
        if (mVehicle != NULL && (ai == NULL || ai->GetPursuit() != NULL || (mLeaving && mPending))) Forget();

        if (mVehicle == NULL) {
            if (mPending && (now < mRequestedAt || now >= mExpires)) mPending = false;
            if (mPending && manager->CanSpawn911Helicopter()) Begin(manager->Spawn911Helicopter(mCentre), now);
            return;
        }
        if (mLeaving) return;

        if (manager->GetPlayerPursuit() != NULL) mSpotted = true;
        if (!mSpotted && (ai->GetGoalName() != 0 || now >= mExpires || now < mRequestedAt)) {
            if (ai->GetGoalName() == 0) mPursuitAI->StartFlee();
            mLeaving = true;
            return;
        }
        if (!mSpotted && now >= mNextSight) {
            mNextSight = now + Seconds(REPORT_SIGHT_MS);
            AITarget* target = ai->GetTarget();
            mSpotted = target != NULL && mPursuitAI->CanSeeTarget(target);
        }
        Fly(ai);
    }

    void ReportSearch::Begin(IVehicle* vehicle, float now) {
        if (vehicle == NULL) return;
        const UMath::Vector3* position = vehicle->GetPosition();
        mVehicle = vehicle;
        mPursuitAI = vehicle->GetPursuitAI();
        mPending = false;
        mSpotted = false;
        mLeaving = mPursuitAI == NULL;
        mHavePoint = false;
        mNextSight = now;
        mTurn = position != NULL ? atan2f(position->z - mCentre.z, position->x - mCentre.x) / RADIANS_PER_TURN : 0;
    }

    void ReportSearch::Forget() {
        mVehicle = NULL;
        mPursuitAI = NULL;
        mSpotted = false;
        mLeaving = false;
        mHavePoint = false;
    }

    void ReportSearch::Fly(IVehicleAI* ai) {
        IAIHelicopter* iah = mVehicle->GetAIHelicopter();
        ISimable* simable = mVehicle->GetSimable();
        IRigidBody* body = simable != NULL ? simable->GetRigidBody() : NULL;
        if (iah == NULL || body == NULL) return;

        if (!mHavePoint || UMath::Distancexz(body->GetPosition(), mPoint) < REPORT_POINT_RANGE) NextPoint(iah);
        ai->SetDriveSpeed(mSpeed);
        iah->SetDestinationVelocity(body->GetLinearVelocity());
        ai->SetDriveTarget(mPoint);
        iah->SetLookAtPosition(mPoint);
        ai->DoDriving(REPORT_DRIVE_FLAGS);
    }

    void ReportSearch::NextPoint(IAIHelicopter* iah) {
        const AIActionHeliPursuit::Settings& search = AIActionHeliPursuit::sSettings;
        mRadius = mHavePoint ? Smaller(mRadius + REPORT_RADIUS_GROWTH, search.SearchRadiusLargest) : search.SearchRadiusFirst;
        mTurn += (float)1 / REPORT_POINTS_PER_TURN;
        if (mTurn >= 1) mTurn -= 1;

        float span = search.SearchRadiusLargest - search.SearchRadiusFirst;
        float progress = span > 0 ? Clamp((mRadius - search.SearchRadiusFirst) / span, 0, 1) : 1;
        mSpeed = search.SearchSpeedMin + (search.SearchSpeedMax - search.SearchSpeedMin) * progress;

        float angle = mTurn * RADIANS_PER_TURN;
        mPoint = UMath::Vector(mCentre.x + cosf(angle) * mRadius, mCentre.y, mCentre.z + sinf(angle) * mRadius);
        iah->RestrictPointToRoadNet(mPoint);
        iah->FilterHeliAltitude(mPoint);
        mPoint.y += REPORT_HEIGHT_NEAR + (REPORT_HEIGHT_FAR - REPORT_HEIGHT_NEAR) * progress;
        mHavePoint = true;
    }

}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "AIVehicleHelicopter.hpp"
#include "AIActionHeliPursuit.hpp"
#include "HeliSheet.hpp"
#include "SimpleChopper.hpp"

namespace {

    constexpr uintptr_t kgHeliVehicle = 0x0090D61Cu;

    constexpr uintptr_t kManager_ClearPlayback      = 0x0070E570u;
    constexpr uintptr_t kEventHistory_Find          = 0x004CB3F0u;
    constexpr uintptr_t kManager_mGlobalHistory     = 0x00992718u;
    constexpr uintptr_t kEAXAirSupport_Bailout      = 0x00717C00u;
    constexpr unsigned  kIsDeadSlot                 = 0x6Cu;
    constexpr int       kSPCH1_EventID_HeliBailout  = 176;

    bool gHeliWasDead = false;

    constexpr uint32_t kCopHeliCollection = 0x47C914A3u;
    constexpr uint16_t kBoundsPrimVsWorld   = 1u << 1;
    constexpr uint16_t kBoundsPrimVsObjects = 1u << 2;
    constexpr uint16_t kBoundsPrimVsGround  = 1u << 3;
    constexpr uint16_t kBoundsBox           = 1u << 6;
    constexpr float    kBoundsPrecision     = 1000.0f;
    constexpr int16_t  kQuaternionOne       = 32767;

    static_assert(offsetof(CollisionGeometry::Bounds, fFlags) == 0xE, "Bounds::fFlags");
    static_assert(offsetof(CollisionGeometry::Bounds, fHalfDimensions) == 0x10, "Bounds::fHalfDimensions");
    static_assert(offsetof(CollisionGeometry::Bounds, fNumChildren) == 0x16, "Bounds::fNumChildren");
    static_assert(offsetof(CollisionGeometry::Bounds, fChildIndex) == 0x1E, "Bounds::fChildIndex");
    static_assert(offsetof(CollisionGeometry::Bounds, fRadius) == 0x20, "Bounds::fRadius");
    static_assert(offsetof(CollisionGeometry::Bounds, fCollection) == 0x2C, "Bounds::fCollection");
    static_assert(sizeof(CollisionGeometry::Bounds) == 0x30, "Bounds");
    static_assert(sizeof(CollisionGeometry::Collection) == 0x10, "Collection");

    struct alignas(16) HeliCollision {
        CollisionGeometry::Collection header;
        CollisionGeometry::Bounds     bounds[3];
    };

    HeliCollision gHeliCollision = {};

    int16_t CompressBounds(float value) {
        return static_cast<int16_t>(std::lround(value * kBoundsPrecision));
    }

    CollisionGeometry::Bounds MakeBox(const CollisionGeometry::Bounds& root, const UMath::Vector3& offset, const UMath::Vector3& half, uint16_t flags) {
        CollisionGeometry::Bounds box = {};
        box.fOrientation[3] = kQuaternionOne;
        box.fPosition[0] = CompressBounds(offset.x);
        box.fPosition[1] = CompressBounds(offset.y);
        box.fPosition[2] = CompressBounds(offset.z);
        box.fFlags = flags;
        box.fHalfDimensions[0] = CompressBounds(half.x);
        box.fHalfDimensions[1] = CompressBounds(half.y);
        box.fHalfDimensions[2] = CompressBounds(half.z);
        box.fPCloudIndex = -1;
        box.fChildIndex = -1;
        box.fRadius = std::fmax(half.x, std::fmax(half.y, half.z));
        box.fSurface = root.fSurface;
        box.fCollection = &gHeliCollision.header;
        return box;
    }

    const CollisionGeometry::Collection* HeliCollisionFrom(const CollisionGeometry::Collection* original) {
        const auto* root = reinterpret_cast<const CollisionGeometry::Bounds*>(original + 1);
        if (root->fNumChildren != 0) return original;

        gHeliCollision.header = *original;
        gHeliCollision.header.fNumBounds = 3;
        gHeliCollision.bounds[0] = *root;
        gHeliCollision.bounds[0].fNumChildren = 2;
        gHeliCollision.bounds[0].fChildIndex = 1;
        gHeliCollision.bounds[0].fCollection = &gHeliCollision.header;
        gHeliCollision.bounds[1] = MakeBox(*root, CollisionGeometry::Collections::kHeliCabinOffset, CollisionGeometry::Collections::kHeliCabinHalf,
                                           kBoundsBox | kBoundsPrimVsWorld | kBoundsPrimVsObjects | kBoundsPrimVsGround);
        gHeliCollision.bounds[2] = MakeBox(*root, CollisionGeometry::Collections::kHeliTailOffset, CollisionGeometry::Collections::kHeliTailHalf,
                                           kBoundsBox | kBoundsPrimVsWorld | kBoundsPrimVsObjects);
        Log::Info("CollisionGeometry  COPHELI 0x%p (1 box, half %.3f %.3f %.3f) -> 0x%p: cabin box half (%.2f %.2f %.2f) at (%.2f %.2f %.2f), tail box half (%.2f %.2f %.2f) at (%.2f %.2f %.2f)",
                  original, root->fHalfDimensions[0] / kBoundsPrecision, root->fHalfDimensions[1] / kBoundsPrecision, root->fHalfDimensions[2] / kBoundsPrecision,
                  &gHeliCollision.header, CollisionGeometry::Collections::kHeliCabinHalf.x, CollisionGeometry::Collections::kHeliCabinHalf.y,
                  CollisionGeometry::Collections::kHeliCabinHalf.z, CollisionGeometry::Collections::kHeliCabinOffset.x, CollisionGeometry::Collections::kHeliCabinOffset.y,
                  CollisionGeometry::Collections::kHeliCabinOffset.z, CollisionGeometry::Collections::kHeliTailHalf.x, CollisionGeometry::Collections::kHeliTailHalf.y,
                  CollisionGeometry::Collections::kHeliTailHalf.z, CollisionGeometry::Collections::kHeliTailOffset.x, CollisionGeometry::Collections::kHeliTailOffset.y,
                  CollisionGeometry::Collections::kHeliTailOffset.z);
        return &gHeliCollision.header;
    }

    constexpr uintptr_t kSim_GetTime                       = 0x006E8DE0u;
    constexpr uintptr_t kAttrib_StringToKey                = 0x00454640u;
    constexpr uintptr_t kVehicleRenderConnEffect_Update    = 0x00744A50u;
    constexpr unsigned  kEffect_mLocalMatrix               = 0x10u;
    constexpr unsigned  kEffect_mEmitterGroup              = 0x50u;
    constexpr unsigned  kCopManagerNumActiveCopHelicopters = 0x9Cu;
    constexpr unsigned  kCopManagerVehicles                = 0xC8u;
    constexpr unsigned  kCopManagerNumVehicles             = 0xD0u;
    constexpr unsigned  kCopManagerIPursuitList            = 0x128u;
    constexpr unsigned  kPursuitListNext                   = 0x0u;
    constexpr unsigned  kPursuitListValue                  = 0x8u;
    constexpr unsigned  kIPursuitIsPlayerPursuit           = 0x8Cu;
    constexpr int       kMostCopVehicles                   = 64;
    constexpr int       kMostPursuits                      = 32;
    constexpr float     kBreakerBurstTime                  = 6.0f;
    constexpr float     kLongestEffectStep                 = 0.1f;
    constexpr float     kTopUpDelay                        = 5.0f;

    constexpr const char* kBreakerBurstGroup      = "fxex_gasstation";
    constexpr const char* kBreakerFlamesGroup     = "michael_bay";
    constexpr const char* kBreakerSmallFlames     = "fxfire_sm1";
    constexpr const char* kBreakerFlareUps        = "fxex_large1";
    constexpr const char* kBreakerSmokeGroup      = "fxcar_cop_death1";

    struct alignas(16) bMatrix4 {
        float m[16];
    };

    class EmitterEffect {
      public:
        bool Play(const char* group, const bMatrix4& world, float dT) {
            if (!mReady) {
                const bMatrix4 identity = { { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f } };
                std::memcpy(mEffect + kEffect_mLocalMatrix, &identity, sizeof(identity));
                mReady = true;
            }
            if (group != mGroup) {
                mGroup = group;
                mKey = Game::Call<uint32_t>(kAttrib_StringToKey, group);
            }
            Game::ThisCall<void>(kVehicleRenderConnEffect_Update, static_cast<void*>(mEffect), &world, mKey, dT, 1.0f,
                                 static_cast<const void*>(nullptr));
            return EmitterGroup() != nullptr;
        }

        void Remove() {
            if (!mReady) return;
            mGroup = nullptr;
            Game::ThisCall<void>(kVehicleRenderConnEffect_Update, static_cast<void*>(mEffect), static_cast<const void*>(nullptr), 0u, 0.0f, 0.0f,
                                 static_cast<const void*>(nullptr));
        }

        const void* EmitterGroup() const {
            const void* group = nullptr;
            std::memcpy(&group, mEffect + kEffect_mEmitterGroup, sizeof(group));
            return group;
        }

      private:
        alignas(16) unsigned char mEffect[0x60] = {};
        bool        mReady = false;
        const char* mGroup = nullptr;
        uint32_t    mKey = 0;
    };

    bool IsFinite(const UMath::Vector3& v) {
        return Memory::IsFinite(v.x) && Memory::IsFinite(v.y) && Memory::IsFinite(v.z);
    }

    const IVehicle* AsVehicle(uintptr_t vehicle) {
        return reinterpret_cast<const IVehicle*>(vehicle);
    }

    const UMath::Vector3* VehiclePosition(uintptr_t vehicle) {
        return Game::ThisCall<const UMath::Vector3*>(Game::VirtualFunction(AsVehicle(vehicle), IVehicle::kGetPosition), vehicle);
    }

    class BreakerFire {
      public:
        uintptr_t Vehicle() const {
            return mVehicle;
        }

        void Watch(uintptr_t vehicle) {
            mVehicle = vehicle;
            mWasDestroyed = AsVehicle(vehicle)->IsDestroyed();
        }

        void Forget() {
            if (mBurning) Log::Info("IVehicle 0x%08X  heli wreck left AICopManager, breaker fire out", mVehicle);
            PutOut();
            mVehicle = 0;
            mWasDestroyed = false;
        }

        void Update() {
            const bool destroyed = AsVehicle(mVehicle)->IsDestroyed();
            if (destroyed && !mWasDestroyed) Ignite();
            if (!destroyed && mBurning) {
                Log::Info("IVehicle 0x%08X  heli respawned, breaker fire out", mVehicle);
                PutOut();
            }
            mWasDestroyed = destroyed;
            if (!destroyed && !AsVehicle(mVehicle)->IsActive()) {
                mVehicle = 0;
                return;
            }
            if (mBurning) Burn();
        }

      private:
        void Ignite() {
            mBurning = true;
            mSmallFlames = false;
            mBurnStart = Game::Call<float>(kSim_GetTime);
            mLastBurnTime = mBurnStart;
            mBurstEnd = mBurnStart + kBreakerBurstTime;
            Log::Info("IVehicle 0x%08X  heli IsDestroyed  %s, then %s + %s until the wreck is gone", mVehicle, kBreakerBurstGroup, kBreakerFlamesGroup,
                      kBreakerSmokeGroup);
        }

        void PutOut() {
            mBurst.Remove();
            mFlames.Remove();
            mFlareUps.Remove();
            mSmoke.Remove();
            mBurning = false;
        }

        void Burn() {
            const float now = Game::Call<float>(kSim_GetTime);
            if (now < mBurnStart) {
                PutOut();
                return;
            }
            const float dT = std::clamp(now - mLastBurnTime, 0.0f, kLongestEffectStep);
            mLastBurnTime = now;

            const UMath::Vector3* position = VehiclePosition(mVehicle);
            if (position == nullptr || !IsFinite(*position)) return;
            bMatrix4 world = { { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f } };
            world.m[12] = position->z;
            world.m[13] = -position->x;
            world.m[14] = position->y;

            if (now < mBurstEnd)
                mBurst.Play(kBreakerBurstGroup, world, dT);
            else
                mBurst.Remove();

            if (!mSmallFlames && !mFlames.Play(kBreakerFlamesGroup, world, dT)) {
                mSmallFlames = true;
                Log::Info("emittergroup %s not in attributes.bin, %s + %s instead", kBreakerFlamesGroup, kBreakerSmallFlames, kBreakerFlareUps);
            }
            if (mSmallFlames) {
                mFlames.Play(kBreakerSmallFlames, world, dT);
                mFlareUps.Play(kBreakerFlareUps, world, dT);
            }
            mSmoke.Play(kBreakerSmokeGroup, world, dT);
        }

        EmitterEffect mBurst;
        EmitterEffect mFlames;
        EmitterEffect mFlareUps;
        EmitterEffect mSmoke;
        uintptr_t     mVehicle = 0;
        bool          mWasDestroyed = false;
        bool          mBurning = false;
        bool          mSmallFlames = false;
        float         mBurnStart = 0.0f;
        float         mBurstEnd = -1.0f;
        float         mLastBurnTime = 0.0f;
    };

    BreakerFire gBreakerFires[AIVehicleHelicopter::kMostHelicopters];
    float       gTopUpWaitStart = 0.0f;

    static_assert(offsetof(AIVehicleHelicopter, mIOwner) == 0x34, "Behavior::mIOwner");
    static_assert(offsetof(AIVehicleHelicopter, mDriveSpeed) == 0x84, "AIVehicle::mDriveSpeed");
    static_assert(offsetof(AIVehicleHelicopter, mDest) == 0x88, "AIVehicle::mDest");
    static_assert(offsetof(AIVehicleHelicopter, mIAIHelicopter) == 0x7A4, "AIVehicleHelicopter's IAIHelicopter");
    static_assert(offsetof(AIVehicleHelicopter, mDestinationVelocity) == 0x7AC, "AIVehicleHelicopter::mDestinationVelocity");
    static_assert(offsetof(AIVehicleHelicopter, mLookAtPosition) == 0x7B8, "AIVehicleHelicopter::mLookAtPosition");
    static_assert(offsetof(AIVehicleHelicopter, mLastPlaceHeliSawPerp) == 0x7C4, "AIVehicleHelicopter::mLastPlaceHeliSawPerp");
    static_assert(offsetof(AIVehicleHelicopter, mHeight) == 0x7D0, "AIVehicleHelicopter::mHeight");
    static_assert(offsetof(AIVehicleHelicopter, mStrafeToDest) == 0x7D4, "AIVehicleHelicopter::mStrafeToDest");
    static_assert(offsetof(AIVehicleHelicopter, mPerpHiddenFromMe) == 0x7D5, "AIVehicleHelicopter::mPerpHiddenFromMe");
    static_assert(offsetof(AIVehicleHelicopter, mHeliFuelTimeRemaining) == 0x7D8, "AIVehicleHelicopter::mHeliFuelTimeRemaining");
    static_assert(offsetof(AIVehicleHelicopter, mShadowScale) == 0x7DC, "AIVehicleHelicopter::mShadowScale");
    static_assert(offsetof(AIVehicleHelicopter, mDustStormIntensity) == 0x7E0, "AIVehicleHelicopter::mDustStormIntensity");
    static_assert(offsetof(AIVehicleHelicopter, mHeliSheetCoord) == 0x7F0, "AIVehicleHelicopter::mHeliSheetCoord");
    static_assert(offsetof(AIVehicleHelicopter, mISimpleChopper) == 0x8B0, "AIVehicleHelicopter::mISimpleChopper");

    constexpr float kReferenceStep  = 1.0f / 60.0f;
    constexpr float kApartDistance  = 14.0f;
    constexpr float kApartHeight    = 6.0f;
    constexpr float kApartStiffness = 1.0f;
    constexpr float kClosestApart   = 0.01f;

    struct HelicopterState {
        ISimable*        lastOwner = nullptr;
        float            lastFuel = 0.0f;
        LARGE_INTEGER    lastTick = {};
        bool             haveLastTick = false;
        float            smoothed = 0.0f;
        unsigned         frames = 0;
        float            elapsed = 0.0f;
        bool             reported = false;
        bool             keptApart = false;
        HeliSheet::State sheet = {};
#if defined(_DEBUG)
        unsigned long    lastLiveLogMs = 0;
#endif
    };

    LARGE_INTEGER gFrequency = {};
    bool          gClockReady = false;
    bool          gCounter = false;

    unsigned long gLastRotateMs = 0;
    unsigned long gLastFuelWarningMs = 0;

    PerHelicopter<HelicopterState, AIVehicleHelicopter::kMostHelicopters * 2> gHelicopters;
    HelicopterState                                                           gUnknownHelicopter;
    HelicopterState*                                                          gHeli = &gUnknownHelicopter;

    void ResetClock() {
        gHeli->haveLastTick = false;
        gHeli->smoothed = 0.0f;
    }

    void InitClock() {
        gClockReady = true;
        gCounter = QueryPerformanceFrequency(&gFrequency) != 0 && gFrequency.QuadPart > 0;
        if (gCounter)
            Log::Info("clock  QueryPerformanceFrequency=%lld", gFrequency.QuadPart);
        else
            Log::Warn("clock  QueryPerformanceFrequency=0  dt fixed at %.5f", kReferenceStep);
    }

    void AdvanceClock() {
        if (!gClockReady) InitClock();
        float dt = kReferenceStep;
        LARGE_INTEGER now;
        if (gCounter && QueryPerformanceCounter(&now)) {
            if (gHeli->haveLastTick) {
                const double seconds = static_cast<double>(now.QuadPart - gHeli->lastTick.QuadPart) / static_cast<double>(gFrequency.QuadPart);
                if (seconds > 0.0 && seconds < 100.0) dt = static_cast<float>(seconds);
            }
            gHeli->lastTick = now;
            gHeli->haveLastTick = true;
        }

        const float outlier = kReferenceStep * 3.0f;
        if (gHeli->smoothed <= 0.0f)
            gHeli->smoothed = dt <= outlier ? dt : kReferenceStep;
        else if (dt <= outlier)
            gHeli->smoothed += (dt - gHeli->smoothed) * std::clamp(dt / 0.25f, 0.0f, 1.0f);
        gHeli->smoothed = std::clamp(gHeli->smoothed, kReferenceStep * 0.125f, kReferenceStep);

        ++gHeli->frames;
        gHeli->elapsed += dt;
        if (!gHeli->reported && gHeli->elapsed >= 2.0f) {
            gHeli->reported = true;
            Log::Info("clock  OnDriving %u calls / %.3f s = %.2f Hz  smoothedDt=%.5f", gHeli->frames, gHeli->elapsed, gHeli->frames / gHeli->elapsed,
                      gHeli->smoothed);
        }
    }

    float SmoothedFrames() {
        return gHeli->smoothed > 0.0f ? gHeli->smoothed / kReferenceStep : 1.0f;
    }

    constexpr uintptr_t kGetVehicleVectors         = 0x00579840u;
    constexpr uintptr_t kFEngSetInvisible          = 0x00514C70u;
    constexpr uintptr_t kFEngIsScriptSet           = 0x00514DA0u;
    constexpr uintptr_t kFEngSetScript             = 0x00514D10u;
    constexpr uintptr_t kMinimapShowNonPursuitCops = 0x0091CF00u;
    constexpr uint32_t  kFEHASH_INIT               = 0x001744B3u;
    constexpr uint32_t  kFEHASH_TRACKING           = 0x55CAFA92u;
    constexpr float     kTrackingTime              = 0.25f;
    constexpr int       kExtraHelicopters          = AIVehicleHelicopter::kMostHelicopters - 1;
    constexpr int       kDeepestArt                = 16;
    constexpr int       kMostCopiesMade            = kExtraHelicopters * 2;

    static_assert(offsetof(FEMinList, Head) == 0x8, "FEMinList::Head");
    static_assert(sizeof(FEMinList) == 0x10, "FEMinList");
    static_assert(offsetof(FEObject, Type) == 0x18, "FEObject::Type");
    static_assert(offsetof(FEObject, Flags) == 0x1C, "FEObject::Flags");
    static_assert(offsetof(FEObject, pData) == 0x2C, "FEObject::pData");
    static_assert(offsetof(FEObject, Responses) == 0x34, "FEObject::Responses");
    static_assert(offsetof(FEObject, Scripts) == 0x44, "FEObject::Scripts");
    static_assert(offsetof(FEObject, Cached) == 0x58, "FEObject::Cached");
    static_assert(sizeof(FEObject) == 0x5C, "FEObject");
    static_assert(offsetof(FEGroup, Children) == 0x5C, "FEGroup::Children");
    static_assert(sizeof(FEGroup) == 0x6C, "FEGroup");
    static_assert(offsetof(FEMultiImage, ImageFlags) == 0x5C, "FEImage::ImageFlags");
    static_assert(offsetof(FEMultiImage, hTexture) == 0x60, "FEMultiImage::hTexture");
    static_assert(sizeof(FEMultiImage) == 0x78, "FEMultiImage");
    static_assert(offsetof(FEPackage, Objects) == 0x44, "FEPackage::Objects");
    static_assert(sizeof(bPList) == 0x8, "bPList");
    static_assert(offsetof(Minimap, Objects) == 0x8, "HudElement::Objects");
    static_assert(offsetof(Minimap, pPackageName) == 0x10, "HudElement::pPackageName");
    static_assert(offsetof(Minimap, mHeliElementArt) == 0xC8, "Minimap::mHeliElementArt");
    static_assert(offsetof(Minimap, mHeliLineOfSiteArt) == 0xCC, "Minimap::mHeliLineOfSiteArt");

    const IVehicle* gMinimapHelicopter = nullptr;
    const FEObject* gNoCopiesFor = nullptr;

    void GetVehicleVectors(bVector2* pos, bVector2* dir, ISimable* isimable) {
        Game::Call<void>(kGetVehicleVectors, pos, dir, isimable);
    }

    void FEngSetInvisible(FEObject* object) {
        Game::Call<void>(kFEngSetInvisible, object);
    }

    bool FEngIsScriptSet(FEObject* object, uint32_t script) {
        return Game::Call<bool>(kFEngIsScriptSet, object, script);
    }

    void FEngSetScript(FEObject* object, uint32_t script, bool force) {
        Game::Call<void>(kFEngSetScript, object, script, force);
    }

    bool MinimapShowNonPursuitCops() {
        return Game::Global<bool>(kMinimapShowNonPursuitCops);
    }

    FEMinList* FindParentList(FEMinList& list, const FEObject* object, int depth) {
        for (FEObject* child = list.Head; child != nullptr; child = child->Next) {
            if (child == object) return &list;
            if (child->Type != FEObject::FE_Group || depth >= kDeepestArt) continue;
            if (FEMinList* found = FindParentList(static_cast<FEGroup*>(child)->Children, object, depth + 1)) return found;
        }
        return nullptr;
    }

    void CopyArtData(const FEObject* from, FEObject* to, int depth) {
        to->Cached = nullptr;
        to->NameHash = from->NameHash;
        if (from->pData != nullptr && to->pData != nullptr && from->DataSize == to->DataSize) std::memcpy(to->pData, from->pData, from->DataSize);
        if (from->Type == FEMultiImage::FE_MultiImage && to->Type == FEMultiImage::FE_MultiImage) {
            const FEMultiImage* image = static_cast<const FEMultiImage*>(from);
            FEMultiImage*       copy = static_cast<FEMultiImage*>(to);
            std::memcpy(copy->hTexture, image->hTexture, sizeof(image->hTexture));
            std::memcpy(copy->TextureFlags, image->TextureFlags, sizeof(image->TextureFlags));
        }
        if (from->Type != FEObject::FE_Group || to->Type != FEObject::FE_Group || depth >= kDeepestArt) return;
        const FEObject* child = static_cast<const FEGroup*>(from)->Children.Head;
        FEObject*       copy = static_cast<FEGroup*>(to)->Children.Head;
        for (; child != nullptr && copy != nullptr; child = child->Next, copy = copy->Next) CopyArtData(child, copy, depth + 1);
    }

    void RegisterArt(bPList& objects, FEObject* object, int depth) {
        if (object->Type != FEObject::FE_Group) {
            objects.AddTail(object);
            return;
        }
        if (depth >= kDeepestArt) return;
        for (FEObject* child = static_cast<FEGroup*>(object)->Children.Head; child != nullptr; child = child->Next) RegisterArt(objects, child, depth + 1);
    }

    class ArtCopies {
      public:
        int Check(const FEObject* original) {
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
            if (linked != mCount) Log::Info("Minimap  FEObject 0x%p  %d of %d copies still linked", original, linked, mCount);
            mCount = linked;
            return linked;
        }

        void Grow(FEPackage* package, bPList& objects, FEObject* original, int wanted) {
            if (mCount >= wanted || package == nullptr) return;
            FEMinList* parent = FindParentList(package->Objects, original, 0);
            if (parent == nullptr) {
                Log::Warn("Minimap  FEObject 0x%p NameHash=0x%08X not found in FEPackage 0x%p, no copies", original, original->NameHash, package);
                return;
            }
            FEObject* previous = mCount > 0 ? mCopies[mCount - 1] : original;
            while (mCount < wanted) {
                if (mMade >= kMostCopiesMade) {
                    Log::Warn("Minimap  FEObject 0x%p  %d copies made already, no more", original, mMade);
                    return;
                }
                ++mMade;
                FEObject* copy = original->Clone(false);
                if (copy == nullptr) return;
                CopyArtData(original, copy, 0);
                parent->AddNode(previous, copy);
                RegisterArt(objects, copy, 0);
                mCopies[mCount++] = copy;
                previous = copy;
                Log::Info("Minimap  FEObject 0x%p NameHash=0x%08X Type=%d Clone -> 0x%p, FEMinList 0x%p NumElements=%u", original, original->NameHash,
                          original->Type, copy, parent, parent->NumElements);
            }
        }

        int Count() const {
            return mCount;
        }

        FEObject* Get(int index) const {
            return mCopies[index];
        }

        void HideFrom(int index) {
            for (int i = index; i < mCount; ++i) FEngSetInvisible(mCopies[i]);
        }

      private:
        const FEObject* mOriginal = nullptr;
        FEObject*       mCopies[kExtraHelicopters] = {};
        int             mCount = 0;
        int             mMade = 0;
    };

    ArtCopies gHeliArtCopies;
    ArtCopies gHeliLineOfSiteArtCopies;

#if defined(_DEBUG)
    constexpr unsigned long kLiveLogMs = 1000;

    void DescribeWords(const void* object, size_t count, char* out, size_t outLength) {
        const uint32_t* words = static_cast<const uint32_t*>(object);
        size_t used = 0;
        out[0] = '\0';
        for (size_t i = 0; i < count && used < outLength; ++i)
            used += static_cast<size_t>(std::snprintf(out + used, outLength - used, "%s%08X", i ? " " : "", words[i]));
    }
#endif

}

void EAXAirSupport::HeliDestroyed() {
    Game::Call<void>(kManager_ClearPlayback);
    if (uint16_t* history = Game::ThisCall<uint16_t*>(kEventHistory_Find, kManager_mGlobalHistory, kSPCH1_EventID_HeliBailout)) history[2] = 0;
    Bailout();
}

bool EAXAirSupport::IsDead() {
    return Game::ThisCall<bool>(Game::VirtualFunction(this, kIsDeadSlot), this);
}

void EAXAirSupport::Bailout() {
    Game::ThisCall<void>(kEAXAirSupport_Bailout, this);
}

void __cdecl EAXAirSupport::UpdateEntry(Detour::Registers* registers) {
    auto* heli = reinterpret_cast<EAXAirSupport*>(static_cast<uintptr_t>(registers->esi));
    const bool dead = heli->IsDead();
    if (dead && !gHeliWasDead) heli->HeliDestroyed();
    gHeliWasDead = dead;
}

const CollisionGeometry::Collection* CollisionGeometry::Collections::Find(uint32_t name) {
    return Game::ThisCall<const Collection*>(kFind, this, name);
}

const CollisionGeometry::Collection* CollisionGeometry::Collections::VehicleFind(uint32_t name) {
    const Collection* found = Find(name);
    if (found == nullptr || name != kCopHeliCollection || found->fNameHash != kCopHeliCollection || found->fNumBounds != 1) return found;
    return HeliCollisionFrom(found);
}

bool IVehicle::IsActiveOnMinimap() const {
    return IsActive() && (this == gMinimapHelicopter || !IsChopper());
}

void Minimap::UpdateCopElements(IVehicle* ivehicle) {
    Game::ThisCall<void>(kUpdateCopElements, this, ivehicle);
}

void Minimap::UpdateElementArt(bVector2* elementPos, bVector2* elementDir, FEObject* elementArt, bool pulse) {
    Game::ThisCall<void>(kUpdateElementArt, this, elementPos, elementDir, elementArt, pulse);
}

bool Minimap::ShowsPursuitCops(IVehicle* ivehicle) const {
    if (MinimapShowNonPursuitCops()) return true;
    IVehicleAI* ai = ivehicle != nullptr ? ivehicle->GetAIVehiclePtr() : nullptr;
    IPursuit*   ipursuit = ai != nullptr ? ai->GetPursuit() : nullptr;
    return ipursuit != nullptr && !ipursuit->IsPursuitBailed();
}

void Minimap::UpdateHelicopterElements(IVehicle* ivehicle) {
    IVehicle* helicopters[kExtraHelicopters] = {};
    int       count = 0;
    gMinimapHelicopter = nullptr;
    const IVehicle::List& vehicles = IVehicle::AICops();
    for (size_t i = 0; vehicles.mBegin != nullptr && i < vehicles.mSize && i < static_cast<size_t>(kMostCopVehicles); ++i) {
        IVehicle* vehicle = vehicles.mBegin[i];
        if (vehicle == nullptr || !vehicle->IsActive() || !vehicle->IsChopper()) continue;
        if (gMinimapHelicopter == nullptr)
            gMinimapHelicopter = vehicle;
        else if (count < kExtraHelicopters)
            helicopters[count++] = vehicle;
    }

    UpdateCopElements(ivehicle);
    if (mHeliElementArt == nullptr || mHeliLineOfSiteArt == nullptr) return;

    int copies = std::min(gHeliArtCopies.Check(mHeliElementArt), gHeliLineOfSiteArtCopies.Check(mHeliLineOfSiteArt));
    if (count > 0 && !ShowsPursuitCops(ivehicle)) count = 0;
    if (count > copies && mHeliElementArt != gNoCopiesFor) {
        cFEng*     feng = cFEng::Get();
        FEPackage* package = feng != nullptr && pPackageName != nullptr ? feng->FindPackage(pPackageName) : nullptr;
        if (package == nullptr) Log::Warn("Minimap 0x%p  cFEng::FindPackage(pPackageName[+0x10]=0x%p) found nothing, no copies", this, pPackageName);
        gHeliArtCopies.Grow(package, Objects, mHeliElementArt, count);
        gHeliLineOfSiteArtCopies.Grow(package, Objects, mHeliLineOfSiteArt, count);
        copies = std::min(gHeliArtCopies.Count(), gHeliLineOfSiteArtCopies.Count());
        if (copies < count) gNoCopiesFor = mHeliElementArt;
    }

    const int shown = std::min(count, copies);
    for (int i = 0; i < shown; ++i) UpdateHelicopterElement(helicopters[i], gHeliArtCopies.Get(i), gHeliLineOfSiteArtCopies.Get(i));
    gHeliArtCopies.HideFrom(shown);
    gHeliLineOfSiteArtCopies.HideFrom(shown);
}

void Minimap::UpdateHelicopterElement(IVehicle* helicopter, FEObject* heliArt, FEObject* lineOfSiteArt) {
    bVector2 targetPos = {};
    bVector2 targetDir = { 1.0f, 0.0f };
    GetVehicleVectors(&targetPos, &targetDir, helicopter->GetSimable());

    const IPursuitAI* ipursuitai = helicopter->GetPursuitAI();
    if (ipursuitai != nullptr && (MinimapShowNonPursuitCops() || ipursuitai->WasWithinEngagementRadius())) {
        const bool     tracking = ipursuitai->GetPursuitTarget() != nullptr && ipursuitai->GetTimeSinceTargetSeen() <= kTrackingTime;
        const uint32_t script = tracking ? kFEHASH_TRACKING : kFEHASH_INIT;
        if (!FEngIsScriptSet(lineOfSiteArt, script)) FEngSetScript(lineOfSiteArt, script, true);
    }
    UpdateElementArt(&targetPos, &targetDir, heliArt, false);
    UpdateElementArt(&targetPos, &targetDir, lineOfSiteArt, false);
}

bool AICopManager::HasVehicle(uintptr_t vehicle) const {
    const uintptr_t manager = reinterpret_cast<uintptr_t>(this);
    uintptr_t vehicles = 0;
    int       count = 0;
    if (!Memory::Read(manager + kCopManagerVehicles, &vehicles, sizeof(vehicles)) || !Memory::Read(manager + kCopManagerNumVehicles, &count, sizeof(count))
        || vehicles == 0 || count <= 0 || count > kMostCopVehicles)
        return false;
    for (int i = 0; i < count; ++i) {
        uintptr_t listed = 0;
        if (Memory::Read(vehicles + i * sizeof(uintptr_t), &listed, sizeof(listed)) && listed == vehicle) return true;
    }
    return false;
}

void AICopManager::UpdateBreakerFire() {
    for (BreakerFire& fire : gBreakerFires)
        if (fire.Vehicle() != 0 && !HasVehicle(fire.Vehicle())) fire.Forget();

    const uintptr_t manager = reinterpret_cast<uintptr_t>(this);
    uintptr_t vehicles = 0;
    int       count = 0;
    if (Memory::Read(manager + kCopManagerVehicles, &vehicles, sizeof(vehicles)) && Memory::Read(manager + kCopManagerNumVehicles, &count, sizeof(count))
        && vehicles != 0 && count > 0 && count <= kMostCopVehicles) {
        for (int i = 0; i < count; ++i) {
            uintptr_t vehicle = 0;
            if (!Memory::Read(vehicles + i * sizeof(uintptr_t), &vehicle, sizeof(vehicle)) || vehicle == 0) continue;
            bool watched = false;
            for (const BreakerFire& fire : gBreakerFires) watched = watched || fire.Vehicle() == vehicle;
            if (watched || !AsVehicle(vehicle)->IsChopper() || !AsVehicle(vehicle)->IsActive()) continue;
            for (BreakerFire& fire : gBreakerFires) {
                if (fire.Vehicle() != 0) continue;
                fire.Watch(vehicle);
                break;
            }
        }
    }

    for (BreakerFire& fire : gBreakerFires)
        if (fire.Vehicle() != 0) fire.Update();
}

int AICopManager::GetNumActiveCopHelicopters() const {
    int count = 0;
    Memory::Read(reinterpret_cast<uintptr_t>(this) + kCopManagerNumActiveCopHelicopters, &count, sizeof(count));
    return count;
}

IPursuit* AICopManager::GetPlayerPursuit() const {
    uintptr_t head = 0;
    uintptr_t node = 0;
    if (!Memory::Read(reinterpret_cast<uintptr_t>(this) + kCopManagerIPursuitList, &head, sizeof(head)) || head == 0
        || !Memory::Read(head + kPursuitListNext, &node, sizeof(node)))
        return nullptr;
    for (int i = 0; node != 0 && node != head && i < kMostPursuits; ++i) {
        IPursuit* ipursuit = nullptr;
        if (!Memory::Read(node + kPursuitListValue, &ipursuit, sizeof(ipursuit))) return nullptr;
        if (ipursuit != nullptr && Game::ThisCall<bool>(Game::VirtualFunction(ipursuit, kIPursuitIsPlayerPursuit), ipursuit)) return ipursuit;
        if (!Memory::Read(node + kPursuitListNext, &node, sizeof(node))) return nullptr;
    }
    return nullptr;
}

bool AICopManager::SpawnPursuitHelicopter(IPursuit* ipursuit) {
    return Game::ThisCall<bool>(kSpawnPursuitHelicopter, this, ipursuit);
}

void AICopManager::UpdateHelicopterTopUp() {
    const float now = Game::Call<float>(kSim_GetTime);
    const int   wanted = AIVehicleHelicopter::Helicopters();
    const int   active = GetNumActiveCopHelicopters();
    if (wanted <= 1 || active < 1 || active >= wanted || now < gTopUpWaitStart) {
        gTopUpWaitStart = now;
        return;
    }
    if (now - gTopUpWaitStart < kTopUpDelay) return;
    gTopUpWaitStart = now;

    IPursuit* ipursuit = GetPlayerPursuit();
    if (ipursuit == nullptr) return;
    AIVehicleHelicopter*&      heliVehicle = Game::Global<AIVehicleHelicopter*>(kgHeliVehicle);
    AIVehicleHelicopter* const previous = heliVehicle;
    heliVehicle = nullptr;
    const bool spawned = SpawnPursuitHelicopter(ipursuit);
    if (!spawned) heliVehicle = previous;
    Log::Info("AICopManager 0x%p  SpawnPursuitHelicopter(IPursuit=0x%p)=%d  mNumActiveCopHelicopters[+0x9C] %d -> %d, Helicopters=%d  gHeliVehicle[0x%08X] 0x%p -> 0x%p",
              this, ipursuit, spawned, active, GetNumActiveCopHelicopters(), wanted, kgHeliVehicle, previous, heliVehicle);
}

void __cdecl AICopManager::ApplyBreakerZonesEntry(Detour::Registers* registers) {
    auto* manager = reinterpret_cast<AICopManager*>(static_cast<uintptr_t>(registers->ecx));
    if (manager == nullptr) return;
    manager->UpdateBreakerFire();
    manager->UpdateHelicopterTopUp();
}

void AIVehicleHelicopter::Refresh() {
    sBrake.stoppingRatio = sSettings.StoppingRatio;
    sBrake.speedScale    = sSettings.BrakeSpeedScale;
    Log::Info("AIVehicleHelicopter  sBrake@0x%p stoppingRatio=%g speedScale=%g", &sBrake, sBrake.stoppingRatio, sBrake.speedScale);

    sSightFacing = sSettings.FrontSightOnly ? kFrontFacing : kAnyFacing;
    sDespawnDistance = sSettings.DespawnDistance;
    if (sSightPatched && sSettings.AllAroundSight != sVisualSphere) {
        Log::Info("kHeliVisualSphere[0x%08X] %g -> %g", kHeliVisualSphere, Game::Global<float>(kHeliVisualSphere), sSettings.AllAroundSight);
        Game::Global<float>(kHeliVisualSphere) = sSettings.AllAroundSight;
        sVisualSphere = sSettings.AllAroundSight;
    }
    Log::Info("AIVehicleHelicopter  sSightFacing@0x%p=%g kHeliVisualSphere=%g", &sSightFacing, sSightFacing, sVisualSphere);

    if (sHidingPatched && sSettings.HidingSpots != sHidingSpots) {
        if (Patch::RewriteStoredByte("AIVehicle hiding zone | mHiddenFromHelicopters = true", kHidingSpotHeliStore, sHidingSpots ? 1 : 0,
                                     sSettings.HidingSpots ? 1 : 0))
            sHidingSpots = sSettings.HidingSpots;
        else
            sHidingPatched = false;
    }

    if (!sSpawnPatched || sSettings.SpawnDistance == sSpawnDistance) return;
    if (!Patch::RewritePushedFloat("AICopManager::SpawnPursuitHelicopter | testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition,
                                   sSpawnDistance, sSettings.SpawnDistance)) {
        sSpawnPatched = false;
        return;
    }
    sSpawnDistance = sSettings.SpawnDistance;
}

int AIVehicleHelicopter::Helicopters() {
    return std::clamp(static_cast<int>(std::lround(sSettings.Helicopters)), 1, kMostHelicopters);
}

void __cdecl AIVehicleHelicopter::OnDrivingEntry(Detour::Registers* registers) {
    auto* heli = reinterpret_cast<AIVehicleHelicopter*>(static_cast<uintptr_t>(registers->ecx));
    if (heli == nullptr || heli->mIOwner == nullptr) return;
    gHeli = &gHelicopters.Use(heli);
    AdvanceClock();
    heli->UpdateHelicopterOptions();
}

void AIVehicleHelicopter::AvoidCameraHook(UMath::Vector3& dest) {
    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(GetRigidBody());
    if (pursuit != nullptr && pursuit->IsStriking()) return;
    Game::ThisCall<void>(kAvoidCamera, this, &dest);
}

void __cdecl AIVehicleHelicopter::RespawnTimerEntry(Detour::Registers* registers) {
    if (sSettings.RespawnDelay <= 0.0f) return;
    Log::Info("AIPursuit 0x%08X  mSpawnHeliTimer[+0xE0] = %g (TimeBetweenHeliActive %g)", registers->edi, sSettings.RespawnDelay,
              *reinterpret_cast<const float*>(&registers->ecx));
    std::memcpy(&registers->ecx, &sSettings.RespawnDelay, sizeof(registers->ecx));
}

void __cdecl AIVehicleHelicopter::CanSeeTargetEntry(Detour::Registers* registers) {
    if (sSettings.LineOfSight <= 0.0f) return;
    std::memcpy(&registers->edx, &sSettings.LineOfSight, sizeof(registers->edx));
}

void AIVehicleHelicopter::UpdateHelicopterOptions() {
    IRigidBody* rigidBody = GetRigidBody();
    if (rigidBody == nullptr) return;
    const UMath::Vector3 position = rigidBody->GetPosition();
    if (!IsFinite(position)) return;

    if (!Memory::IsFinite(mHeliFuelTimeRemaining)) {
        const unsigned long now = GetTickCount();
        if (now - gLastFuelWarningMs >= 30000) {
            gLastFuelWarningMs = now;
            uint32_t bits = 0;
            std::memcpy(&bits, &mHeliFuelTimeRemaining, sizeof(bits));
            Log::Warn("AIVehicleHelicopter 0x%p  mHeliFuelTimeRemaining[+0x7D8]=0x%08X non-finite, frame skipped", this, bits);
        }
        return;
    }

    const unsigned long now = GetTickCount();
    if (now - gLastRotateMs >= 30000) {
        gLastRotateMs = now;
        Log::CheckRotate();
    }

    AIPerpVehicle::UpdateHeat();

    const bool newHelicopter = mIOwner != gHeli->lastOwner || mHeliFuelTimeRemaining > gHeli->lastFuel + 1.0f;
    if (newHelicopter)
        BeginHelicopter(position);
    else
        gHeli->lastFuel = mHeliFuelTimeRemaining;

    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(rigidBody);
    const bool striking = pursuit != nullptr && pursuit->IsStriking();
    sBrake.stoppingRatio = striking ? kNoBrakeRatio : sSettings.StoppingRatio;
    sBrake.speedScale    = striking ? 1.0f : sSettings.BrakeSpeedScale;
    HeliSheet::Update(gHeli->sheet, position, pursuit);
    KeepApart(rigidBody);
    SimpleChopper::ApplySpeedCap(mISimpleChopper);
    if (!newHelicopter) SimpleChopper::ScaleMotionFilters(SmoothedFrames());
    LogLive(rigidBody, pursuit);
}

void AIVehicleHelicopter::KeepApart(IRigidBody* rigidBody) const {
    const IVehicle::List& vehicles = IVehicle::AICops();
    if (vehicles.mBegin == nullptr) return;
    const UMath::Vector3 position = rigidBody->GetPosition();
    UMath::Vector3       velocity = rigidBody->GetLinearVelocity();
    const IRigidBody*    closest = nullptr;
    float                closestDistance = kApartDistance;
    bool                 pushed = false;
    for (size_t i = 0; i < vehicles.mSize && i < static_cast<size_t>(kMostCopVehicles); ++i) {
        const IVehicle* vehicle = vehicles.mBegin[i];
        if (vehicle == nullptr || !vehicle->IsActive() || !vehicle->IsChopper() || vehicle->IsDestroyed()) continue;
        ISimable*   simable = vehicle->GetSimable();
        IRigidBody* other = simable != nullptr ? simable->GetRigidBody() : nullptr;
        if (other == nullptr || other == rigidBody) continue;

        const UMath::Vector3 apart = position - other->GetPosition();
        const float distance = std::sqrt(apart.x * apart.x + apart.z * apart.z);
        if (std::fabs(apart.y) >= kApartHeight || distance >= kApartDistance || !IsFinite(apart)) continue;

        const bool           lowerAddress = reinterpret_cast<uintptr_t>(rigidBody) < reinterpret_cast<uintptr_t>(other);
        const UMath::Vector3 away = distance > kClosestApart ? UMath::Vector3{ apart.x / distance, 0.0f, apart.z / distance }
                                                             : UMath::Vector3{ lowerAddress ? 1.0f : -1.0f, 0.0f, 0.0f };
        const UMath::Vector3& otherVelocity = other->GetLinearVelocity();
        const float separating = (velocity.x - otherVelocity.x) * away.x + (velocity.z - otherVelocity.z) * away.z;
        const float wanted = (kApartDistance - distance) * kApartStiffness;
        if (distance < closestDistance) {
            closest = other;
            closestDistance = distance;
        }
        if (separating >= wanted) continue;
        velocity = velocity + away * ((wanted - separating) * 0.5f);
        pushed = true;
    }

    if (pushed) rigidBody->SetLinearVelocity(velocity);
    if ((closest != nullptr) != gHeli->keptApart) {
        gHeli->keptApart = closest != nullptr;
        Log::Info("AIVehicleHelicopter 0x%p  %s IRigidBody 0x%p  dXZ=%.2f  kept %.0f m apart", this, closest != nullptr ? "too close to" : "clear of",
                  closest, closestDistance, kApartDistance);
    }
}

void AIVehicleHelicopter::BeginHelicopter(const UMath::Vector3& position) {
    gHeli->lastOwner = mIOwner;
    gHeli->sheet = {};
    ResetClock();

    Log::Info("AIVehicleHelicopter 0x%p  BeginHelicopter  gHeliVehicle[0x%08X]=0x%p mIOwner[+0x34]=0x%p mISimpleChopper[+0x8B0]=0x%p "
              "pos=(%.2f, %.2f, %.2f)",
              this, kgHeliVehicle, Game::Global<AIVehicleHelicopter*>(kgHeliVehicle), mIOwner, mISimpleChopper, position.x, position.y, position.z);
    if (const IRigidBody* player = GetLocalPlayerRigidBody()) {
        const UMath::Vector3& playerPosition = player->GetPosition();
        Log::Info("  player IRigidBody=0x%p pos=(%.2f, %.2f, %.2f) dXZ=%.2f dY=%.2f", player, playerPosition.x, playerPosition.y, playerPosition.z,
                  UMath::Distancexz(position, playerPosition), position.y - playerPosition.y);
    }
    Log::Info("  mHeliFuelTimeRemaining[+0x7D8]=%.2f FuelTime=%g heliLOSdistance=%g push[0x%08X]=%g", mHeliFuelTimeRemaining, sSettings.FuelTime,
              sSettings.LineOfSight, kSpawnPursuitHelicopterIncNavPosition.va, Game::Global<float>(kSpawnPursuitHelicopterIncNavPosition.va + 1));

    gHeli->lastFuel = mHeliFuelTimeRemaining;
    if (sSettings.FuelTime > 0.0f) {
        mHeliFuelTimeRemaining = sSettings.FuelTime;
        gHeli->lastFuel = sSettings.FuelTime;
        Log::Info("  mHeliFuelTimeRemaining[+0x7D8] -> %.2f", mHeliFuelTimeRemaining);
    }
}

#if defined(_DEBUG)
void AIVehicleHelicopter::LogLive(const IRigidBody* rigidBody, const AIActionHeliPursuit* pursuit) const {
    const unsigned long now = GetTickCount();
    const bool full = now - gHeli->lastLiveLogMs >= kLiveLogMs;
    if (full) {
        gHeli->lastLiveLogMs = now;
        const UMath::Vector3& position = rigidBody->GetPosition();
        const UMath::Vector3& velocity = rigidBody->GetLinearVelocity();
        char sheet[sizeof(HeliSheetCoordinate) / sizeof(uint32_t) * 9 + 1];
        DescribeWords(&mHeliSheetCoord, sizeof(HeliSheetCoordinate) / sizeof(uint32_t), sheet, sizeof(sheet));

        Log::Info("AIVehicleHelicopter 0x%p  gHeliVehicle[0x%08X]=0x%p mIOwner[+0x34]=0x%p IRigidBody=0x%p mISimpleChopper[+0x8B0]=0x%p", this,
                  kgHeliVehicle, Game::Global<AIVehicleHelicopter*>(kgHeliVehicle), mIOwner, rigidBody, mISimpleChopper);
        Log::Info("  pos=(%.2f, %.2f, %.2f) vel=(%.2f, %.2f, %.2f) GetSpeedXZ=%.2f smoothedDt=%.5f frames=%.3f", position.x, position.y, position.z,
                  velocity.x, velocity.y, velocity.z, rigidBody->GetSpeedXZ(), gHeli->smoothed, SmoothedFrames());
        Log::Info("  mDriveSpeed[+0x84]=%.2f mDest[+0x88]=(%.2f, %.2f, %.2f) mHeight[+0x7D0]=%.2f mStrafeToDest[+0x7D4]=%d mPerpHiddenFromMe[+0x7D5]=%d",
                  mDriveSpeed, mDest.x, mDest.y, mDest.z, mHeight, mStrafeToDest, mPerpHiddenFromMe);
        Log::Info("  mDestinationVelocity[+0x7AC]=(%.2f, %.2f, %.2f) mLookAtPosition[+0x7B8]=(%.2f, %.2f, %.2f) mLastPlaceHeliSawPerp[+0x7C4]=(%.2f, %.2f, %.2f)",
                  mDestinationVelocity.x, mDestinationVelocity.y, mDestinationVelocity.z, mLookAtPosition.x, mLookAtPosition.y, mLookAtPosition.z,
                  mLastPlaceHeliSawPerp.x, mLastPlaceHeliSawPerp.y, mLastPlaceHeliSawPerp.z);
        Log::Info("  mHeliFuelTimeRemaining[+0x7D8]=%.2f mShadowScale[+0x7DC]=%.3f mDustStormIntensity[+0x7E0]=%.3f", mHeliFuelTimeRemaining,
                  mShadowScale, mDustStormIntensity);
        Log::Info("  mHeliSheetCoord[+0x7F0] %s", sheet);
        if (const IRigidBody* player = GetLocalPlayerRigidBody()) {
            const UMath::Vector3& playerPosition = player->GetPosition();
            Log::Info("  player IRigidBody=0x%p pos=(%.2f, %.2f, %.2f) GetSpeedXZ=%.2f dXZ=%.2f dY=%.2f", player, playerPosition.x, playerPosition.y,
                      playerPosition.z, player->GetSpeedXZ(), UMath::Distancexz(position, playerPosition), position.y - playerPosition.y);
        }
        Log::Info("  sBrake@0x%p stoppingRatio=%g speedScale=%g  push[0x%08X]=%g heliLOSdistance=%g", &sBrake, sBrake.stoppingRatio, sBrake.speedScale,
                  kSpawnPursuitHelicopterIncNavPosition.va, Game::Global<float>(kSpawnPursuitHelicopterIncNavPosition.va + 1), sSettings.LineOfSight);
    }
    if (pursuit != nullptr) pursuit->LogLive(full);
    HeliSheet::LogLive(gHeli->sheet, full);
    if (full) SimpleChopper::LogLive(mISimpleChopper);
}
#endif

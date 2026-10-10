#define WIN32_LEAN_AND_MEAN
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

    constexpr float kReferenceStep = 1.0f / 60.0f;

    LARGE_INTEGER gFrequency = {};
    LARGE_INTEGER gLastTick = {};
    bool          gClockReady = false;
    bool          gCounter = false;
    bool          gHaveLastTick = false;
    float         gSmoothed = 0.0f;
    unsigned      gFrames = 0;
    float         gElapsed = 0.0f;
    bool          gReported = false;

    unsigned long gLastRotateMs = 0;
    unsigned long gLastFuelWarningMs = 0;
    ISimable*     gLastOwner = nullptr;
    float         gLastFuel = 0.0f;

    void ResetClock() {
        gHaveLastTick = false;
        gSmoothed = 0.0f;
    }

    void InitClock() {
        gClockReady = true;
        gCounter = QueryPerformanceFrequency(&gFrequency) != 0 && gFrequency.QuadPart > 0;
        if (gCounter)
            Log::Info("clock  QueryPerformanceFrequency=%lld", gFrequency.QuadPart);
        else
            Log::Warn("clock  QueryPerformanceFrequency=0  dt fixed at %.5f", kReferenceStep);
        ResetClock();
    }

    void AdvanceClock() {
        if (!gClockReady) InitClock();
        float dt = kReferenceStep;
        LARGE_INTEGER now;
        if (gCounter && QueryPerformanceCounter(&now)) {
            if (gHaveLastTick) {
                const double seconds = static_cast<double>(now.QuadPart - gLastTick.QuadPart) / static_cast<double>(gFrequency.QuadPart);
                if (seconds > 0.0 && seconds < 100.0) dt = static_cast<float>(seconds);
            }
            gLastTick = now;
            gHaveLastTick = true;
        }

        const float outlier = kReferenceStep * 3.0f;
        if (gSmoothed <= 0.0f)
            gSmoothed = dt <= outlier ? dt : kReferenceStep;
        else if (dt <= outlier)
            gSmoothed += (dt - gSmoothed) * std::clamp(dt / 0.25f, 0.0f, 1.0f);
        gSmoothed = std::clamp(gSmoothed, kReferenceStep * 0.125f, kReferenceStep);

        ++gFrames;
        gElapsed += dt;
        if (!gReported && gElapsed >= 2.0f) {
            gReported = true;
            Log::Info("clock  OnDriving %u calls / %.3f s = %.2f Hz  smoothedDt=%.5f", gFrames, gElapsed, gFrames / gElapsed, gSmoothed);
        }
    }

    float SmoothedFrames() {
        return gSmoothed > 0.0f ? gSmoothed / kReferenceStep : 1.0f;
    }

    bool IsFinite(const UMath::Vector3& v) {
        return Memory::IsFinite(v.x) && Memory::IsFinite(v.y) && Memory::IsFinite(v.z);
    }

#if defined(_DEBUG)
    constexpr unsigned long kLiveLogMs = 1000;

    unsigned long gLastLiveLogMs = 0;

    void DescribeWords(const void* object, size_t count, char* out, size_t outLength) {
        const uint32_t* words = static_cast<const uint32_t*>(object);
        size_t used = 0;
        out[0] = '\0';
        for (size_t i = 0; i < count && used < outLength; ++i)
            used += static_cast<size_t>(std::snprintf(out + used, outLength - used, "%s%08X", i ? " " : "", words[i]));
    }
#endif

}

bool HeliVehicleActive() {
    return Game::Global<AIVehicleHelicopter*>(kgHeliVehicle) != nullptr;
}

void AIVehicleHelicopter::Refresh() {
    sBrake.stoppingRatio = sSettings.StoppingRatio;
    sBrake.speedScale    = sSettings.BrakeSpeedScale;
    Log::Info("AIVehicleHelicopter  sBrake@0x%p stoppingRatio=%g speedScale=%g", &sBrake, sBrake.stoppingRatio, sBrake.speedScale);

    if (!sSpawnPatched || sSettings.SpawnDistance == sSpawnDistance) return;
    if (!Patch::RewritePushedFloat("AICopManager::SpawnPursuitHelicopter | testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition,
                                   sSpawnDistance, sSettings.SpawnDistance)) {
        sSpawnPatched = false;
        return;
    }
    sSpawnDistance = sSettings.SpawnDistance;
}

void __cdecl AIVehicleHelicopter::OnDrivingEntry(Detour::Registers* registers) {
    auto* heli = reinterpret_cast<AIVehicleHelicopter*>(static_cast<uintptr_t>(registers->ecx));
    if (heli == nullptr || !HeliVehicleActive()) return;
    AdvanceClock();
    heli->UpdateHelicopterOptions();
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

    const bool newHelicopter = mIOwner != gLastOwner || mHeliFuelTimeRemaining > gLastFuel + 1.0f;
    if (newHelicopter)
        BeginHelicopter(position);
    else
        gLastFuel = mHeliFuelTimeRemaining;

    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(rigidBody);
    HeliSheet::Update(position, pursuit);
    SimpleChopper::ApplySpeedCap(mISimpleChopper);
    if (!newHelicopter) SimpleChopper::ScaleMotionFilters(SmoothedFrames());
    LogLive(rigidBody, pursuit);
}

void AIVehicleHelicopter::AvoidCamera(UMath::Vector3& dest) {
    Game::ThisCall<void>(kAvoidCamera, this, &dest);
}

void AIVehicleHelicopter::AvoidCameraHook(UMath::Vector3& dest) {
    const AIActionHeliPursuit* pursuit = AIActionHeliPursuit::Find(GetRigidBody());
    if (pursuit != nullptr && pursuit->IsCrushing()) return;
    AvoidCamera(dest);
}

void AIVehicleHelicopter::BeginHelicopter(const UMath::Vector3& position) {
    gLastOwner = mIOwner;
    HeliSheet::BeginHelicopter();
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

    gLastFuel = mHeliFuelTimeRemaining;
    if (sSettings.FuelTime > 0.0f) {
        mHeliFuelTimeRemaining = sSettings.FuelTime;
        gLastFuel = sSettings.FuelTime;
        Log::Info("  mHeliFuelTimeRemaining[+0x7D8] -> %.2f", mHeliFuelTimeRemaining);
    }
}

#if defined(_DEBUG)
void AIVehicleHelicopter::LogLive(const IRigidBody* rigidBody, const AIActionHeliPursuit* pursuit) const {
    const unsigned long now = GetTickCount();
    const bool full = now - gLastLiveLogMs >= kLiveLogMs;
    if (full) {
        gLastLiveLogMs = now;
        const UMath::Vector3& position = rigidBody->GetPosition();
        const UMath::Vector3& velocity = rigidBody->GetLinearVelocity();
        char sheet[sizeof(HeliSheetCoordinate) / sizeof(uint32_t) * 9 + 1];
        DescribeWords(&mHeliSheetCoord, sizeof(HeliSheetCoordinate) / sizeof(uint32_t), sheet, sizeof(sheet));

        Log::Info("AIVehicleHelicopter 0x%p  gHeliVehicle[0x%08X]=0x%p mIOwner[+0x34]=0x%p IRigidBody=0x%p mISimpleChopper[+0x8B0]=0x%p", this,
                  kgHeliVehicle, Game::Global<AIVehicleHelicopter*>(kgHeliVehicle), mIOwner, rigidBody, mISimpleChopper);
        Log::Info("  pos=(%.2f, %.2f, %.2f) vel=(%.2f, %.2f, %.2f) GetSpeedXZ=%.2f smoothedDt=%.5f frames=%.3f", position.x, position.y, position.z,
                  velocity.x, velocity.y, velocity.z, rigidBody->GetSpeedXZ(), gSmoothed, SmoothedFrames());
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
    HeliSheet::LogLive(full);
    if (full) SimpleChopper::LogLive(mISimpleChopper);
}
#endif

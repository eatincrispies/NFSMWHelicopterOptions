#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
    static_assert(offsetof(AIVehicleHelicopter, mHeight) == 0x7D0, "AIVehicleHelicopter::mHeight");
    static_assert(offsetof(AIVehicleHelicopter, mStrafeToDest) == 0x7D4, "AIVehicleHelicopter::mStrafeToDest");
    static_assert(offsetof(AIVehicleHelicopter, mHeliFuelTimeRemaining) == 0x7D8, "AIVehicleHelicopter::mHeliFuelTimeRemaining");
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
        if (!gCounter) Log::Warn("No high-resolution timer is available; the helicopter's motion is smoothed as if at 60 FPS.");
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
            Log::Info("Helicopter updates arrive %.0f times per second.", gFrames / gElapsed);
        }
    }

    float SmoothedFrames() {
        return gSmoothed > 0.0f ? gSmoothed / kReferenceStep : 1.0f;
    }

    bool IsFinite(const UMath::Vector3& v) {
        return Memory::IsFinite(v.x) && Memory::IsFinite(v.y) && Memory::IsFinite(v.z);
    }

}

bool HeliVehicleActive() {
    return Game::Global<AIVehicleHelicopter*>(kgHeliVehicle) != nullptr;
}

void AIVehicleHelicopter::Refresh() {
    sBrake.stoppingRatio = sSettings.StoppingRatio;
    sBrake.speedScale    = sSettings.BrakeSpeedScale;
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
            Log::Warn("The helicopter's fuel could not be read this frame.");
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

    HeliSheet::Update(position, AIActionHeliPursuit::Find(rigidBody));
    SimpleChopper::ApplySpeedCap(mISimpleChopper);
    if (newHelicopter) return;

    SimpleChopper::ScaleMotionFilters(SmoothedFrames());
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

    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    if (playerRigidBody != nullptr) {
        const UMath::Vector3 playerPosition = playerRigidBody->GetPosition();
        Log::Info("Helicopter spawned %.0f m from you and %.0f m above you, with %.0f s of fuel.", UMath::Distancexz(position, playerPosition),
                  position.y - playerPosition.y, mHeliFuelTimeRemaining);
    } else {
        Log::Info("Helicopter spawned with %.0f s of fuel.", mHeliFuelTimeRemaining);
    }

    gLastFuel = mHeliFuelTimeRemaining;
    if (sSettings.FuelTime > 0.0f) {
        mHeliFuelTimeRemaining = sSettings.FuelTime;
        gLastFuel = sSettings.FuelTime;
        Log::Info("Fuel set to %.0f s by [Helicopter:FuelTime].", sSettings.FuelTime);
    }
    if (sSettings.LineOfSight > 0.0f) Log::Info("Line of sight set to %.0f m by [Helicopter:LineOfSight].", sSettings.LineOfSight);
}

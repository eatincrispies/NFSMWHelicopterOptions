#include <cstdint>
#include "HeliSheet.hpp"
#include "AIActionHeliPursuit.hpp"

namespace {

    constexpr uintptr_t kbIgnoreHeliSheet     = 0x0090D621u;
    constexpr uintptr_t kNeverIgnoreHeliSheet = 0x008EB1F4u;
    constexpr float     kReturnFraction       = 0.8f;

    bool gFarFromTarget = false;
    bool gIgnoring = false;

#if defined(_DEBUG)
    bool gLoggedIgnore = false;
    bool gLoggedReady = false;
#endif

    bool& bIgnoreHeliSheet() {
        return Game::Global<bool>(kbIgnoreHeliSheet);
    }

    bool NeverIgnoreHeliSheet() {
        return Game::Global<bool>(kNeverIgnoreHeliSheet);
    }

    bool GameIgnoresDuringSkidHit(const AIActionHeliPursuit* pursuit) {
        return pursuit != nullptr && pursuit->IsSkidHitting() && !NeverIgnoreHeliSheet();
    }

    bool IsOutOfRange(const UMath::Vector3& heliPosition) {
        const float ignoreDistance = HeliSheet::sSettings.IgnoreHeliSheetDistance;
        IRigidBody* playerRigidBody = ignoreDistance > 0.0f ? GetLocalPlayerRigidBody() : nullptr;
        if (playerRigidBody == nullptr) return false;

        const float distance = UMath::Distancexz(heliPosition, playerRigidBody->GetPosition());
        const float limit = gFarFromTarget ? ignoreDistance * kReturnFraction : ignoreDistance;
        const bool  outOfRange = distance > limit;
        if (outOfRange != gFarFromTarget)
            Log::Info("HeliSheet  dXZ=%.2f %s %.2f  IgnoreHeliSheetDistance=%g return=%g  far %d -> %d", distance, outOfRange ? ">" : "<=", limit,
                      ignoreDistance, ignoreDistance * kReturnFraction, gFarFromTarget, outOfRange);
        return outOfRange;
    }

}

void HeliSheet::BeginHelicopter() {
    gFarFromTarget = false;
}

void HeliSheet::Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit) {
    gFarFromTarget = IsOutOfRange(heliPosition);

    const bool ignore = !sSettings.HeliSheet || gFarFromTarget;
    if (ignore || gIgnoring) {
        bIgnoreHeliSheet() = ignore || GameIgnoresDuringSkidHit(pursuit);
        gIgnoring = ignore;
    }
}

#if defined(_DEBUG)
void HeliSheet::LogLive(bool full) {
    const bool ignore = bIgnoreHeliSheet();
    if (!gLoggedReady || ignore != gLoggedIgnore) {
        Log::Info("HeliSheet  bIgnoreHeliSheet[0x%08X] %d -> %d  NeverIgnoreHeliSheet[0x%08X]=%d far=%d forced=%d", kbIgnoreHeliSheet, gLoggedIgnore,
                  ignore, kNeverIgnoreHeliSheet, NeverIgnoreHeliSheet(), gFarFromTarget, gIgnoring);
        gLoggedIgnore = ignore;
        gLoggedReady = true;
    }
    if (full)
        Log::Info("HeliSheet  bIgnoreHeliSheet[0x%08X]=%d NeverIgnoreHeliSheet[0x%08X]=%d  HeliSheet=%d IgnoreHeliSheetDistance=%g far=%d forced=%d",
                  kbIgnoreHeliSheet, ignore, kNeverIgnoreHeliSheet, NeverIgnoreHeliSheet(), sSettings.HeliSheet, sSettings.IgnoreHeliSheetDistance,
                  gFarFromTarget, gIgnoring);
}
#endif
